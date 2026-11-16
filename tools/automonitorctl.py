#!/usr/bin/env python3
# Archivo: automonitorctl.py
# Descripción: Consulta el monitor local mediante socket y presenta snapshots de solo lectura.
# Necesita: Python 3, socket de un driver activo y version.h para identificación.
# Retornos: Código de proceso y salida interactiva/JSON; no inicia otro driver ni escribe a equipos.

"""Read-only SSH viewer for a running Automonitor driver (Python standard library)."""
import argparse
import datetime
import json
import os
import re
from pathlib import Path
import socket
import sys
import time
import unicodedata

DEFAULT_SOCKET = Path(__file__).resolve().parent.parent / '.runtime' / 'automonitor.sock'
MAX_SNAPSHOT = 8 * 1024 * 1024


def product_version():
    base = Path(__file__).resolve().parent
    for path in (base / 'version.h', base.parent / 'src/include/version.h'):
        if path.is_file():
            match = re.search(r'^#define AUTOMONITOR_VERSION "([^"]+)"', path.read_text(), re.M)
            if match:
                return match.group(1)
    return 'unknown'


def safe(value):
    """Never render terminal controls supplied by devices, serial data or log text."""
    return ''.join(c if not unicodedata.category(c).startswith('C') else ' ' for c in str(value))


def clipped(value, width):
    result, used = [], 0
    for char in safe(value):
        cells = 0 if unicodedata.combining(char) else 2 if unicodedata.east_asian_width(char) in ('W', 'F') else 1
        if used + cells > width:
            break
        result.append(char)
        used += cells
    return ''.join(result)


def elapsed(milliseconds):
    if milliseconds is None:
        return '--'
    seconds = milliseconds / 1000.0
    return '{:.1f}s'.format(seconds) if seconds < 60 else '{:.1f}m'.format(seconds / 60)


def stamp(timestamp):
    try:
        return datetime.datetime.fromtimestamp(timestamp).strftime('%H:%M:%S')
    except (ValueError, TypeError, OverflowError, OSError):
        return '--:--:--'


def fetch(path):
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as connection:
        connection.settimeout(0.8)
        connection.connect(str(path))
        chunks, size = [], 0
        deadline = time.monotonic() + 1.5
        while True:
            if time.monotonic() > deadline:
                raise ValueError('tiempo de lectura agotado')
            block = connection.recv(65536)
            if not block:
                break
            size += len(block)
            if size > MAX_SNAPSHOT:
                raise ValueError('instantanea demasiado grande')
            chunks.append(block)
    snapshot = json.loads(b''.join(chunks).decode('utf-8'))
    if not isinstance(snapshot, dict) or snapshot.get('version') != 1:
        raise ValueError('version de monitor incompatible')
    if 'error' in snapshot:
        raise ValueError(snapshot['error'])
    if not isinstance(snapshot.get('devices'), list) or not isinstance(snapshot.get('metrics'), dict):
        raise ValueError('instantanea incompleta')
    return snapshot


def find_device(devices, name):
    exact = [d for d in devices if d['name'] == name or d['key'] == name]
    candidates = exact or [d for d in devices if name.lower() in d['name'].lower()]
    if len(candidates) != 1:
        raise ValueError('dispositivo inexistente o nombre ambiguo: ' + safe(name))
    return candidates[0]


def device_line(device):
    return '{} | {} {} esclavo={} | {} | tags {}/{} | errores {}'.format(
        device['name'], device['protocol'], device['endpoint'], device['slave'], device['status'],
        device['fresh_tags'], len(device['tags']), device['failures'])


def tag_line(tag):
    value = '--' if tag['value'] is None else '{} {}'.format(tag['value'], tag['unit']).strip()
    return '{} = {} | {} | periodo {}s | edad {} | tag {:.2f}ms | rc={} | errores {}/{}'.format(
        tag['name'], value, tag['status'], tag['period_s'], elapsed(tag['age_ms']),
        tag['latency_us'] / 1000, tag['result'], tag['consecutive_failures'], tag['failures'])


def compact_tag(tag):
    value = '--' if tag['value'] is None else '{} {}'.format(tag['value'], tag['unit']).strip()
    return '{:<14} {:<10} {:<11} {:>5} {:>6} {:>6.2f} {:>5}'.format(
        clipped(tag['name'], 14), clipped(value, 10), tag['status'], tag['period_s'],
        elapsed(tag['age_ms']), tag['latency_us'] / 1000, tag['failures'])


def compact_device(device):
    return '{:<17} {:<8} {:<19} {:<13} {:>5} E={}'.format(
        clipped(device['name'], 17), device['protocol'], clipped(device['endpoint'], 19),
        device['status'], '{}/{}'.format(device['fresh_tags'], len(device['tags'])), device['failures'])


def event_line(event):
    repeated = ' (x{})'.format(event['repeats']) if event['repeats'] > 1 else ''
    return '{} {} [{}] {}{}'.format(stamp(event['time']), event['level'], event['component'], event['message'], repeated)


