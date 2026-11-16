#!/usr/bin/env python3
# Archivo: test_full_email.py
# Descripción: Comprueba recorrido completo de alarmas y correo con SQL y SMTP locales.
# Necesita: Intérprete y dependencias del runner; consultar tests/README.md para el perfil SQL reservado.
# Retornos: 0 al completar las comprobaciones; un error o aserción hace fallar la prueba.

"""Real process: config -> SQL alarms/directory -> libcurl TLS/AUTH/DATA; loopback only."""
import base64
from contextlib import ExitStack
import os
from pathlib import Path
import shutil
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
from smtp_fixture import Server, certificate

BINARY = Path(sys.argv[1]).resolve(strict=True)
CLIENT = os.environ.get('MARIADB_CLIENT') or shutil.which('mariadb') or shutil.which('mysql')
assert CLIENT, 'Local test MariaDB client is required'
ROOT = Path(__file__).resolve().parents[1]
STOP = threading.Event()


def receive(connection, length):
    data = b''
    while len(data) < length and not STOP.is_set():
        try:
            block = connection.recv(length - len(data))
        except socket.timeout:
            continue
        if not block:
            return None
        data += block
    return data if len(data) == length else None


def modbus_peer(listener):
    while not STOP.is_set():
        try:
            connection, _ = listener.accept()
        except socket.timeout:
            continue
        except OSError:
            return
        with connection:
            connection.settimeout(.1)
            try:
                while not STOP.is_set():
                    header = receive(connection, 7)
                    if header is None:
                        break
                    transaction, protocol, length, unit = struct.unpack('>HHHB', header)
                    request = receive(connection, length - 1)
                    if request is None:
                        break
                    function, address, count = struct.unpack('>BHH', request)
                    assert function == 3 and 0 < count <= 20 and address >= 0
                    payload = bytes((function, count * 2)) + struct.pack('>H', 280) * count
                    connection.sendall(struct.pack('>HHHB', transaction, protocol, len(payload) + 1, unit) + payload)
            except (ConnectionError, OSError):
                pass


with tempfile.TemporaryDirectory(prefix='driver-full-email-') as temporary, socket.socket() as listener, ExitStack() as resources:
    base = Path(temporary)
    listener.bind(('127.0.0.1', 0))
    listener.listen(2)
    listener.settimeout(.1)
    port = listener.getsockname()[1]
    cert, key = certificate(base)
    username, password = 'full-process@example.invalid', 'test-only-Fixture-AUTH-%!'
    smtp = Server(cert, key, expected_auth=(username, password))
    resources.callback(smtp.close)
    setup = (ROOT / 'tests' / 'fixtures' / 'startup_alarm.sql').read_text().replace('driver_startup_fixture', 'driver_email_fixture')
    setup += 'INSERT INTO qrytablacom (id,registro,tag,puerto) VALUES ' + ','.join(
        "({},{},'r{}',{})".format(i + 1, i, i, port) for i in range(159)) + ';\n'
    setup += "INSERT INTO tbdirectorio VALUES(30,'sender@example.invalid','alarm30@example.invalid','','test','');\n"
    setup += 'DELETE FROM tbdefalarmas;\n'
    for i in range(3):
        setup += "INSERT INTO tbdefalarmas VALUES({},'$(meter.r{})>250','$(meter.r{})<240','alarm{}','EMAIL[7];EMAIL[30];','EMAIL[7];EMAIL[30];','meter','r{}',1,'r{}');\n".format(52+i, i, i, i, i, i)
    subprocess.run([CLIENT, '--no-defaults', '--host=127.0.0.1', '--port=33077', '-u', 'root'], input=setup, text=True, check=True)
    (base / 'run').mkdir()
    (base / 'html').mkdir()
    shutil.copyfile(ROOT / 'html' / 'plantilla_emails.html', base / 'html' / 'plantilla_emails.html')
    (base / 'configdb.cfg').write_text('''[DATABASE]
db_direccion=127.0.0.1
db_puerto=33077
db_nombre=driver_email_fixture
db_usuario=root
db_clave=
nombre_concentrador=startup-test
habilitar_acceso_externo=0
periodo_muestreo_tbejecutar=1
periodo_muestreo_tbcambios=1
periodo_revision_alarmas=0.1
smtp_from=sender@example.invalid
smtp_usuario={}
smtp_pass={}
smtp_servidor=smtps://127.0.0.1:{}
'''.format(username, password, smtp.port))
    peer = threading.Thread(target=modbus_peer, args=(listener,), daemon=True)
    peer.start()
    process = None
    output_path = base / 'output.log'
    try:
        with output_path.open('w') as output:
            process = subprocess.Popen([str(BINARY)], cwd=base / 'run', stdout=output, stderr=subprocess.STDOUT)
            deadline = time.monotonic() + 20
            while time.monotonic() < deadline and process.poll() is None and len(smtp.messages) < 6:
                time.sleep(.05)
            assert process.poll() is None, output_path.read_text()
            assert len(smtp.messages) == 6, output_path.read_text()
            assert len(smtp.auth_payloads) == 6
            for token in smtp.auth_payloads:
                assert base64.b64decode(token).split(b'\0') == [b'', username.encode(), password.encode()]
            expected_recipients = {b'RCPT TO:<alarm7@example.invalid>': 3, b'RCPT TO:<alarm30@example.invalid>': 3}
            for recipient, count in expected_recipients.items():
                assert smtp.envelopes.count(recipient) == count, smtp.envelopes
            # Several alarm evaluations later, accepted messages must not repeat.
            time.sleep(.5)
            assert len(smtp.messages) == 6
            process.send_signal(signal.SIGTERM)
            assert process.wait(timeout=10) == 0, output_path.read_text()
        logs = output_path.read_text()
        assert logs.count('aceptado por el servidor') == 6, logs
        assert 'smtp=535' not in logs and 'FOREIGN KEY' not in logs, logs
        assert password not in logs and username not in logs
        assert any(line.split() == ["Tags", "Modbus", "TCP", "159"] for line in logs.splitlines())
        print('PASS full email: real config, SQL/directory, 3 alarms/2 recipients, verified SASL values, 6 accepted messages, no duplicates; loopback only')
    finally:
        if process is not None and process.poll() is None:
            process.send_signal(signal.SIGTERM)
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        STOP.set()
        peer.join(timeout=2)
