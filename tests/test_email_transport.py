#!/usr/bin/env python3
# Archivo: test_email_transport.py
# Descripción: Comprueba transporte SMTP real contra servidores de prueba locales.
# Necesita: Intérprete y dependencias del runner; consultar tests/README.md para el perfil SQL reservado.
# Retornos: 0 al completar las comprobaciones; un error o aserción hace fallar la prueba.

"""Compare the real original/current senders over loopback SMTP; no external mail."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest
from smtp_fixture import Server, certificate

BINARY = Path(sys.argv.pop(1)).resolve(strict=True)
ORIGINAL = Path(sys.argv.pop(1)).resolve(strict=True)
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

    def send(self, url, payload='large', binary=BINARY):
        result = subprocess.run([str(binary), url, payload], cwd=self.base / 'run',
                                capture_output=True, text=True, timeout=10)
        output = result.stdout + result.stderr
        if binary == BINARY:
            self.assertNotIn('fixture-secret', output)
            self.assertNotIn('fixture-user', output)
        self.assertNotIn('runtime error:', output)
        self.assertNotIn('AddressSanitizer', output)
        return result.returncode, output

    def test_actual_original_and_current_senders_match(self):
        for payload in ('small', 'cc'):
            server = Server(self.cert, self.key)
            try:
                url = 'smtps://127.0.0.1:{}'.format(server.port)
                for binary in (ORIGINAL, BINARY):
                    result, output = self.send(url, payload=payload, binary=binary)
                    self.assertEqual(result, 0, output)
                self.assertEqual(len(server.messages), 2)
                # Message-ID is intentionally fresh per call; every other byte must match.
                messages = [re.sub(rb'^Message-ID:[^\r]*\r\n', b'Message-ID: <normalized>\r\n', m, flags=re.M)
                            for m in server.messages]
                self.assertEqual(messages[0], messages[1])
                self.assertEqual(len(server.auth_payloads), 2)
                self.assertEqual(server.auth_payloads[0], server.auth_payloads[1])
                n = len(server.envelopes) // 2
                self.assertEqual(server.envelopes[:n], server.envelopes[n:])
            finally:
                server.close()

    def test_smtps_large_payload_uses_original_message_format(self):
        server = Server(self.cert, self.key)
        try:
            result, output = self.send('smtps://127.0.0.1:{}'.format(server.port))
            self.assertEqual(result, 0, output)
            self.assertIn('modo=EMISOR_ORIGINAL', output)
            self.assertIn('aceptado por el servidor', output)
            self.assertIn('alarma=42 dispositivo=meter directorio=7', output)
            self.assertIn('smtp=250', output)
            self.assertEqual(len(server.messages), 1)
            self.assertIn(b'X' * 120000 + b'END_LARGE_DESCRIPTION', server.messages[0])
            self.assertIn(b'Content-Transfer-Encoding: 7bit', server.messages[0])
            self.assertIn(b'Cc:  (Usuario)\r\n', server.messages[0])
            self.assertIn(b'Date: Vie, 13 Nov 2026 13:00:00\r\n', server.messages[0])
        finally:
            server.close()

    def test_original_defaults_allow_slow_smtps(self):
        server = Server(self.cert, self.key, tls_delay=4)
        try:
            result, output = self.send('smtps://127.0.0.1:{}'.format(server.port))
            self.assertEqual(result, 0, output)
            self.assertIn('limites=libcurl_original', output)
            self.assertEqual(len(server.messages), 1)
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