def plain(snapshot, device_name=None):
    db = snapshot['sql']
    lines = ['AUTOMONITOR {} {} [{}] PID={} gen={}'.format(snapshot.get('driver_version', '?'), snapshot['concentrator'], snapshot['phase'], snapshot['pid'], snapshot['generation']),
             'MariaDB {} (ultima operacion hace {}) | Alarmas activas {} / sin evaluar {} | Ventanas pendientes {}'.format(
                 db['status'], elapsed(db['age_ms']), snapshot['alarms_active'], snapshot['alarms_unknown'], snapshot['metrics']['pending_windows'])]
    devices = [find_device(snapshot['devices'], device_name)] if device_name else snapshot['devices']
    for device in devices:
        lines.append(device_line(device))
        if device_name:
            lines.extend('  ' + tag_line(tag) for tag in device['tags'])
    lines.append('Eventos recientes:')
    lines.extend(event_line(event) for event in snapshot['events'][-5:])
    return '\n'.join(safe(line) for line in lines)


class View:
    def __init__(self, path, device_name=None):
        self.path = path
        self.device_name = device_name
        self.snapshot = None
        self.error = None
        self.error_since = None
        self.selected_key = None
        self.selected = 0
        self.tag_selected = 0
        self.details = bool(device_name)
        self.rate = 0.0
        self.previous = None

    def refresh(self):
        try:
            snapshot = fetch(self.path)
            sample = (snapshot['pid'], snapshot['generation'], snapshot['generated_at'], snapshot['metrics']['reads'])
            if self.previous and sample[:2] == self.previous[:2] and sample[2] > self.previous[2]:
                self.rate = max(0, sample[3] - self.previous[3]) / (sample[2] - self.previous[2])
            elif not self.previous or sample[:2] != self.previous[:2]:
                self.rate = 0.0
            self.previous = sample
            self.snapshot = snapshot
            devices = snapshot['devices']
            if self.device_name:
                self.selected_key = find_device(devices, self.device_name)['key']
                self.device_name = None
            matches = [i for i, device in enumerate(devices) if device['key'] == self.selected_key]
            self.selected = matches[0] if matches else min(self.selected, max(0, len(devices) - 1))
            self.selected_key = devices[self.selected]['key'] if devices else None
            self.error = None
            self.error_since = None
        except (OSError, ValueError, KeyError, TypeError) as error:
            self.error = safe(error)
            if self.error_since is None:
                self.error_since = time.monotonic()

    def move(self, delta, devices=False):
        items = self.snapshot['devices'] if self.snapshot else []
        if not items:
            return
        if self.details and not devices:
            self.tag_selected = max(0, min(self.tag_selected + delta, len(items[self.selected]['tags']) - 1))
        else:
            self.selected = max(0, min(self.selected + delta, len(items) - 1))
            self.selected_key = items[self.selected]['key']
            self.tag_selected = 0

    def draw(self, screen, curses):
        screen.erase()
        height, width = screen.getmaxyx()
        def line(row, text, style=0):
            if 0 <= row < height and width > 1:
                try:
                    screen.addstr(row, 0, clipped(text, width - 1), style)
                except curses.error:
                    pass
        bold = curses.A_BOLD
        if height < 12 or width < 55:
            line(0, 'AUTOMONITOR - Amplia la terminal (minimo 55x12)', bold)
            line(1, '[Q] Cerrar visor | [R] Actualizar')
            screen.refresh()
            return
        snapshot = self.snapshot
        if snapshot:
            line(0, 'AUTOMONITOR {} {} [{}]'.format(snapshot.get('driver_version', '?'), snapshot['concentrator'], 'SIN CONEXION' if self.error else snapshot['phase']), bold)
            line(1, 'PID {} | {} | Lecturas tags/s {:.1f} | Alarmas {} (+{} sin evaluar)'.format(
                snapshot['pid'], stamp(snapshot['generated_at']), self.rate,
                snapshot['alarms_active'], snapshot['alarms_unknown']))
            db = snapshot['sql']
            line(2, 'MariaDB {} hace {} | Ventanas pendientes {} | Muestras guardadas {}'.format(
                'SIN VIGENCIA' if self.error else db['status'], elapsed(db['age_ms']), snapshot['metrics']['pending_windows'], snapshot['metrics']['stored_samples']))
        else:
            line(0, 'AUTOMONITOR - Esperando driver', bold)
            line(1, 'Socket: ' + str(self.path))
        if self.error:
            line(3, 'SIN CONEXION - DATOS SIN VIGENCIA | {}'.format(self.error), bold | curses.A_REVERSE)
        elif snapshot:
            devices = snapshot['devices']
            online = sum(d['status'] == 'ONLINE' for d in devices)
            line(3, 'Dispositivos ONLINE {}/{} | Gen {} | Lectura del estado: solo memoria'.format(online, len(devices), snapshot['generation']))
        event_count = min(4, max(1, (height - 10) // 3))
        event_top = height - event_count - 3
        body_top, body_height = 6, max(1, event_top - 7)
        devices = snapshot['devices'] if snapshot else []
        if self.details and devices:
            device = devices[self.selected]
            line(4, '{} | {} {} | esclavo {} | {}'.format(device['name'], device['protocol'], device['endpoint'], device['slave'], 'SIN VIGENCIA' if self.error else device['status']), bold)
            line(5, '  TAG            VALOR      ESTADO      PER.s   EDAD TAG.ms ERROR')
            tags = device['tags']
            self.tag_selected = max(0, min(self.tag_selected, max(0, len(tags) - 1)))
            start = max(0, min(self.tag_selected - body_height + 1, max(0, len(tags) - body_height)))
            for row, tag in enumerate(tags[start:start + body_height], body_top):
                current = start + row - body_top == self.tag_selected
                line(row, ('> ' if current else '  ') + compact_tag(dict(tag, status='SIN_VIGENCIA') if self.error else tag), curses.A_REVERSE if current else 0)
            selected = tags[self.tag_selected] if tags else None
            line(event_top - 1, 'Tag {}/{} | {} | rc={} | errores consecutivos {}'.format(self.tag_selected + 1 if tags else 0, len(tags), 'ultima valida ' + stamp(selected['acquired_at']) if selected else '--', selected['result'] if selected else '--', selected['consecutive_failures'] if selected else 0))
        else:
            line(4, 'DISPOSITIVOS - Enter para ver valores y periodos individuales', bold)
            line(5, '  NOMBRE            PROTO    DESTINO             ESTADO        TAGS ERRORES')
            start = max(0, min(self.selected - body_height + 1, max(0, len(devices) - body_height)))
            for row, device in enumerate(devices[start:start + body_height], body_top):
                current = start + row - body_top == self.selected
                line(row, ('> ' if current else '  ') + compact_device(dict(device, status='SIN_VIGENCIA') if self.error else device), curses.A_REVERSE if current else 0)
            if not devices:
                line(body_top, 'Sin dispositivos publicados' if snapshot else 'El driver debe estar iniciado con esta version del monitor.')
            line(event_top - 1, 'Dispositivo {}/{}'.format(self.selected + 1 if devices else 0, len(devices)))
        line(event_top, 'EVENTOS RECIENTES (registro completo en log/devlog.log)', bold)
        events = snapshot['events'][-event_count:] if snapshot else []
        for row, event in enumerate(events, event_top + 1):
            line(row, event_line(event))
        line(height - 2, '[Q] Cerrar visor [R] Actualizar [Enter/Esc] Detalle/lista', bold)
        line(height - 1, '[Arriba/Abajo] Seleccionar [Izq/Der] Dispositivo [PgUp/PgDn] Pagina')
        screen.refresh()

    def run(self, screen):
        import curses
        try:
            curses.curs_set(0)
        except curses.error:
            pass
        screen.keypad(True)
        screen.timeout(100)
        next_refresh = 0
        while True:
            if time.monotonic() >= next_refresh:
                self.refresh()
                next_refresh = time.monotonic() + 1
            self.draw(screen, curses)
            key = screen.getch()
            if key in (ord('q'), ord('Q')):
                return
            if key in (ord('r'), ord('R')):
                next_refresh = 0
            elif key == curses.KEY_UP:
                self.move(-1)
            elif key == curses.KEY_DOWN:
                self.move(1)
            elif key == curses.KEY_LEFT:
                self.move(-1, devices=True)
            elif key == curses.KEY_RIGHT:
                self.move(1, devices=True)
            elif key == curses.KEY_PPAGE:
                self.move(-max(1, screen.getmaxyx()[0] - 12))
            elif key == curses.KEY_NPAGE:
                self.move(max(1, screen.getmaxyx()[0] - 12))
            elif key in (10, 13, curses.KEY_ENTER):
                self.details = not self.details
            elif key == 27:
                self.details = False


def main(argv=None):
    parser = argparse.ArgumentParser(description='Monitor SSH de Automonitor. No inicia adquisicion ni consulta Modbus/SQL.')
    parser.add_argument('--socket', type=Path, default=DEFAULT_SOCKET, help='socket local del driver')
    parser.add_argument('--version', action='version', version='automonitorctl ' + product_version())
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--once', action='store_true', help='imprimir una instantanea y salir')
    mode.add_argument('--json', action='store_true', help='imprimir una instantanea JSON y salir')
    parser.add_argument('--device', help='abrir un dispositivo por nombre (unico)')
    args = parser.parse_args(argv)
    try:
        if args.once or args.json:
            snapshot = fetch(args.socket)
            print(json.dumps(snapshot, ensure_ascii=True) if args.json else plain(snapshot, args.device))
        else:
            if not sys.stdin.isatty() or not sys.stdout.isatty() or os.environ.get('TERM', 'dumb') == 'dumb':
                parser.error('se necesita una terminal SSH; usa --once o --json para salida sin TUI')
            import curses
            curses.wrapper(View(args.socket, args.device).run)
        return 0
    except KeyboardInterrupt:
        return 0
    except (OSError, ValueError, ImportError) as error:
        print('Monitor: {}. Comprueba que el driver este iniciado y la ruta --socket.'.format(safe(error)), file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
