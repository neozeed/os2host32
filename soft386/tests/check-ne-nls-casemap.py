#!/usr/bin/env python3
"""H4H: execute unmodified-style NE far imports for NLS.1 DosCaseMap.

Check 16-bit Pascal 10-byte frame, AX return, common CP437 uppercase,
explicit country/codepage, invalid-country and invalid guest pointer paths.
"""
import pathlib
import struct
import subprocess
import sys
import tempfile


def fixture(path):
    code = bytearray(b'\x89\xe3')  # mov bx,sp
    fixups = []
    failures = []
    data_sel = 0x108  # NE segment two (GDT selector base 0x100, stride 8)

    def farcall(module, ordinal):
        at = len(code)
        code.extend(b'\x9a\xff\xff\xff\xff')
        fixups.append(struct.pack('<BBHHH', 3, 1, at + 1, module, ordinal))

    def push_word(x):
        code.extend(b'\x68' + struct.pack('<H', x))

    def invoke(length, country_off, buffer_off, selector=data_sel):
        # Pascal formal order DosCaseMap(len,cc,buf); push len first.
        push_word(length)
        push_word(data_sel)
        push_word(country_off)
        push_word(selector)
        push_word(buffer_off)
        farcall(1, 1)

    def check_ax(value):
        code.extend(b'\x3d' + struct.pack('<H', value))  # cmp ax,imm16
        code.extend(b'\x74\x03\xe9\x00\x00')  # je over near jmp failure
        failures.append(len(code) - 2)

    def check_sp():
        code.extend(b'\x39\xdc')  # cmp sp,bx
        code.extend(b'\x74\x03\xe9\x00\x00')  # je over near jmp failure
        failures.append(len(code) - 2)

    def check_byte(offset, value):
        code.extend(b'\x80\x3e' + struct.pack('<H', offset) + bytes([value]))
        code.extend(b'\x74\x03\xe9\x00\x00')  # je over near jmp failure
        failures.append(len(code) - 2)

    # Two bytes are mapped, including an embedded NUL outside the span.
    invoke(2, 0x10, 0x40)
    check_sp()
    check_ax(0)
    check_byte(0x40, ord('A'))
    check_byte(0x41, ord('B'))
    check_byte(0x42, ord('c'))

    # CP437 OEM high-byte mapping must use common CP437, not host toupper().
    invoke(1, 0x10, 0x48)
    check_sp()
    check_ax(0)
    check_byte(0x48, 0x9a)  # CP437 lowercase ue-dieresis -> uppercase

    # Invalid country: exact core error 398, buffer stays unchanged.
    invoke(2, 0x14, 0x44)
    check_sp()
    check_ax(398)
    check_byte(0x44, ord('x'))

    # Bad segment must return error, never touch data at 0x46.
    invoke(1, 0x10, 0x46, selector=0x7770)
    check_sp()
    check_ax(87)
    check_byte(0x46, ord('q'))

    # Length 0 allows a null buffer without incorrectly dereferencing it.
    invoke(0, 0x10, 0, selector=0)
    check_sp()
    check_ax(0)

    push_word(1)
    push_word(0)
    farcall(2, 5)  # DosExit(EXIT_PROCESS, 0)
    failure_offset = len(code)
    push_word(1)
    push_word(42)
    farcall(2, 5)  # DosExit(EXIT_PROCESS, 42)
    for at in failures:
        delta = failure_offset - (at + 2)
        assert -32768 <= delta <= 32767, (delta, at, failure_offset)
        struct.pack_into('<h', code, at, delta)

    ne = 0x80
    out = bytearray(0x700)
    out[:2] = b'MZ'
    struct.pack_into('<I', out, 0x3c, ne)
    out[ne:ne+2] = b'NE'
    values = {0x0e: 2, 0x10: 0, 0x12: 0x200, 0x14: 0,
              0x16: 1, 0x18: 0x250, 0x1a: 2, 0x1c: 2,
              0x1e: 2, 0x22: 0x40, 0x28: 0x50,
              0x2a: 0x54, 0x32: 9}
    for off, val in values.items():
        struct.pack_into('<H', out, ne + off, val)
    out[ne+0x36] = 1
    struct.pack_into('<HHHH', out, ne+0x40, 1, len(code), 0x0d00, 0x200)
    struct.pack_into('<HHHH', out, ne+0x48, 2, 0x60, 0x0c01, 0x100)
    names = b'\x03NLS\x08DOSCALLS'
    struct.pack_into('<HH', out, ne+0x50, 0, 4)
    out[ne+0x54:ne+0x54+len(names)] = names
    out[0x200:0x200+len(code)] = code
    pos = 0x200 + len(code)
    assert pos+2+8*len(fixups)<0x400
    struct.pack_into('<H', out, pos, len(fixups))
    for i, rec in enumerate(fixups):
        out[pos+2+i*8:pos+10+i*8] = rec
    struct.pack_into('<HH', out, 0x400+0x10, 1, 437)
    struct.pack_into('<HH', out, 0x400+0x14, 999, 437)
    out[0x400+0x40:0x400+0x43] = b'abc'
    out[0x400+0x44:0x400+0x46] = b'xy'
    out[0x400+0x46] = ord('q')
    out[0x400+0x48] = 0x81
    path.write_bytes(out)


def main():
    with tempfile.TemporaryDirectory(prefix='soft386-ne-nls-') as tmp:
        path = pathlib.Path(tmp) / 'nls-casemap-ne16.exe'
        fixture(path)
        p = subprocess.run([sys.argv[1], '--trace-hc', '--run', str(path)],
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                           timeout=20)
        log = (p.stdout + p.stderr).decode('latin1', errors='replace')
        for expected in ('[1] NLS', '[2] DOSCALLS',
                         'NLS16.1 DosCaseMap len=2 country=1 cp=437 rc=0',
                         'NLS16.1 DosCaseMap len=2 country=999 cp=437 rc=398',
                         'NLS16.1 DosCaseMap len=0 country=1 cp=437 rc=0',
                         'termination=Dos16Exit(EXIT_PROCESS) rc=0'):
            assert expected in log, (expected, p.returncode, log)
        assert p.returncode == 0, (p.returncode, log)
    print('NE-H4H NLS.1 PASS: CP437 mapping, 10-byte Pascal cleanup, AX, invalid country/selector, zero-length')


if __name__ == '__main__':
    main()
