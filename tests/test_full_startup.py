#!/usr/bin/env python3
"""Optional local SQL fixture: 159 TCP tags and the formerly crashing EMAIL[7].

The HTML template is intentionally empty: the real email sender must return
before opening SMTP. In-memory email delivery is exercised separately by the
state test, whose transport is replaced at link time.
"""
import os
import importlib.util
import json
import pathlib
import shutil
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time

binary = pathlib.Path(sys.argv[1]).resolve(strict=True)
client = os.environ.get("MARIADB_CLIENT") or shutil.which("mariadb") or shutil.which("mysql")
assert client, "MariaDB client required for the optional startup fixture"
viewer_path = pathlib.Path(__file__).resolve().parents[1] / 'tools' / 'automonitorctl.py'
spec = importlib.util.spec_from_file_location('monitor_client', viewer_path)
viewer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(viewer)
stop = threading.Event()

def receive(connection, length):
    data = b""
    while len(data) < length and not stop.is_set():
        try:
            block = connection.recv(length - len(data))
        except socket.timeout:
            continue
        if not block:
            return None
        data += block
    return data if len(data) == length else None

def serve(listener):
    while not stop.is_set():
        try:
            connection, _ = listener.accept()
        except socket.timeout:
            continue
        except OSError:
            return
        with connection:
            connection.settimeout(0.1)
            try:
                while not stop.is_set():
                    header = receive(connection, 7)
                    if header is None:
                        break
                    transaction, protocol, length, unit = struct.unpack(">HHHB", header)
                    request = receive(connection, length - 1)
                    if request is None:
                        break
                    function, address, count = struct.unpack(">BHH", request)
                    assert function == 3 and 0 < count <= 20 and address >= 0
                    payload = bytes((function, count * 2)) + struct.pack(">H", 280) * count
                    connection.sendall(struct.pack(">HHHB", transaction, protocol, len(payload) + 1, unit) + payload)
            except (ConnectionError, OSError):
                pass

