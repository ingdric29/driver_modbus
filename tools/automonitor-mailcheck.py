#!/usr/bin/env python3
# Archivo: automonitor-mailcheck.py
# Descripción: Diagnostica conectividad SMTP/TLS sin autenticar ni enviar mensajes.
# Necesita: Python 3, servidor explícito o smtp_servidor en configdb.cfg y certificados confiables.
# Retornos: Informe de diagnóstico y código de proceso según resultados; no utiliza contraseñas SMTP.

"""Diagnose SMTP connectivity/TLS without authentication or sending mail."""
import argparse
import configparser
from pathlib import Path
import smtplib
import socket
import ssl
import sys
import time
from urllib.parse import urlsplit

DEFAULT_CONFIG = Path(__file__).resolve().parent.parent / 'configdb.cfg'


def endpoint(value):
    url = urlsplit(value.strip())
    if url.scheme not in ('smtp', 'smtps') or not url.hostname:
        raise ValueError('smtp_servidor debe usar smtp://host:587 o smtps://host:465')
    if url.username is not None or url.password is not None or url.query or url.fragment or url.path not in ('', '/'):
        raise ValueError('la URL debe contener solamente protocolo, servidor y puerto; no credenciales')
    port = url.port or (465 if url.scheme == 'smtps' else 25)
    return url.hostname, port, url.scheme == 'smtps'


def configured_server(path):
    config = configparser.ConfigParser(interpolation=None, strict=False)
    with path.open() as source:
        config.read_file(source)
    # Match the driver's iteration over INI sections, reading only this public field.
    value = ''
    for section in sorted(config.sections()):
        value = config.get(section, 'smtp_servidor', fallback='')
    if not value:
        raise ValueError('falta smtp_servidor en configdb.cfg; usa --server para indicarlo')
    return value


def probe(host, port, implicit_tls, timeout):
    mode = 'TLS implicito' if implicit_tls else 'STARTTLS'
    print('{}:{} | {}'.format(host, port, mode), flush=True)
    started = time.monotonic()
    connection = None
    stage = 'DNS/TCP/TLS' if implicit_tls else 'DNS/TCP/saludo SMTP'
    try:
        context = ssl.create_default_context()
        if implicit_tls:
            connection = smtplib.SMTP_SSL(host, port, timeout=timeout, context=context)
        else:
            connection = smtplib.SMTP(host, port, timeout=timeout)
        if not implicit_tls:
            stage = 'EHLO/STARTTLS'
            code, _ = connection.ehlo('automonitor.local')
            if code != 250:
                raise ValueError('el servidor rechazo EHLO (codigo {})'.format(code))
            connection.starttls(context=context)
        stage = 'EHLO tras TLS'
        code, _ = connection.ehlo('automonitor.local')
        if code != 250:
            raise ValueError('EHLO rechazo codigo {}'.format(code))
        print('  OK: certificado verificado, {}, {:.2f}s'.format(connection.sock.version(), time.monotonic() - started))
        print('  Configuracion compatible: smtp_servidor={}://{}:{}'.format('smtps' if implicit_tls else 'smtp', '[' + host + ']' if ':' in host else host, port))
        return True
    except ssl.SSLCertVerificationError as error:
        print('  ERROR {}: certificado no valido ({})'.format(stage, error.verify_message))
    except ssl.SSLError as error:
        print('  ERROR {}: negociacion TLS ({})'.format(stage, error.reason or type(error).__name__))
    except socket.gaierror:
        print('  ERROR DNS: no se pudo resolver el servidor')
    except (socket.timeout, TimeoutError):
        print('  ERROR {}: tiempo agotado ({:.2f}s)'.format(stage, time.monotonic() - started))
    except ConnectionRefusedError:
        print('  ERROR TCP: conexion rechazada')
    except smtplib.SMTPResponseException as error:
        print('  ERROR {}: respuesta SMTP {}'.format(stage, error.smtp_code))
    except smtplib.SMTPNotSupportedError:
        print('  ERROR {}: el servidor no anuncia STARTTLS'.format(stage))
    except (OSError, smtplib.SMTPException, ValueError) as error:
        # Do not print SMTP replies/credentials; only the local failure category.
        print('  ERROR {}: {}'.format(stage, type(error).__name__))
    finally:
        if connection is not None:
            connection.close()
    return False


def main(argv=None):
    parser = argparse.ArgumentParser(description='Comprobar conexion y TLS SMTP. No autentica ni envia correo; no detiene el driver.')
    parser.add_argument('--config', type=Path, default=DEFAULT_CONFIG)
    parser.add_argument('--server', help='URL SMTP alternativa, sin usuario ni clave')
    parser.add_argument('--ports', help='comparar puertos: 465=TLS implicito; otros=STARTTLS (ejemplo: 465,587)')
    parser.add_argument('--timeout', type=float, default=6, help='limite por operacion de red, segundos (1..30)')
    args = parser.parse_args(argv)
    try:
        if not 1 <= args.timeout <= 30:
            raise ValueError('timeout debe estar entre 1 y 30 segundos')
        host, port, implicit = endpoint(args.server or configured_server(args.config))
        ports = [int(item) for item in args.ports.split(',')] if args.ports else [port]
        if not ports or len(ports) > 5 or any(not 1 <= p <= 65535 for p in ports):
            raise ValueError('indica de uno a cinco puertos validos')
        print('Prueba de DNS, SMTP y TLS; sin AUTH, MAIL FROM, RCPT TO ni DATA.')
        results = [probe(host, p, p == 465 if args.ports else implicit, args.timeout) for p in ports]
        print('TLS accesible no confirma autenticacion ni entrega en la bandeja de entrada.')
        return 0 if any(results) else 1
    except (ValueError, OSError, configparser.Error):
        print('Diagnostico: configura una URL smtp/smtps valida y verifica la ruta --config; no incluyas credenciales en --server.', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
