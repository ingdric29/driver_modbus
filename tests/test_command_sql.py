#!/usr/bin/env python3
"""Driver real: SQL -> FC06 + alarmas CMD/EMAIL; solo servidores temporales loopback."""
import os
from pathlib import Path
import signal
import socket
import statistics
import struct
import subprocess
import sys
import tempfile
import threading
import time

from smtp_fixture import Server, certificate

ROOT = Path(__file__).resolve().parents[1]
BINARY = Path(sys.argv[1]).resolve(strict=True)


class ModbusPeer:
    def __init__(self):
        self.listener = socket.socket()
        self.listener.bind(('127.0.0.1', 0))
        self.listener.listen(4)
        self.listener.settimeout(.1)
        self.port = self.listener.getsockname()[1]
        self.stop = threading.Event()
        self.high = threading.Event()
        self.writes = []
        self.errors = []
        self.reads = 0
        self.thread = threading.Thread(target=self.serve)
        self.thread.start()

    def receive(self, connection, count):
        data = b''
        while len(data) < count and not self.stop.is_set():
            try:
                block = connection.recv(count - len(data))
            except socket.timeout:
                continue
            if not block:
                return None
            data += block
        return data if len(data) == count else None

    def serve(self):
        while not self.stop.is_set():
            try:
                connection, _ = self.listener.accept()
            except socket.timeout:
                continue
            except OSError:
                break
            try:
                with connection:
                    connection.settimeout(.1)
                    while not self.stop.is_set():
                        header = self.receive(connection, 7)
                        if header is None:
                            break
                        transaction, protocol, length, unit = struct.unpack('>HHHB', header)
                        request = self.receive(connection, length - 1)
                        if request is None:
                            break
                        function, address, value = struct.unpack('>BHH', request)
                        assert protocol == 0 and unit == 1
                        if function == 3:
                            self.reads += 1
                            payload = bytes((3, value * 2)) + struct.pack(
                                '>H', 280 if self.high.is_set() else 220) * value
                        else:
                            assert function == 6
                            self.writes.append((address, value, time.monotonic()))
                            payload = bytes((0x86, 2)) if address == 9 else request
                        connection.sendall(struct.pack(
                            '>HHHB', transaction, 0, len(payload) + 1, unit) + payload)
            except (OSError, ConnectionError):
                pass
            except Exception as error:
                self.errors.append(error)

    def count(self, address):
        return sum(item[0] == address for item in self.writes)

    def close(self):
        self.stop.set()
        self.listener.close()
        self.thread.join(timeout=3)
        assert not self.thread.is_alive()
        assert not self.errors, self.errors


