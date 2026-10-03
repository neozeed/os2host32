#!/usr/bin/env python3
"""Host integration: real TCP stream, Telnet fragmentation, independent ANSI grid.
No historical EXE or external network. Windows GUI checks live in window-size-smoke.
"""
import pathlib
import select
import socket
import subprocess
import sys
import tempfile
import time

checks = 0


def check(condition, message):
    global checks
    checks += 1
    if not condition:
        raise AssertionError(message)


class Grid:
    """Small independent oracle for the ANSI operations exercised by the fixture.
    Text and SGR attributes tracked per cell; no font/pixel claims.
    """
    def __init__(self):
        self.cells = [[(32, ()) for _ in range(80)] for _ in range(24)]
        self.x = self.y = 0
        self.attr = ()

    def clear(self, start, end):
        for pos in range(start, end):
            self.cells[pos // 80][pos % 80] = (32, self.attr)

    def feed(self, data):
        i = 0
        while i < len(data):
            byte = data[i]
            i += 1
            if byte == 27:
                assert data[i] == 91
                i += 1
                start = i
                while not 64 <= data[i] <= 126:
                    i += 1
                args, command = data[start:i].decode('ascii'), chr(data[i])
                i += 1
                if args.startswith('?'):
                    assert (args, command) in [('?6', 'l'), ('?7', 'h')]
                    continue
                values = [int(x or 0) for x in args.split(';')]
                if command == 'H':
                    self.y = (values[0] or 1) - 1
                    self.x = (values[1] or 1) - 1 if len(values) > 1 else 0
                elif command == 'J':
                    if values[0] == 2:
                        self.clear(0, 1920)
                    else:
                        assert values[0] == 0
                        self.clear(self.y * 80 + self.x, 1920)
                elif command == 'K':
                    lo = 0 if values[0] in (1, 2) else self.x
                    hi = self.x + 1 if values[0] == 1 else 80
                    self.clear(self.y * 80 + lo, self.y * 80 + hi)
                elif command == 'm':
                    for value in values:
                        self.attr = () if value == 0 else self.attr + (value,)
                elif command == 'r':
                    assert args == ''
                    self.x = self.y = 0
                else:
                    raise AssertionError(('unsupported ANSI', command, args))
            elif byte == 13:
                self.x = 0
            elif byte == 10:
                self.y += 1
                if self.y == 24:
                    self.cells.pop(0)
                    self.cells.append([(32, self.attr) for _ in range(80)])
                    self.y = 23
            else:
                assert byte >= 32 and self.x < 79, 'unexpected control or column-80 write'
                self.cells[self.y][self.x] = (byte, self.attr)
                self.x += 1

    def row(self, n):
        return bytes(byte for byte, _ in self.cells[n-1])


def recv_exact(client, length):
    data = bytearray()
    while len(data) < length:
        part = client.recv(length - len(data))
        if not part:
            raise AssertionError('unexpected disconnect')
        data.extend(part)
    return bytes(data)


def quiet(client):
    check(not select.select([client], [], [], .08)[0], 'unexpected echo/negotiation loop/output')


def fragmented(client, data):
    for byte in data:
        client.sendall(bytes([byte]))


def main():
    exe = str(pathlib.Path(sys.argv[1]).resolve())
    fixtures = {}
    for page in range(1, 6):
        output = subprocess.check_output([exe, '--dump', str(page)])
        check(output == subprocess.check_output([exe, '--dump', str(page)]), 'non-deterministic dump')
        fixtures[page] = output
        grid = Grid()
        grid.feed(output)
        check(all(grid.row(n)[79:] == b' ' for n in range(1,25)), 'column 80 must stay blank')
        if page == 1:
            for row in range(1,25):
                check(grid.row(row).startswith(('ROW %02d ' % row).encode()), 'row label missing')
        elif page == 2:
            check(grid.row(1) == grid.row(12) == grid.row(24), 'pixel strips differ')
            check(grid.row(1)[:30] == b'\xdb'*10 + b'\xdf'*10 + b'\xdc'*10, 'CP437 half blocks')
        elif page == 3:
            for color in range(8):
                check(grid.cells[color+2][0][1] == (30+color,), 'foreground color')
                check(grid.cells[color+2][39][1] == (37, 40+color), 'background color')
            for row, attr in [(14, 1), (15, 4), (16, 7)]:
                check(grid.cells[row-1][0][1] == (attr,), 'attribute')
        elif page == 4:
            check(grid.row(4).rstrip() == b'SPACES: [' + b' '*20 + b']', 'space overwrite')
            check(grid.row(6).rstrip() == b'EL0: KEEP', 'erase to end of line')
            check(not grid.row(8).strip(), 'erase whole line')
            check(grid.row(10).rstrip() == b'OVERWRITE: 0123456789', 'overwrite')
            check(grid.row(12).rstrip() == b' '*14 + b'KEEP', 'erase start of line')
            check(grid.row(14).rstrip() == b'ED0: KEEP', 'erase to end of display')
            check(not grid.row(15).strip(), 'erase following rows')
        else:
            for row in range(1, 25):
                expected = 'SCROLL %02d | expected final row %02d | HHHH yyyy ____' % (row+6, row)
                check(grid.row(row).rstrip() == expected.encode(), 'scroll grid mismatch')
    print('Fixed ANSI grids, CP437 strips, SGR attributes, erasure and 24-row scroll PASS')

    with socket.socket() as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    with tempfile.TemporaryFile() as log:
        proc = subprocess.Popen([exe, '--port', str(port)], stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + 5
            while True:
                try:
                    client = socket.create_connection(('127.0.0.1', port), timeout=2)
                    break
                except OSError:
                    if proc.poll() is not None or time.monotonic() > deadline:
                        raise
                    time.sleep(.02)
            negotiation = bytes([255,251,1, 255,251,3, 255,253,3, 255,251,0, 255,253,0])
            with client:
                check(recv_exact(client, 15) == negotiation, 'initial Telnet options')
                check(recv_exact(client, len(fixtures[1])) == fixtures[1], 'initial page')
                acks = bytes([255,253,1, 255,253,3, 255,251,3, 255,253,0, 255,251,0])
                fragmented(client, acks)
                quiet(client)
                client.sendall(acks)  # duplicate acknowledgments cannot loop
                quiet(client)
                fragmented(client, bytes([255,253,200, 255,251,201]))
                check(recv_exact(client, 6) == bytes([255,252,200, 255,254,201]), 'unsupported options')
                # Subnegotiation contents include page/quit keys and escaped IAC;
                # never let them leak into the application's command handler.
                client.sendall(bytes([255,250,31]) + b'Q12345'*400 + bytes([255,255]))
                quiet(client)
                fragmented(client, bytes([255,240]))
                quiet(client)
                client.sendall(bytes([255,255]) + b'\r\n\r\0')
                quiet(client)
                for page in range(1,6):
                    client.sendall(str(page).encode())
                    expected = fixtures[page].replace(b'\xff', b'\xff\xff')
                    check(recv_exact(client, len(expected)) == expected, 'wire page differs from fixture')
                    client.sendall(b'r\r\n')
                    check(recv_exact(client, len(expected)) == expected, 'redraw differs')
                    quiet(client)
                client.sendall(b'Q')
                check(client.recv(1) == b'', 'Q must disconnect')
            with socket.create_connection(('127.0.0.1', port), timeout=2) as client:
                check(recv_exact(client, 15) == negotiation, 'reconnect negotiation')
                check(recv_exact(client, len(fixtures[1])) == fixtures[1], 'reconnect resets page')
                client.sendall(bytes([255,254,1,255,254,3,255,252,3,255,254,0,255,252,0]))
                quiet(client)  # refusal of pending offers needs no answer
                client.sendall(b'1')
                check(recv_exact(client, len(fixtures[1])) == fixtures[1], 'ASCII works without options')
                client.sendall(b'2')
                warning = (b'\x1b[0m\x1b[2J\x1b[HPage 2 needs Telnet BINARY output (8-bit CP437).\r\n'
                           b'Enable binary negotiation in the client, then press R.\r\n'
                           b'ASCII pages 1, 3, 4, 5 remain available.')
                check(recv_exact(client, len(warning)) == warning, 'no high-bit data without BINARY')
                client.sendall(bytes([255,253,0]) + b'R')
                check(recv_exact(client,3) == bytes([255,251,0]), 'late BINARY enable')
                check(recv_exact(client,len(fixtures[2])) == fixtures[2], 'CP437 after BINARY enable')
                client.sendall(b'Q')
                check(client.recv(1) == b'', 'second disconnect')
        finally:
            proc.terminate()
            proc.wait(timeout=5)
            log.seek(0)
            if sys.exc_info()[0]:
                print(log.read().decode(errors='replace'))
    print('TCP sessions, fragmented IAC/SB, option refusal/duplicates, redraw and reconnect PASS')
    print('Telnet display server: %d checks PASS' % checks)


if __name__ == '__main__':
    main()
