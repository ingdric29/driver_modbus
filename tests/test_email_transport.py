#!/usr/bin/env python3
"""Exercise actual libcurl sender over loopback TLS, never external SMTP."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from smtp_fixture import Server, certificate

BINARY = Path(sys.argv.pop(1)).resolve(strict=True)
TEMPLATE = Path(__file__).resolve().parents[1] / 'html' / 'plantilla_emails.html'


class TransportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='driver-smtp-')
        cls.base = Path(cls.temp.name)
        cls.cert, cls.key = certificate(cls.base)
        (cls.base / 'run').mkdir()
        (cls.base / 'html').mkdir()
        (cls.base / 'html' / 'plantilla_emails.html').write_bytes(TEMPLATE.read_bytes())

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def send(self, url, trusted=True, ca_file=None):
        result = subprocess.run([str(BINARY), url, str(ca_file or self.cert) if trusted else '-'], cwd=self.base / 'run',
                                capture_output=True, text=True, timeout=8)
        output = result.stdout + result.stderr
        self.assertNotIn('fixture-secret', output)
        self.assertNotIn('fixture-user', output)
        self.assertNotIn('runtime error:', output)
        self.assertNotIn('AddressSanitizer', output)
        return result.returncode, output

    def test_smtps_and_mandatory_starttls_large_payload(self):
        for implicit in (True, False):
            server = Server(self.cert, self.key, implicit=implicit)
            try:
                result, output = self.send('{}://127.0.0.1:{}'.format('smtps' if implicit else 'smtp', server.port))
                self.assertEqual(result, 0, output)
                self.assertIn('aceptado por el servidor', output)
                self.assertIn('alarma=42 dispositivo=meter directorio=7', output)
                self.assertIn('smtp=250', output)
                self.assertIn(b'AUTH', server.commands)
                if not implicit:
                    self.assertLess(server.commands.index(b'STARTTLS'), server.commands.index(b'AUTH'))
                self.assertEqual(len(server.messages), 1)
                self.assertIn(b'X' * 120000 + b'END_LARGE_DESCRIPTION', server.messages[0])
                self.assertIn(b'Content-Transfer-Encoding: 8bit', server.messages[0])
                self.assertNotIn(b'Cc: ', server.messages[0])
                self.assertRegex(server.messages[0].decode(), r'Date: [A-Z][a-z]{2}, .* \+0000')
            finally:
                server.close()

    def test_original_defaults_allow_slow_smtps_without_added_deadlines(self):
        server = Server(self.cert, self.key, tls_delay=4)
        try:
            result, output = self.send('smtps://127.0.0.1:{}'.format(server.port), trusted=False)
            self.assertEqual(result, 0, output)
            self.assertIn('modo=SMTPS_ORIGINAL limites=libcurl_original', output)
            self.assertIn('aceptado por el servidor', output)
            self.assertEqual(len(server.messages), 1)
        finally:
            server.close()

    def test_no_plaintext_fallback_when_starttls_unavailable(self):
        server = Server(self.cert, self.key, implicit=False, offer_starttls=False)
        try:
            result, output = self.send('smtp://127.0.0.1:{}'.format(server.port))
            self.assertEqual(result, 64, output)
            self.assertNotIn(b'AUTH', server.commands)
            self.assertFalse(server.messages)
        finally:
            server.close()

    def test_authentication_failure_is_not_success(self):
        server = Server(self.cert, self.key, reject_auth=True)
        try:
            result, output = self.send('smtps://127.0.0.1:{}'.format(server.port))
            self.assertEqual(result, 67, output)
            self.assertIn('etapa=AUTH', output)
            self.assertIn('smtp=535', output)
            self.assertFalse(server.messages)
        finally:
            server.close()

    def test_original_peer_policy_and_explicit_ca_override(self):
        # No CA override reproduces the original peer-chain setting; host checking stays on.
        server = Server(self.cert, self.key)
        try:
            result, output = self.send('smtps://127.0.0.1:{}'.format(server.port), trusted=False)
            self.assertEqual(result, 0, output)
            self.assertEqual(len(server.messages), 1)
        finally:
            server.close()
        with tempfile.TemporaryDirectory() as directory:
            unrelated_ca, _ = certificate(directory)
            server = Server(self.cert, self.key)
            try:
                result, output = self.send('smtps://127.0.0.1:{}'.format(server.port), ca_file=unrelated_ca)
                self.assertEqual(result, 60, output)
                self.assertNotIn(b'AUTH', server.commands)
                self.assertFalse(server.messages)
            finally:
                server.close()

    def test_server_rejection_after_data_is_not_success(self):
        server = Server(self.cert, self.key, reject_data=True)
        try:
            result, output = self.send('smtps://127.0.0.1:{}'.format(server.port))
            self.assertNotEqual(result, 0, output)
            self.assertIn('smtp=554', output)
            self.assertNotIn('aceptado por el servidor', output)
        finally:
            server.close()


if __name__ == '__main__':
    unittest.main()
