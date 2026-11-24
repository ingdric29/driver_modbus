#!/usr/bin/env python3
# Archivo: test_monitor_client.py
# Descripción: Comprueba visor, navegación y restauración del terminal con un monitor simulado.
# Necesita: Intérprete y dependencias del runner; consultar tests/README.md para el perfil SQL reservado.
# Retornos: 0 al completar las comprobaciones; un error o aserción hace fallar la prueba.

"""Pure rendering and real PTY/socket tests; never starts an acquisition process."""
import copy
import fcntl
import importlib.util
import json
import os
from pathlib import Path
import pty
import select
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import termios
import threading
import time
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / 'tools' / 'automonitorctl.py'
spec = importlib.util.spec_from_file_location('automonitorctl', SCRIPT)
client = importlib.util.module_from_spec(spec)
spec.loader.exec_module(client)

TAG = dict(key=1, name='rVoltaje', address=0, period_s=.1, unit='V', status='ACTUAL', value='230',
           acquired_at=100, age_ms=50, result=1, reads=50, failures=2, consecutive_failures=0, latency_us=250)
DEVICE = dict(key='meter1', name='meter', protocol='MBTCP', endpoint='127.0.0.1:502', slave=1,
              status='ONLINE', fresh_tags=1, reads=50, failures=2, tags=[TAG])
SNAPSHOT = dict(version=1, pid=1234, generated_at=100, uptime_s=100, generation=1, concentrator='test',
                phase='EJECUTANDO', alarms_active=0, alarms_unknown=1,
                sql=dict(status='OK', age_ms=100, endpoint='tcp://127.0.0.1:3306', schema='fixture'),
                metrics=dict(reads=50, pending_windows=0, stored_samples=20), devices=[DEVICE],
                events=[dict(time=100, level='ERROR', component='CORREO', message='SMTP timeout', repeats=2)])


class Fixture:
    def __init__(self, directory):
        self.path = str(Path(directory) / 'monitor.sock')
        self.listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.listener.bind(self.path)
        self.listener.listen(8)
        self.listener.settimeout(.1)
        self.stop = threading.Event()
        self.requests = 0
        self.thread = threading.Thread(target=self.serve)
        self.thread.start()

    def serve(self):
        while not self.stop.is_set():
            try:
                connection, _ = self.listener.accept()
            except socket.timeout:
                continue
            except OSError:
                return
            with connection:
                try:
                    connection.sendall(json.dumps(SNAPSHOT).encode())
                    self.requests += 1
                except OSError:
                    pass

    def close(self):
        self.stop.set()
        self.listener.close()
        self.thread.join(timeout=2)


class MonitorTests(unittest.TestCase):
    def test_controls_unicode_and_plain_output(self):
        self.assertNotIn('\x1b', client.safe('\x1b[31m\nvalue\u202e'))
        self.assertEqual(client.clipped('abc界x', 5), 'abc界')
        self.assertIn('periodo 0.1s', client.plain(SNAPSHOT, 'meter'))
        self.assertIn('SMTP timeout (x2)', client.plain(SNAPSHOT))
        self.assertIn('ONLINE', client.compact_device(DEVICE))
        counters = dict(DEVICE, deferred_reads=15, write_failures=1, reconnections=2)
        self.assertIn('E=2 P=15 W=1 R=2', client.compact_device(counters))
        self.assertIn('errores lectura 2 | pospuestas 15 | errores escritura 1 | reconexiones 2', client.device_line(counters))
        self.assertIn('ACTUAL', client.compact_tag(TAG))
        with self.assertRaises(ValueError):
            client.find_device([DEVICE, DEVICE], 'meter')

    def test_navigation_and_disconnect_marks_snapshot_obsolete(self):
        view = client.View('/missing')
        view.snapshot = copy.deepcopy(SNAPSHOT)
        other = copy.deepcopy(DEVICE)
        other['key'], other['name'] = 'meter2', 'other'
        view.snapshot['devices'].append(other)
        view.move(1)
        self.assertEqual(view.selected_key, 'meter2')
        view.details = True
        view.move(100)
        self.assertEqual(view.tag_selected, 0)
        view.refresh()
        self.assertIsNotNone(view.error)
        self.assertIsNotNone(view.snapshot)
        self.assertIsNotNone(view.error_since)

    def test_drawing_marks_old_values_and_statuses_without_connection(self):
        import curses
        class Screen:
            def __init__(self):
                self.lines = {}
            def erase(self):
                self.lines = {}
            def getmaxyx(self):
                return (24, 100)
            def addstr(self, row, col, text, style):
                self.lines[row] = text
            def refresh(self):
                pass
        screen = Screen()
        view = client.View('/missing')
        view.snapshot = copy.deepcopy(SNAPSHOT)
        view.draw(screen, curses)
        self.assertIn('ONLINE', '\n'.join(screen.lines.values()))
        view.error = 'driver terminado'
        view.draw(screen, curses)
        output = '\n'.join(screen.lines.values())
        self.assertIn('DATOS SIN VIGENCIA', output)
        self.assertNotIn('ONLINE', output)
        view.details = True
        view.draw(screen, curses)
        output = '\n'.join(screen.lines.values())
        self.assertNotIn('ACTUAL', output)
        self.assertIn('SIN_VIGENCIA', output)
        self.assertIn('230 V', output)  # Retains the old value, explicitly invalidated.

    def test_cli_and_interactive_terminal(self):
        with tempfile.TemporaryDirectory(prefix='monitor-view-') as directory:
            fixture = Fixture(directory)
            master = slave = process = None
            try:
                output = subprocess.check_output([sys.executable, str(SCRIPT), '--socket', fixture.path, '--once', '--device', 'meter'], text=True)
                self.assertIn('rVoltaje = 230 V', output)
                result = subprocess.check_output([sys.executable, str(SCRIPT), '--socket', fixture.path, '--json'], text=True)
                self.assertEqual(json.loads(result)['pid'], 1234)
                master, slave = pty.openpty()
                original = termios.tcgetattr(slave)
                fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', 24, 100, 0, 0))
                process = subprocess.Popen([sys.executable, str(SCRIPT), '--socket', fixture.path],
                                           stdin=slave, stdout=slave, stderr=slave, env=dict(os.environ, TERM='xterm'))
                deadline = time.monotonic() + 5
                screen = b''
                while time.monotonic() < deadline and b'DISPOSITIVOS' not in screen:
                    if select.select([master], [], [], .2)[0]:
                        screen += os.read(master, 65536)
                self.assertIn(b'DISPOSITIVOS', screen)
                os.write(master, b'\r')
                deadline = time.monotonic() + 3
                while time.monotonic() < deadline and b'rVoltaje' not in screen:
                    if select.select([master], [], [], .2)[0]:
                        screen += os.read(master, 65536)
                self.assertIn(b'rVoltaje', screen)
                # Resize down and back, force refresh, then close only the viewer.
                for rows, cols in ((10, 40), (30, 100)):
                    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', rows, cols, 0, 0))
                    process.send_signal(signal.SIGWINCH)
                    time.sleep(.15)
                os.write(master, b'r\x1bq')
                self.assertEqual(process.wait(timeout=4), 0)
                self.assertEqual(termios.tcgetattr(slave), original)
                self.assertEqual(client.fetch(fixture.path)['pid'], 1234)  # Q did not stop the source.
                self.assertGreaterEqual(fixture.requests, 4)
            finally:
                if process is not None and process.poll() is None:
                    process.kill()
                    process.wait()
                if master is not None:
                    os.close(master)
                if slave is not None:
                    os.close(slave)
                fixture.close()


if __name__ == '__main__':
    unittest.main()
