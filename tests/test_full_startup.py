#!/usr/bin/env python3
"""Optional local SQL fixture: 159 TCP tags and the formerly crashing EMAIL[7].

The HTML template is intentionally empty: the real email sender must return
before opening SMTP. In-memory email delivery is exercised separately by the
state test, whose transport is replaced at link time.
"""
import os
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
            process.send_signal(signal.SIGTERM)
            assert process.wait(timeout=10) == 0, output_path.read_text()
        text = output_path.read_text()
        assert "numero tags mbtcp:159" in text
        assert "FOREIGN KEY" not in text, text
        assert "AddressSanitizer" not in text and "runtime error:" not in text, text
        print("PASS full startup: 159 TCP tags, EMAIL[7], valid alarm history and controlled shutdown; no SMTP delivery")
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