with socket.socket() as listener, socket.socket() as unused_smtp, tempfile.TemporaryDirectory(prefix="driver-full-startup-") as directory:
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    listener.settimeout(0.1)
    port = listener.getsockname()[1]
    unused_smtp.bind(("127.0.0.1", 0))  # Reserved, not listening; no SMTP recipient exists.
    fixture = pathlib.Path(__file__).with_name("fixtures") / "startup_alarm.sql"
    setup = fixture.read_text()
    setup += "INSERT INTO qrytablacom (id,registro,tag,puerto) VALUES " + ",".join(
        f"({i + 1},{i},'r{i}',{port})" for i in range(159)
    ) + ";\n"
    subprocess.run([client, "--no-defaults", "--host=127.0.0.1", "--port=33077", "-u", "root"], input=setup, text=True, check=True)
    base = pathlib.Path(directory)
    (base / "run").mkdir()
    (base / "html").mkdir()
    (base / "html" / "plantilla_emails.html").write_text("")
    (base / "configdb.cfg").write_text(f"""[DATABASE]
db_direccion=127.0.0.1
db_puerto=33077
db_nombre=driver_startup_fixture
db_usuario=root
db_clave=
nombre_concentrador=startup-test
habilitar_acceso_externo=0
periodo_muestreo_tbejecutar=1
periodo_muestreo_tbcambios=1
periodo_revision_alarmas=0.1
smtp_servidor=smtp://127.0.0.1:{unused_smtp.getsockname()[1]}
smtp_usuario=
smtp_pass=
""")
    peer = threading.Thread(target=serve, args=(listener,), daemon=True)
    peer.start()
    process = None
    output_path = base / "output.log"
    try:
        with output_path.open("w") as output:
            process = subprocess.Popen([str(binary)], cwd=base / "run", stdout=output, stderr=subprocess.STDOUT)
            until = time.monotonic() + 15
            while time.monotonic() < until and process.poll() is None:
                text = output_path.read_text()
                if "ERROR: HTML vacío" in text:
                    break
                time.sleep(0.02)
            assert process.poll() is None, output_path.read_text()
            assert "ERROR: HTML vacío" in output_path.read_text(), "EMAIL[7] was not reached"
            monitor_path = base / '.runtime' / 'automonitor.sock'
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                snapshot = viewer.fetch(monitor_path)
                if snapshot['devices'] and snapshot['devices'][0]['status'] == 'ONLINE' and snapshot['sql']['status'] == 'OK':
                    break
                time.sleep(.1)
            assert snapshot['pid'] == process.pid and snapshot['phase'] == 'EJECUTANDO', snapshot
            assert len(snapshot['devices']) == 1, snapshot
            device = snapshot['devices'][0]
            assert device['name'] == 'meter' and device['status'] == 'ONLINE', device
            assert len(device['tags']) == 159 and device['fresh_tags'] == 159, device
            assert all(float(tag['value']) == 280 and tag['period_s'] == .1 for tag in device['tags']), device
            assert snapshot['sql']['status'] == 'OK' and snapshot['sql']['operations_ok'] > 0, snapshot['sql']
            assert snapshot['alarms_active'] == 1 and snapshot['alarms_unknown'] == 0, snapshot
            assert any(event['component'] == 'CORREO' for event in snapshot['events']), snapshot['events']
            text_view = subprocess.check_output([sys.executable, str(viewer_path), '--socket', str(monitor_path), '--once', '--device', 'meter'], text=True)
            assert 'r158 = 280.000000 V' in text_view and 'periodo 0.1s' in text_view, text_view
            # Concurrent viewers only inspect snapshots; acquisition continues after every client exits.
            clients = [subprocess.Popen([sys.executable, str(viewer_path), '--socket', str(monitor_path), '--json'], stdout=subprocess.PIPE) for _ in range(3)]
            for child in clients:
                data, _ = child.communicate(timeout=4)
                assert child.returncode == 0 and json.loads(data)['pid'] == process.pid
            assert process.poll() is None
            generation = snapshot['generation']
            # Reload through the existing configuration mechanism, retaining the monitor endpoint.
            subprocess.run([client, '--no-defaults', '--host=127.0.0.1', '--port=33077', '-u', 'root',
                            '-e', 'UPDATE driver_startup_fixture.tbcambios SET cont_tbregistros=cont_tbregistros+1'], check=True)
            deadline = time.monotonic() + 12
            while time.monotonic() < deadline and process.poll() is None:
                snapshot = viewer.fetch(monitor_path)
                if snapshot['generation'] > generation and snapshot['phase'] == 'EJECUTANDO' and snapshot['devices'][0]['status'] == 'ONLINE':
                    break
                time.sleep(.1)
            assert snapshot['generation'] > generation and snapshot['devices'][0]['status'] == 'ONLINE', snapshot
            process.send_signal(signal.SIGTERM)
            assert process.wait(timeout=10) == 0, output_path.read_text()
            assert not monitor_path.exists(), 'socket must be removed on shutdown'
        text = output_path.read_text()
        assert any(line.split() == ["Tags", "Modbus", "TCP", "159"] for line in text.splitlines())
        assert "[ OK ] Trabajadores iniciados." in text
        assert "[ METRICAS ] Totales desde el inicio" in text
        assert "METRICS reads=" not in text
        assert "METRICS reads=" in (base / "log" / "devlog.log").read_text()
        assert "FOREIGN KEY" not in text, text
        assert "AddressSanitizer" not in text and "runtime error:" not in text, text
        print("PASS full startup: 159 TCP tags, live monitor/CLI, concurrent viewers, catalog reload, EMAIL[7] and controlled shutdown; no SMTP delivery")
    finally:
        if process is not None and process.poll() is None:
            process.send_signal(signal.SIGTERM)
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        stop.set()
        peer.join(timeout=2)