with tempfile.TemporaryDirectory(prefix='driver-command-sql-') as temporary:
    base = Path(temporary)
    sysroot = Path(os.environ.get('COMMAND_TEST_SYSROOT',
                                  os.environ.get('S7_TEST_SYSROOT', '/usr')))
    env = os.environ.copy()
    env['PATH'] = str(sysroot / 'bin') + ':' + env['PATH']
    with socket.socket() as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    subprocess.run([str(sysroot / 'bin/mariadb-install-db'), '--no-defaults',
                    '--basedir=' + str(sysroot), '--datadir=' + str(base / 'data'),
                    '--skip-test-db', '--auth-root-authentication-method=normal'],
                   env=env, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    db_log = (base / 'database.log').open('w')
    db = subprocess.Popen([str(sysroot / 'sbin/mariadbd'), '--no-defaults',
                           '--basedir=' + str(sysroot), '--datadir=' + str(base / 'data'),
                           '--socket=' + str(base / 'db.sock'), '--pid-file=' + str(base / 'db.pid'),
                           '--bind-address=127.0.0.1', '--port=' + str(port),
                           '--innodb-buffer-pool-size=32M'],
                          env=env, stdout=db_log, stderr=db_log)
    peer = smtp = driver = output = None

    def sql(query):
        return subprocess.check_output([
            str(sysroot / 'bin/mariadb'), '--no-defaults', '-h127.0.0.1', '-P' + str(port),
            '-uroot', '-N', '-e', query], env=env, text=True, stderr=subprocess.PIPE).strip()

    def until(check, seconds=12):
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            if driver is not None and driver.poll() is not None:
                raise AssertionError((base / 'driver.log').read_text())
            try:
                if check():
                    return
            except (subprocess.CalledProcessError, OSError):
                pass
            time.sleep(.02)
        logs = (base / 'driver.log').read_text() if (base / 'driver.log').exists() else ''
        raise AssertionError('Timeout\n' + logs)

    prefix = 'USE driver_command_fixture; '
    try:
        until(lambda: sql('SELECT 1') == '1')
        peer = ModbusPeer()
        cert, key = certificate(base)
        smtp = Server(cert, key, expected_auth=('fixture-user', 'fixture-secret'))
        setup = (ROOT / 'tests/fixtures/startup_alarm.sql').read_text().replace(
            'driver_startup_fixture', 'driver_command_fixture')
        setup += f"""
ALTER TABLE tbejecutar ADD ejecutar_tbcmd INT, ADD tiempo_tbcmd DATETIME,
 ADD tiempo_segundos BIGINT, ADD lote TEXT;
INSERT INTO tbejecutar VALUES(1,'startup-test',0,NULL,0,'manual');
UPDATE tbdispositivos SET puerto={peer.port};
INSERT INTO qrytablacom (id,registro,tag,puerto) VALUES (1,0,'r0',{peer.port});
INSERT INTO tbcmd VALUES
 (10,3,321,NULL,1,'startup-test','manual','meter'),
 (11,4,999,NULL,0,'startup-test','manual','meter'),
 (12,5,888,NULL,1,'startup-test','other','meter'),
 (20,9,444,NULL,1,'startup-test','rejected','meter'),
 (90,7,77,NULL,1,'startup-test','alarm','meter');
UPDATE tbdefalarmas SET accionactivar='CMD[90];EMAIL[7];';
"""
        sql(setup)
        (base / 'run').mkdir()
        (base / 'html').mkdir()
        (base / 'html/plantilla_emails.html').write_bytes(
            (ROOT / 'html/plantilla_emails.html').read_bytes())
        (base / 'configdb.cfg').write_text(f'''[DATABASE]
db_direccion=127.0.0.1
db_puerto={port}
db_nombre=driver_command_fixture
db_usuario=root
db_clave=
nombre_concentrador=startup-test
habilitar_acceso_externo=0
periodo_muestreo_tbejecutar=0.2
periodoext_muestreo_tbejecutar=0.2
periodo_muestreo_tbcambios=1
periodo_revision_alarmas=0.1
smtp_from=sender@example.invalid
smtp_usuario=fixture-user
smtp_pass=fixture-secret
smtp_servidor=smtps://127.0.0.1:{smtp.port}
''')
        output = (base / 'driver.log').open('w')
        driver = subprocess.Popen([str(BINARY)], cwd=base / 'run', env=env,
                                  stdout=output, stderr=output)
        until(lambda: peer.reads >= 2)
        log_path = base / 'log/devlog.log'

        def logs():
            return log_path.read_text() if log_path.exists() else ''

        latencies = []
        for attempt in range(3):
            before = peer.count(3)
            sql(prefix + "UPDATE tbcmd SET cmd_datetime=NULL WHERE id=10; "
                "UPDATE tbejecutar SET ejecutar_tbcmd=1,lote='manual' WHERE id=1;")
            committed = time.monotonic()
            if attempt == 0:
                peer.high.set()  # Alarm CMD/EMAIL while the SQL order is pending.
            until(lambda: peer.count(3) == before + 1)
            observed = [item[2] for item in peer.writes if item[0] == 3][-1]
            latencies.append(max(0, observed - committed))
            until(lambda: sql(prefix +
                             'SELECT cmd_datetime IS NOT NULL FROM tbcmd WHERE id=10') == '1')
        until(lambda: len(smtp.messages) == 1)
        assert peer.count(7) == 1  # Alarm CMD[id] stayed available after manual catalog reloads.
        assert peer.count(4) == 0 and peer.count(5) == 0
        assert all(item[1] == 321 for item in peer.writes if item[0] == 3)

        # Remote Modbus exception: three legacy attempts; no SQL acceptance.
        sql(prefix + "UPDATE tbejecutar SET ejecutar_tbcmd=1,lote='rejected' WHERE id=1")
        until(lambda: 'command=20 modbus=failed ack=skipped attempts=3' in logs())
        assert peer.count(9) == 3
        assert sql(prefix + 'SELECT cmd_datetime IS NULL FROM tbcmd WHERE id=20') == '1'
        peer.high.clear()
        until(lambda: len(smtp.messages) == 2)  # Deactivation email after failed Modbus order.

        # PLC accepted; SQL confirmation failed. Observe it without resending the order.
        sql(prefix + """
DELIMITER $$
CREATE TRIGGER reject_ack BEFORE UPDATE ON tbcmd FOR EACH ROW
BEGIN IF NEW.id=10 THEN SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='fixture ack failure'; END IF; END$$
DELIMITER ;
""")
        before = peer.count(3)
        sql(prefix + "UPDATE tbejecutar SET ejecutar_tbcmd=1,lote='manual' WHERE id=1")
        until(lambda: 'command=10 modbus=accepted ack=failed attempts=1' in logs())
        assert peer.count(3) == before + 1
        time.sleep(.4)
        assert peer.count(3) == before + 1
        sql(prefix + 'DROP TRIGGER reject_ack')

        # Another activation must still send CMD and email after the SQL failure.
        peer.high.set()
        until(lambda: len(smtp.messages) == 3)
        assert peer.count(7) == 2
        time.sleep(.3)
        assert len(smtp.messages) == 3
        assert smtp.envelopes.count(b'RCPT TO:<alarm7@example.invalid>') == 3
        assert peer.reads > 5
        shutdown = time.monotonic()
        driver.send_signal(signal.SIGTERM)
        assert driver.wait(timeout=10) == 0
        assert time.monotonic() - shutdown < 3
        text = logs()
        assert 'source=local request=1 command=10 modbus=accepted ack=ok attempts=1' in text
        assert 'batch="manual" loaded=5 matched=1 status=accepted' in text
        assert 'COMMAND_METRICS units=us window_limit=256' in text
        assert 'local.ack_sql={n=4,fail=1' in text
        assert 'fixture-secret' not in text and 'fixture-user' not in text
        print('PASS SQL -> FC06: order/filtering, 3 retries, SQL ack failure without replay; '
              'alarm CMD + 3 real TLS/SMTP messages (activate/deactivate/reactivate), '
              'acquisition and shutdown; loopback only')
        print('MEASURE commit SQL -> simulated PLC: samples_ms=' +
              ','.join(f'{value * 1000:.1f}' for value in latencies) +
              f' median_ms={statistics.median(latencies) * 1000:.1f}')
    finally:
        if driver is not None and driver.poll() is None:
            driver.terminate()
            try:
                driver.wait(timeout=10)
            except subprocess.TimeoutExpired:
                driver.kill()
                driver.wait()
        if peer is not None:
            peer.close()
        if smtp is not None:
            smtp.close()
        db.terminate()
        db.wait(timeout=15)
        db_log.close()
        if output is not None:
            output.close()
