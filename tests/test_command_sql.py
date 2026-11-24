#!/usr/bin/env python3
"""Driver real: SQL -> FC06 + alarmas CMD/EMAIL; solo servidores temporales loopback."""
import os
import json
from pathlib import Path
import signal
import socket
import statistics
import re
import struct
import subprocess
import sys
import tempfile
import threading
import time

from smtp_fixture import Server, certificate

ROOT = Path(__file__).resolve().parents[1]
BINARY = Path(sys.argv[1]).resolve(strict=True)
EXTERNAL = '--external' in sys.argv[2:]
RECOVERY_PROBE = '--recovery-probe' in sys.argv[2:]


class ModbusPeer:
    def __init__(self, writes=None):
        self.listener = socket.socket()
        self.listener.bind(('127.0.0.1', 0))
        self.listener.listen(4)
        self.listener.settimeout(.1)
        self.port = self.listener.getsockname()[1]
        self.stop = threading.Event()
        self.high = threading.Event()
        self.writes = [] if writes is None else writes
        self.errors = []
        self.reads = 0
        self.connections = 0
        self.drop_addresses = set()
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
                self.connections += 1
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
                            if address in self.drop_addresses:
                                continue  # PLC received the request, but its response is lost.
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
    peer = peer_other = smtp = driver = output = None

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
        peer_other = ModbusPeer(peer.writes)
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
 (9000,7,77,NULL,1,'startup-test','alarm','meter'),
 (13,6,999,NULL,1,'startup-test','Manual','meter');
