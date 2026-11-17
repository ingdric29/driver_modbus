"""Run the actual driver with local S7 and Modbus servers, never SQL."""
import json
import os
import pathlib
import signal
import socket
import subprocess
import sys
import tempfile
import time
import unittest

BINARY = pathlib.Path(sys.argv.pop(1)).resolve()
FIXTURE = pathlib.Path(sys.argv.pop(1)).resolve()


def snapshot(path):
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as connection:
        connection.settimeout(1)
        connection.connect(str(path))
        chunks = []
        while True:
            chunk = connection.recv(65536)
            if not chunk:
                return json.loads(b"".join(chunks))
            chunks.append(chunk)


class LocalTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="local-driver-")
        self.addCleanup(self.directory.cleanup)
        self.base = pathlib.Path(self.directory.name)
        (self.base / "bin").mkdir()
        self.config = self.base / "local.ini"
        self.server = subprocess.Popen([str(FIXTURE)], stdin=subprocess.PIPE,
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.addCleanup(self.stop_server)
        self.s7_port, self.mb_port, self.second_port = map(int, self.server.stdout.readline().split())
        self.socket = self.base / ".runtime/automonitor.sock"

    def stop_server(self):
        if self.server.poll() is None:
            self.server.communicate("q", timeout=5)
        else:
            self.server.communicate()

    def write_config(self, extra=""):
        text = f"""[device:plc]
protocol=SIEMENS_S7
host=127.0.0.1
port={self.s7_port}
timeout_ms=100
[device:meter]
protocol=MBTCP
host=127.0.0.1
port={self.mb_port}
timeout_ms=100
[device:plc2]
protocol=SIEMENS_S7
host=127.0.0.1
port={self.second_port}
timeout_ms=100
[tag:plc2:live]
address=DB100.DBB0
type=BYTE
period=0.05
[tag:meter:voltage]
address=0
type=word
period=0.05
"""
        for name, address, kind in [("bool", "DB100.DBX0.0", "BOOL"),
                                     ("byte", "DB100.DBB1", "BYTE"),
                                     ("sint", "DB100.DBB1", "SINT"),
                                     ("int", "DB100.DBW2", "INT"),
                                     ("word", "DB100.DBW2", "WORD"),
                                     ("dint", "DB100.DBD4", "DINT"),
                                     ("real", "DB100.DBD8", "REAL"),
                                     ("dword", "DB100.DBD12", "DWORD")]:
            text += f"[tag:plc:{name}]\naddress={address}\ntype={kind}\nperiod=0.05\n"
            if name == "real":
                text += "scale=2\n"
        self.config.write_text(text + extra)

    def start_driver(self):
        self.log = open(self.base / "driver.log", "w+")
        self.addCleanup(self.log.close)
        self.driver = subprocess.Popen([str(BINARY), "--local-config", str(self.config)],
                                       cwd=self.base / "bin", stdout=self.log, stderr=self.log)
        self.addCleanup(self.stop_driver)

    def stop_driver(self):
        if self.driver.poll() is None:
            self.driver.terminate()
            self.driver.wait(timeout=6)

    def wait_for(self, condition, timeout=5):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            self.assertIsNone(self.driver.poll(), (self.base / "driver.log").read_text())
            try:
                data = snapshot(self.socket)
                if condition(data):
                    return data
            except (OSError, ValueError):
                pass
            time.sleep(0.03)
        self.fail("Expected monitor state not reached: " + (self.base / "driver.log").read_text())

    def test_values_coexistence_failure_and_shutdown(self):
        self.write_config()
        self.start_driver()
        data = self.wait_for(lambda s: len(s["devices"]) == 3 and
                             all(d["status"] == "ONLINE" for d in s["devices"]))
        devices = {d["name"]: d for d in data["devices"]}
        values = {t["name"]: float(t["value"]) for t in devices["plc"]["tags"]}
        self.assertEqual(values, {"bool": 1, "byte": 255, "sint": -1, "int": -123,
                                  "word": 65413, "dint": -456, "real": 50, "dword": 4294967295})
        self.assertEqual(float(devices["meter"]["tags"][0]["value"]), 1234)
        self.assertEqual(data["sql"]["operations_ok"], 0)
        self.assertEqual(data["sql"]["operations_failed"], 0)
        self.server.stdin.write("x\n"); self.server.stdin.flush()
        failed = self.wait_for(lambda s: any(d["name"] == "plc" and d["status"] != "ONLINE"
                                            for d in s["devices"]))
        previous = next(d["reads"] for d in failed["devices"] if d["name"] == "meter")
        second = next(d["reads"] for d in failed["devices"] if d["name"] == "plc2")
        self.wait_for(lambda s: any(d["name"] == "meter" and d["reads"] > previous + 2
                                   and d["status"] == "ONLINE" for d in s["devices"]))
        self.wait_for(lambda s: any(d["name"] == "plc2" and d["reads"] > second + 2
                                   and d["status"] == "ONLINE" for d in s["devices"]))
        self.server.stdin.write("s\n"); self.server.stdin.flush()
        self.wait_for(lambda s: all(d["status"] == "ONLINE" for d in s["devices"]))
        self.driver.terminate()
        self.assertEqual(self.driver.wait(timeout=6), 0)
        self.assertFalse(self.socket.exists())

    def test_invalid_configuration_before_network(self):
        self.write_config()
        valid = self.config.read_text()
        for replacement in [valid.replace("DB100.DBD8", "DB100.DBW8"),
                            valid.replace("period=0.05", "period=0"),
                            valid + "[tag:missing:bad]\naddress=DB1.DBB0\ntype=BYTE\n",
                            valid.replace("type=REAL", "type=REAL\nunknown=1")]:
            self.config.write_text(replacement)
            result = subprocess.run([str(BINARY), "--check-local-config", str(self.config)],
                                    cwd=self.base / "bin", capture_output=True, text=True, timeout=3)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(self.socket.exists())
        self.config.write_text(valid)
        subprocess.run([str(BINARY), "--check-local-config", str(self.config)],
                       cwd=self.base / "bin", check=True, capture_output=True, timeout=3)

    def test_batching_periods_reload_and_partial_errors(self):
        self.write_config("""[tag:plc:slow]
address=DB100.DBB16
type=BYTE
period=0.25
[tag:plc:edge]
address=DB100.DBB4095
type=BYTE
period=0.05
[tag:plc:missing]
address=DB100.DBB4096
type=BYTE
period=0.05
""")
        original = self.config.read_text()
        self.start_driver()
        def tags(s):
            return {t['name']: t for d in s['devices'] if d['name'] == 'plc' for t in d['tags']}
        data = self.wait_for(lambda s: len(tags(s)) == 11 and tags(s)['real']['reads'] >= 12
                             and len(s.get('s7_links', [])) == 2)
        values = tags(data)
        self.assertEqual(values['edge']['status'], 'ACTUAL')
        self.assertEqual(float(values['edge']['value']), 255)
        self.assertEqual(values['missing']['status'], 'ERROR')
        self.assertIsNone(values['missing']['value'])
        self.assertIn('Snap7 rc=', values['missing']['diagnostic'])
        self.assertEqual(values['real']['source_address'], 'DB100.DBD8')
        self.assertGreater(values['real']['reads'], values['slow']['reads'] * 2)
        link = next(l for l in data['s7_links'] if l['device'] == 'plc')
        self.assertEqual(link['connections'], 1)  # Remote address failures keep the session.
        self.assertGreaterEqual(link['pdu'], 240)
        self.assertLess(link['dbread_calls'], sum(t['reads'] for t in values.values()))
        generation = data['generation']
        self.config.write_text(original.replace('scale=2', 'scale=4'))
        os.kill(self.driver.pid, signal.SIGHUP)
        changed = self.wait_for(lambda s: s['generation'] == generation + 1
                                and tags(s).get('real', {}).get('value') == '100')
        # Invalid reload must preserve generation and the current acquisition.
        self.config.write_text(original.replace('DB100.DBD8', 'DB100.DBW8'))
        os.kill(self.driver.pid, signal.SIGHUP)
        rejected = self.wait_for(lambda s: any('Recarga rechazada' in e['message'] for e in s['events']))
        self.assertEqual(rejected['generation'], changed['generation'])
        self.assertEqual(tags(rejected)['real']['value'], '100')
        reads = tags(rejected)['real']['reads']
        self.wait_for(lambda s: tags(s)['real']['reads'] > reads + 2)
        # Apply a second valid generation removing a PLC without a second process.
        self.config.write_text(original.replace('[device:plc2]', '[device:plc2]\nenabled=0'))
        os.kill(self.driver.pid, signal.SIGHUP)
        removed = self.wait_for(lambda s: s['generation'] == generation + 2 and len(s['devices']) == 2)
        self.assertEqual(removed['pid'], self.driver.pid)
        second = subprocess.run([str(BINARY), '--local-config', str(self.config)],
                                cwd=self.base / 'bin', capture_output=True, text=True, timeout=3)
        self.assertNotEqual(second.returncode, 0)
        self.assertIsNone(self.driver.poll())
        started = time.monotonic()
        self.driver.terminate()
        self.assertEqual(self.driver.wait(timeout=3), 0)
        self.assertLess(time.monotonic() - started, 2)
        self.assertFalse(self.socket.exists())
        viewer = subprocess.run([sys.executable, str(BINARY.parent / 'automonitorctl'),
                                 '--socket', str(self.socket), '--once'],
                                capture_output=True, text=True, timeout=3)
        self.assertNotEqual(viewer.returncode, 0)

    def test_large_grouped_reads(self):
        self.config.write_text(f"[device:plc]\nprotocol=SIEMENS_S7\nhost=127.0.0.1\nport={self.s7_port}\n"
                               + ''.join(f"[tag:plc:b{i}]\naddress=DB100.DBB{i}\ntype=BYTE\nperiod=1\n"
                                         for i in range(1000, 1600)))
        self.start_driver()
        data = self.wait_for(lambda s: len(s['devices']) == 1
                             and len(s['devices'][0]['tags']) == 600
                             and all(t['status'] == 'ACTUAL' for t in s['devices'][0]['tags'])
                             and len(s.get('s7_links', [])) == 1)
        for tag in data['devices'][0]['tags']:
            self.assertEqual(int(tag['value']), int(tag['name'][1:]) % 256)
        self.assertLessEqual(data['s7_links'][0]['dbread_calls'], 6)
        self.assertEqual(data['s7_links'][0]['connections'], 1)


if __name__ == "__main__":
    unittest.main()
