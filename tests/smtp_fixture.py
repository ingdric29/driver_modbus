"""Loopback-only SMTP fixture. All recipients in tests use example.invalid."""
import socket
import base64
import ssl
import subprocess
import threading
import time
from pathlib import Path


def certificate(directory):
    key, cert = Path(directory) / 'key.pem', Path(directory) / 'cert.pem'
    subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '1',
                    '-keyout', str(key), '-out', str(cert), '-subj', '/CN=localhost',
                    '-addext', 'subjectAltName=DNS:localhost,IP:127.0.0.1'],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return cert, key


class Server:
    def __init__(self, cert, key, implicit=True, authenticate=True, reject_auth=False, reject_data=False, offer_starttls=True, tls_delay=0, expected_auth=None):
        self.context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        self.context.load_cert_chain(str(cert), str(key))
        self.implicit, self.authenticate = implicit, authenticate
        self.reject_auth, self.reject_data = reject_auth, reject_data
        self.offer_starttls = offer_starttls
        self.tls_delay = tls_delay
        self.expected_auth = expected_auth
        self.commands, self.messages, self.errors = [], [], []
        self.auth_payloads, self.envelopes = [], []
        self.listener = socket.socket()
        self.listener.bind(('127.0.0.1', 0))
        self.listener.listen(8)
        self.listener.settimeout(.1)
        self.port = self.listener.getsockname()[1]
        self.stop = threading.Event()
        self.thread = threading.Thread(target=self.serve)
        self.thread.start()

    def auth_response(self, token):
        if self.expected_auth is not None:
            try:
                parts = base64.b64decode(token, validate=True).split(b'\0')
                expected = [b'', self.expected_auth[0].encode(), self.expected_auth[1].encode()]
                if parts != expected:
                    return b'535 fixture credentials mismatch\r\n'
            except ValueError:
                return b'535 invalid fixture credentials\r\n'
        return b'235 authenticated\r\n'

    def serve(self):
        while not self.stop.is_set():
            try:
                connection, _ = self.listener.accept()
            except socket.timeout:
                continue
            except OSError:
                break
            try:
                connection.settimeout(2)
                if self.implicit:
                    time.sleep(self.tls_delay)
                    connection = self.context.wrap_socket(connection, server_side=True)
                connection.sendall(b'220 localhost fixture\r\n')
                buffer = b''
                tls = self.implicit
                data_mode = False
                finished = False
                auth_pending = False
                message = []
                while not self.stop.is_set() and not finished:
                    block = connection.recv(4096)
                    if not block:
                        break
                    buffer += block
                    while b'\r\n' in buffer:
                        line, buffer = buffer.split(b'\r\n', 1)
                        if data_mode:
                            if line == b'.':
                                self.messages.append(b'\r\n'.join(message))
                                connection.sendall(b'554 fixture rejected\r\n' if self.reject_data else b'250 queued fixture\r\n')
                                data_mode = False
                            else:
                                message.append(line[1:] if line.startswith(b'..') else line)
                            continue
                        if auth_pending:
                            self.auth_payloads.append(line)
                            connection.sendall(self.auth_response(line))
                            auth_pending = False
                            continue
                        verb = line.split(b' ', 1)[0].upper()
                        self.commands.append(verb)
                        if verb == b'EHLO':
                            response = b'250-localhost\r\n'
                            if not tls and self.offer_starttls:
                                response += b'250-STARTTLS\r\n'
                            if self.authenticate:
                                response += b'250-AUTH PLAIN\r\n'
                            connection.sendall(response + b'250 SIZE 1048576\r\n')
                        elif verb == b'STARTTLS':
                            connection.sendall(b'220 start TLS\r\n')
                            time.sleep(self.tls_delay)
                            connection = self.context.wrap_socket(connection, server_side=True)
                            tls = True
                            buffer = b''
                        elif verb == b'AUTH':
                            if len(line.split()) >= 3:
                                self.auth_payloads.append(line.split(b' ', 2)[2])
                            if self.reject_auth:
                                connection.sendall(b'535 wrong fixture credentials\r\n')
                            elif len(line.split()) >= 3:
                                connection.sendall(self.auth_response(line.split(b' ', 2)[2]))
                            else:
                                connection.sendall(b'334 \r\n')
                                auth_pending = True
                        elif verb in (b'MAIL', b'RCPT', b'RSET'):
                            self.envelopes.append(line)
                            connection.sendall(b'250 OK\r\n')
                        elif verb == b'DATA':
                            connection.sendall(b'354 send data\r\n')
                            data_mode, message = True, []
                        elif verb == b'QUIT':
                            connection.sendall(b'221 bye\r\n')
                            finished = True
                            break
                        else:
                            connection.sendall(b'500 unknown command\r\n')
            except (OSError, ssl.SSLError):
                pass  # Expected for certificate failures/disconnecting diagnostic clients.
            except Exception as error:
                self.errors.append(error)
            finally:
                connection.close()

    def close(self):
        self.stop.set()
        self.listener.close()
        self.thread.join(timeout=3)
        assert not self.thread.is_alive()
        assert not self.errors, self.errors
