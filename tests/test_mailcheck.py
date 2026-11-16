#!/usr/bin/env python3
# Archivo: test_mailcheck.py
# Descripción: Comprueba diagnóstico SMTP/TLS sin autenticación ni envío real.
# Necesita: Intérprete y dependencias del runner; consultar tests/README.md para el perfil SQL reservado.
# Retornos: 0 al completar las comprobaciones; un error o aserción hace fallar la prueba.

import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from smtp_fixture import Server, certificate

SCRIPT = Path(__file__).resolve().parents[1] / 'tools' / 'automonitor-mailcheck.py'
spec = importlib.util.spec_from_file_location('mailcheck', SCRIPT)
check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check)


class MailcheckTests(unittest.TestCase):
    def test_endpoint_and_credentials_not_accepted_in_url(self):
        self.assertEqual(check.endpoint('smtps://mail.example.invalid'), ('mail.example.invalid', 465, True))
        self.assertEqual(check.endpoint('smtp://mail.example.invalid:587'), ('mail.example.invalid', 587, False))
        self.assertEqual(check.endpoint('smtp://[::1]:587'), ('::1', 587, False))
        for value in ('https://example.invalid', 'smtp://user:secret@example.invalid', 'smtp://host:99999', 'smtp://host/?password=secret'):
            with self.assertRaises(ValueError):
                check.endpoint(value)

    def test_smtps_and_starttls_without_auth_or_mail(self):
        with tempfile.TemporaryDirectory() as directory:
            cert, key = certificate(directory)
            for implicit in (True, False):
                server = Server(cert, key, implicit=implicit)
                try:
                    url = '{}://127.0.0.1:{}'.format('smtps' if implicit else 'smtp', server.port)
                    env = dict(os.environ, SSL_CERT_FILE=str(cert))
                    result = subprocess.run([sys.executable, str(SCRIPT), '--server', url], env=env,
                                            capture_output=True, text=True, timeout=10)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertIn('certificado verificado', result.stdout)
                    self.assertTrue(server.commands)
                    self.assertTrue(set(server.commands) <= {b'EHLO', b'STARTTLS'})
                    self.assertFalse(server.messages)
                finally:
                    server.close()

    def test_untrusted_certificate_reported(self):
        with tempfile.TemporaryDirectory() as directory:
            cert, key = certificate(directory)
            server = Server(cert, key)
            try:
                result = subprocess.run([sys.executable, str(SCRIPT), '--server', 'smtps://127.0.0.1:{}'.format(server.port)],
                                        capture_output=True, text=True, timeout=10)
                self.assertEqual(result.returncode, 1)
                self.assertIn('certificado no valido', result.stdout)
                self.assertFalse(server.messages)
            finally:
                server.close()


if __name__ == '__main__':
    unittest.main()