UPDATE tbdefalarmas SET accionactivar='CMD[9000];EMAIL[7];';
ALTER TABLE tbcmd MODIFY lote VARCHAR(32) CHARACTER SET latin1 COLLATE latin1_swedish_ci;
ALTER TABLE tbejecutar MODIFY lote VARCHAR(64) CHARACTER SET latin1 COLLATE latin1_swedish_ci;
"""
        setup += "INSERT INTO tbcmd VALUES " + ','.join(
            f"({1000+i},{100+i},{i},NULL,1,'startup-test','bulk','meter')"
            for i in range(130)) + ';\n'
        if RECOVERY_PROBE:
            setup += "DELETE FROM tbdefalarmas; DELETE FROM tbcmd; "
            setup += "INSERT INTO tbcmd VALUES "
            setup += "(26,16404,20,'2024-03-18 00:00:00',1,'startup-test','Ajuste','meter'),"
            setup += "(56,16503,78,'2024-03-18 00:00:00',1,'startup-test','Ajuste','meter'),"
            setup += "(57,16504,85,'2024-03-18 00:00:00',1,'startup-test','Ajuste','meter');"
            peer.drop_addresses.add(16404)
        sql(setup)
        if EXTERNAL:
            sql(setup.replace('driver_command_fixture', 'driver_external_command_fixture'))
            sql("USE driver_external_command_fixture; "
                "UPDATE tbcmd SET registro=8,valor=88 WHERE id=9000; "
                "UPDATE tbcmd SET registro=6,valor=654 WHERE id=10")
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
habilitar_acceso_externo={int(EXTERNAL)}
dbext_direccion=127.0.0.1
dbext_puerto={port}
dbext_nombre=driver_external_command_fixture
dbext_usuario=root
dbext_clave=
periodo_muestreo_tbejecutar=0.2
periodoext_muestreo_tbejecutar=0.37
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

        def monitor_device():
            with socket.socket(socket.AF_UNIX) as client:
                client.settimeout(2)
                client.connect(str(base / '.runtime/automonitor.sock'))
                data = b''
                while True:
                    block = client.recv(65536)
                    if not block: break
                    data += block
            return json.loads(data)['devices'][0]

        if RECOVERY_PROBE:
            sql(prefix + "UPDATE tbejecutar SET ejecutar_tbcmd=1,lote='Ajuste' WHERE id=1")
            until(lambda: 'batch="Ajuste" loaded=3 matched=3 status=failed' in logs())
            print('RECOVERY observed: ID26 sends=' + str(peer.count(16404)) +
                  ', ID56 sends=' + str(peer.count(16503)) +
                  ', ID57 sends=' + str(peer.count(16504)), flush=True)
            assert peer.count(16404) == 3
            assert peer.count(16503) == 1 and peer.count(16504) == 1, (
                'Commands after timeout were never transmitted; affected log lines:\n' +
                '\n'.join(line for line in logs().splitlines() if 'COMMAND ' in line))
            assert sql(prefix + "SELECT cmd_datetime FROM tbcmd WHERE id=26") == '2024-03-18 00:00:00'
            assert sql(prefix + "SELECT COUNT(*) FROM tbcmd WHERE id IN (56,57) AND cmd_datetime>'2024-03-18'") == '2'
            time.sleep(.3)
            assert peer.count(16404) == 3 and peer.count(16503) == 1 and peer.count(16504) == 1
            assert 'command=26 modbus=failed ack=skipped attempts=3' in logs()
            print('PASS same-link recovery: failed ID26 remains unconfirmed, IDs56/57 each sent once and confirmed; 2024 timestamps retained only for the failed command')
            before_exception = monitor_device()
            assert before_exception['write_failures'] == 1
            assert before_exception['reconnections'] >= 1
            assert before_exception['failures'] == 0
            assert all(t['consecutive_failures'] == 0 for t in before_exception['tags'])
            connections_before = peer.connections
            sql(prefix + "INSERT INTO tbcmd VALUES "
                "(3000,9,1,'2024-03-18 00:00:00',1,'startup-test','exception','meter'),"
                "(3001,61,7,'2024-03-18 00:00:00',1,'startup-test','exception','meter'); "
                "UPDATE tbejecutar SET ejecutar_tbcmd=1,lote='exception' WHERE id=1")
            until(lambda: 'batch="exception" loaded=2 matched=2 status=failed' in logs())
            assert peer.count(9) == 1 and peer.count(61) == 1
            # Viewer snapshots are cached for one second; wait for the new sample.
            until(lambda: monitor_device()['write_failures'] == before_exception['write_failures'] + 1)
            after_exception = monitor_device()
            assert peer.connections == connections_before
            assert after_exception['reconnections'] == before_exception['reconnections']
            assert after_exception['write_failures'] == before_exception['write_failures'] + 1
            assert after_exception['failures'] == before_exception['failures']
            assert 'command=3000 modbus=failed ack=skipped attempts=1' in logs()
            assert sql(prefix + "SELECT cmd_datetime FROM tbcmd WHERE id=3000") == '2024-03-18 00:00:00'
            assert sql(prefix + "SELECT cmd_datetime>'2024-03-18' FROM tbcmd WHERE id=3001") == '1'
            assert 'delivery=exception' in logs() and 'error_text=' in logs()
            print('PASS exception reporting and same-link continuation without replaying the failed command')
            # Entirely unavailable link: share one 500ms cooldown budget, then keep
            # progressing to a healthy independent device instead of waiting per ID.
            peer.close()
            sql(prefix + f"INSERT INTO tbdispositivos VALUES (3,'other-valid','127.0.0.1',{peer_other.port},1,'',9600,'N',1,'0','','','MBTCP',1,'startup-test'); "
                "INSERT INTO tbcmd VALUES " + ','.join(
                    f"({1000+i},{16600+i},1,'2024-03-18 00:00:00',1,'startup-test','offline','meter')"
                    for i in range(24)) +
                ",(2000,60,99,'2024-03-18 00:00:00',1,'startup-test','offline','other-valid');")
            submitted = time.monotonic()
            sql(prefix + "UPDATE tbejecutar SET ejecutar_tbcmd=1,lote='offline' WHERE id=1")
            until(lambda: 'batch="offline" loaded=25 matched=25 status=failed' in logs())
            assert time.monotonic() - submitted < 5
            assert peer.count(60) == 1
            assert sql(prefix + "SELECT cmd_datetime>'2024-03-18' FROM tbcmd WHERE id=2000") == '1'
            assert sql(prefix + "SELECT COUNT(*) FROM tbcmd WHERE lote='offline' AND id<2000 AND cmd_datetime='2024-03-18 00:00:00'") == '24'
            summary = next(line for line in logs().splitlines() if 'batch="offline"' in line)
            assert int(re.search(r'recovery_wait_us=(\d+)', summary).group(1)) <= 500000
            assert 'delivery=not_sent' in logs()
            assert 'delivery=unknown' in logs()
            assert 'fallo de conexion: Resource temporarily unavailable' not in (base / 'driver.log').read_text()
            print('PASS bounded offline recovery: 24 unavailable commands share the budget; independent device succeeds; unsuccessful 2024 timestamps stay unchanged')
            # A fresh request will encounter the offline link's backoff. Stop while
            # it is pending: the recovery wait must remain interruptible.
            sql(prefix + "UPDATE tbejecutar SET ejecutar_tbcmd=1,lote='offline' WHERE id=1")
            until(lambda: sql(prefix + 'SELECT ejecutar_tbcmd FROM tbejecutar WHERE id=1') == '0')
            stopping = time.monotonic()
            driver.terminate()
            assert driver.wait(timeout=5) == 0
            assert time.monotonic() - stopping < 2
            assert peer.count(60) == 1
            print('PASS shutdown interrupts link recovery without executing later commands')
            sys.exit(0)  # finally closes every temporary resource.

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
        assert peer.count(4) == 0 and peer.count(5) == 0 and peer.count(6) == 0
        assert all(item[1] == 321 for item in peer.writes if item[0] == 3)

        # More than 128 entries, ordered and complete; the alarm CMD is last in the full catalog.
        sql(prefix + "UPDATE tbejecutar SET ejecutar_tbcmd=1,lote='bulk' WHERE id=1")
        until(lambda: 'batch="bulk" loaded=130 matched=130 status=accepted' in logs())
        bulk = [(address, value) for address, value, _ in peer.writes if 100 <= address < 230]
        assert bulk == [(100+i, i) for i in range(130)]
        assert sql(prefix + 'SELECT COUNT(*) FROM tbcmd WHERE lote=\'bulk\' AND cmd_datetime IS NOT NULL') == '130'

        # Prepared, case-sensitive batch selection; no matching command means no traffic.
        for batch in ("missing", "manual' OR 1=1 --"):
            escaped = batch.replace("'", "''")
            sql(prefix + f"UPDATE tbejecutar SET ejecutar_tbcmd=1,lote='{escaped}' WHERE id=1")
            until(lambda: f'batch="{batch}" loaded=0 matched=0 status=empty' in logs())
        assert peer.count(6) == 0

        # Preserve legacy NULL/empty batches and Latin-1 lot names used by the real schema.
        sql(prefix + "INSERT INTO tbcmd VALUES "
            "(14,30,1,NULL,1,'startup-test',NULL,'meter'),"
            "(15,31,2,NULL,1,'startup-test','','meter'),"
            "(16,32,3,NULL,1,'startup-test','pruéba','meter'); "
            "UPDATE tbejecutar SET ejecutar_tbcmd=1,lote=NULL WHERE id=1")
        until(lambda: 'batch="" loaded=2 matched=2 status=accepted' in logs())
        sql(prefix + "UPDATE tbejecutar SET ejecutar_tbcmd=1,lote='pruéba' WHERE id=1")
        until(lambda: 'batch="pruéba" loaded=1 matched=1 status=accepted' in logs())
        assert [(address, value) for address, value, _ in peer.writes if 30 <= address <= 32] == [
            (30, 1), (31, 2), (32, 3)]

        # Reject the whole batch before its first valid command if any entry is invalid.
        bad_values = [("-1", "1"), ("65536", "1"), ("NULL", "1"),
                      ("41", "NULL"), ("41", "65536"), ("41", "-32769"),
                      ("41", "2147483647"), ("41", "-2147483648")]
        for index, (register, value) in enumerate(bad_values):
            batch = f'invalid-{index}'
            first_id = 4000 + index * 2
            before = len(peer.writes)
            sql(prefix + "INSERT INTO tbcmd VALUES "
                f"({first_id},40,123,NULL,1,'startup-test','{batch}','meter'),"
                f"({first_id+1},{register},{value},NULL,1,'startup-test','{batch}','meter'); "
                f"UPDATE tbejecutar SET ejecutar_tbcmd=1,lote='{batch}' WHERE id=1")
            until(lambda: f'batch="{batch}" loaded=2 matched=0 status=rejected' in logs())
            assert len(peer.writes) == before
            assert sql(prefix + f"SELECT COUNT(*) FROM tbcmd WHERE lote='{batch}' AND cmd_datetime IS NOT NULL") == '0'
        # Device enable is checked from the fresh SQL batch too.
        sql(prefix + f"INSERT INTO tbdispositivos VALUES (2,'disabled','127.0.0.1',{peer.port},1,'',9600,'N',1,'0','','','MBTCP',0,'startup-test'); "
            "INSERT INTO tbcmd VALUES(4900,42,1,NULL,1,'startup-test','disabled-device','disabled'); "
            "UPDATE tbejecutar SET ejecutar_tbcmd=1,lote='disabled-device' WHERE id=1")
        until(lambda: 'batch="disabled-device" loaded=1 matched=0 status=skipped' in logs())
        assert peer.count(40) == 0 and peer.count(41) == 0 and peer.count(42) == 0

        # Signed INT16 and unsigned WORD encode explicitly, including the last address.
        sql(prefix + "INSERT INTO tbcmd VALUES "
            "(50000,65535,-32768,NULL,1,'startup-test','boundaries','meter'),"
            "(50001,65535,65535,NULL,1,'startup-test','boundaries','meter'); "
            "UPDATE tbejecutar SET ejecutar_tbcmd=1,lote='boundaries' WHERE id=1")
        until(lambda: 'batch="boundaries" loaded=2 matched=2 status=accepted' in logs())
        assert [value for address, value, _ in peer.writes if address == 65535] == [32768, 65535]

        # Bounded query must reject the entire oversized batch, never execute a prefix.
        sql(prefix + "INSERT INTO tbcmd SELECT 20000+seq,300,1,NULL,1,'startup-test','oversized','meter' FROM seq_1_to_4097; "
            "UPDATE tbejecutar SET ejecutar_tbcmd=1,lote='oversized' WHERE id=1")
        until(lambda: 'batch="oversized" loaded=-1 matched=0 status=failed' in logs())
        assert peer.count(300) == 0
        sql(prefix + "DELETE FROM tbcmd WHERE lote='oversized'")

        # Definitive Modbus exception: one attempt, no SQL acceptance, email continues.
        sql(prefix + "UPDATE tbejecutar SET ejecutar_tbcmd=1,lote='rejected' WHERE id=1")
        until(lambda: 'command=20 modbus=failed ack=skipped attempts=1' in logs())
        assert peer.count(9) == 1
        assert sql(prefix + 'SELECT cmd_datetime IS NULL FROM tbcmd WHERE id=20') == '1'
        peer.high.clear()
        until(lambda: len(smtp.messages) == 2)  # Deactivation email after failed Modbus order.

        # Validate independently by device; skip disabled members even with NULL fields.
        sql(prefix + f"INSERT INTO tbdispositivos VALUES (3,'other-valid','127.0.0.1',{peer_other.port},1,'',9600,'N',1,'0','','','MBTCP',1,'startup-test'); "
            "INSERT INTO tbcmd VALUES "
            "(51000,50,1,NULL,1,'startup-test','mixed','meter'),"
            "(51001,51,2,NULL,1,'startup-test','mixed','other-valid'),"
            "(51002,NULL,NULL,NULL,1,'startup-test','mixed','disabled'),"
            "(51003,65536,3,NULL,1,'startup-test','mixed','meter'),"
            "(51004,53,4,NULL,1,'startup-test','mixed','other-valid'); "
            "UPDATE tbejecutar SET ejecutar_tbcmd=1,lote='mixed' WHERE id=1")
        until(lambda: 'batch="mixed" loaded=5 matched=2 status=partial skipped=1 rejected=2' in logs())
        assert [(address, value) for address, value, _ in peer.writes if 50 <= address <= 53] == [(51, 2), (53, 4)]
        assert sql(prefix + "SELECT COUNT(*) FROM tbcmd WHERE lote='mixed' AND cmd_datetime IS NOT NULL") == '2'
        sql(prefix + "INSERT INTO tbcmd VALUES "
            "(52000,54,1,NULL,1,'startup-test','interleaved','meter'),"
            "(52001,55,2,NULL,1,'startup-test','interleaved','other-valid'),"
            "(52002,56,3,NULL,1,'startup-test','interleaved','meter'); "
            "UPDATE tbejecutar SET ejecutar_tbcmd=1,lote='interleaved' WHERE id=1")
        until(lambda: 'batch="interleaved" loaded=3 matched=3 status=accepted' in logs())
        assert [(address, value) for address, value, _ in peer.writes if 54 <= address <= 56] == [(54, 1), (55, 2), (56, 3)]
        # A disabled group must not block a valid command, matching the real Ajuste incident.
        sql(prefix + "INSERT INTO tbcmd VALUES "
            "(52500,58,52,NULL,1,'startup-test','adjustment','meter'),"
            "(52501,59,300,NULL,1,'startup-test','adjustment','disabled'); "
            "UPDATE tbejecutar SET ejecutar_tbcmd=1,lote='adjustment' WHERE id=1")
        until(lambda: 'batch="adjustment" loaded=2 matched=1 status=accepted skipped=1 rejected=0' in logs())
        assert peer.count(58) == 1 and peer.count(59) == 0
        sql(prefix + "INSERT INTO tbcmd VALUES "
            "(53000,9,1,NULL,1,'startup-test','transport-failure','meter'),"
            "(53001,57,2,NULL,1,'startup-test','transport-failure','other-valid'); "
            "UPDATE tbejecutar SET ejecutar_tbcmd=1,lote='transport-failure' WHERE id=1")
        until(lambda: 'batch="transport-failure" loaded=2 matched=2 status=failed' in logs())
        assert peer.count(9) == 2 and peer.count(57) == 1, (peer.count(9), peer.count(57))

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

        if EXTERNAL:
            before = peer.count(6)
            sql("USE driver_external_command_fixture; "
                "UPDATE tbejecutar SET ejecutar_tbcmd=1,lote='manual' WHERE id=1")
            until(lambda: 'source=externa request=1 command=10 modbus=accepted ack=ok' in logs())
            assert peer.count(6) == before + 1
            assert [value for address, value, _ in peer.writes if address == 6] == [654]
            assert sql('USE driver_external_command_fixture; '
                       'SELECT cmd_datetime IS NOT NULL FROM tbcmd WHERE id=10') == '1'
        # External command IDs must not replace local CMD definitions (same ID, different register).
        peer.high.set()
        until(lambda: len(smtp.messages) == 3)
        assert peer.count(7) == 2 and peer.count(8) == 0
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
        assert 'batch="manual" loaded=1 matched=1 status=accepted' in text
        assert 'COMMAND_METRICS units=us window_limit=256' in text
        assert 'local.ack_sql={n=146,fail=1' in text
        assert 'local.poll_lag=' in text
        if EXTERNAL:
            assert int(re.search(r'externa\.poll_sql=\{n=(\d+)', text).group(1)) >= 3
            assert 'externa.ack_sql={n=1,fail=0' in text
        else:
            assert 'externa.poll_sql=' not in text
        assert 'fixture-secret' not in text and 'fixture-user' not in text
        print('PASS SQL -> FC06: order/filtering, single-attempt exceptions, SQL ack failure without replay; '
              'alarm CMD + 3 real TLS/SMTP messages (activate/deactivate/reactivate), '
              'acquisition and shutdown; loopback only')
        print('PASS device groups: disabled omitted, invalid group isolated, global ID order and other devices survive transport errors')
        print('PASS private batches: 130 ordered commands, exact/prepared filter, 4097 rejected, local alarm catalog isolated')
        print('PASS command scheduler: external=' + str(EXTERNAL) + ', periods=200/370ms')
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
        if peer_other is not None:
            peer_other.close()
        if smtp is not None:
            smtp.close()
        db.terminate()
        db.wait(timeout=15)
        db_log.close()
        if output is not None:
            output.close()
