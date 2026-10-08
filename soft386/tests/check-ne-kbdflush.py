#!/usr/bin/env python3
"""H4D: constructed OS/2 1.x NE with real KBDCALLS.13 far imports.

Assert module-scoped routing, 16-bit HKBD marshalling, AX return status,
and Pascal two-byte cleanup.  Deliberately no Microsoft binary required.
"""
import pathlib
import struct
import subprocess
import sys
import tempfile


def make_fixture(path):
    code = bytearray(b'\x89\xe3')  # mov bx,sp -- compare after each call
    fixups = []
    jfails = []

    def farcall(module, ordinal):
        at = len(code)
        code.extend(b'\x9a\xff\xff\xff\xff')
        fixups.append(struct.pack('<BBHHH', 3, 1, at + 1, module, ordinal))

    for handle in (0, 1):
        code.extend(b'\x68' + struct.pack('<H', handle))  # push HKBD
        farcall(1, 13)  # module 1 = KBDCALLS, ordinal 13
        code.extend(b'\x39\xdc')  # cmp sp,bx: callee popped 2 bytes
        code.append(0x75)  # jne failure
        jfails.append(len(code))
        code.append(0)
        code.extend(b'\x83\xf8\x00')  # cmp ax,0: APIRET must be 0
        code.append(0x75)
        jfails.append(len(code))
        code.append(0)

    code.extend(b'\x6a\x01\x6a\x00')  # DosExit(EXIT_PROCESS, 0)
    farcall(2, 5)  # module 2 = DOSCALLS, ordinal 5
    fail = len(code)
    code.extend(b'\x6a\x01\x6a\x2a')  # DosExit(EXIT_PROCESS, 42)
    farcall(2, 5)
    for at in jfails:
        disp = fail - (at + 1)
        assert -128 <= disp <= 127
        code[at] = disp & 255

    ne = 0x80
    out = bytearray(0x500)
    out[:2] = b'MZ'
    struct.pack_into('<I', out, 0x3c, ne)
    out[ne:ne+2] = b'NE'
    # 2 segments; autodata is segment 2. Both entry and initial stack are NE 16-bit.
    for off, val in {0x0e: 2, 0x10: 0, 0x12: 0x200, 0x14: 0,
                     0x16: 1, 0x18: 0x250, 0x1a: 2, 0x1c: 2,
                     0x1e: 2, 0x22: 0x40, 0x28: 0x50,
                     0x2a: 0x54, 0x32: 9}.items():
        struct.pack_into('<H', out, ne + off, val)
    out[ne+0x36] = 1  # OS/2 target
    struct.pack_into('<HHHH', out, ne+0x40, 1, len(code), 0x0d00, 0x100)
    struct.pack_into('<HHHH', out, ne+0x48, 2, 0x20, 0x0c01, 0x100)
    names = b'\x08KBDCALLS\x08DOSCALLS'
    struct.pack_into('<HH', out, ne+0x50, 0, 9)
    out[ne+0x54:ne+0x54+len(names)] = names
    out[0x200:0x200+len(code)] = code
    pos = 0x200 + len(code)
    struct.pack_into('<H', out, pos, len(fixups))
    for i, rec in enumerate(fixups):
        out[pos+2+i*8:pos+10+i*8] = rec
    path.write_bytes(out)


def main():
    with tempfile.TemporaryDirectory(prefix='soft386-ne-kbd-') as tmp:
        fixture = pathlib.Path(tmp) / 'kbdflush-ne16.exe'
        make_fixture(fixture)
        p = subprocess.run([sys.argv[1], '--trace-hc', '--run', str(fixture)],
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                           timeout=20)
        log = (p.stdout + p.stderr).decode('latin1', errors='replace')
        for expected in ('[1] KBDCALLS', '[2] DOSCALLS',
                         'KBD16.13 KbdFlushBuffer hkbd=0000 rc=0',
                         'KBD16.13 KbdFlushBuffer hkbd=0001 rc=0',
                         'termination=Dos16Exit(EXIT_PROCESS) rc=0'):
            assert expected in log, (expected, p.returncode, log)
        assert p.returncode == 0, (p.returncode, log)
    print('NE-H4D KBDCALLS.13 PASS: two 16-bit far calls, 2-byte Pascal cleanup, AX=0')


if __name__ == '__main__':
    main()
