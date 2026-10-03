#!/usr/bin/env python3
"""Offline loader tests; synthetic LX fixtures plus optional user specimen.

No historical executable is embedded or redistributed. The fixture builder is
test-only, not a production executable writer. An independent Python oracle
compares whole-object fingerprints (including unchanged external operands).
"""
import hashlib
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile

EXE = str(Path(sys.argv[1]).resolve())
MASK = 0xffffffff


def put32(b, offset, value):
    struct.pack_into('<I', b, offset, value & MASK)


def fnv(data):
    h = 2166136261
    for byte in data:
        h = ((h ^ byte) * 16777619) & MASK
    return h


def hashes(output):
    return [int(h, 16) for h in re.findall(
        r'INTAKE MODEL object=\d+ fnv1a=([0-9A-F]+)', output)]


def fixture(records=None, flags=0, page=None, size=64):
    # LX at 0x80; page map at 0x178; fixup offsets at 0x180;
    # records at 0x188; data deliberately NOT immediately after the map.
    b = bytearray(0x480)
    b[:2] = b'MZ'
    put32(b, 0x3c, 0x80)
    b[0x80:0x84] = b'LX\0\0'
    struct.pack_into('<HH', b, 0x88, 2, 1)
    for off, value in {0x14: 1, 0x18: 1, 0x20: 1, 0x24: size,
                       0x28: 4096, 0x2c: 9, 0x40: 0xe0, 0x44: 1,
                       0x48: 0xf8, 0x68: 0x100, 0x6c: 0x108,
                       0x70: 0x200, 0x74: 1, 0x78: 0x210,
                       0x80: 0x400}.items():
        put32(b, 0x80 + off, value)
    struct.pack_into('<6I', b, 0x160, size, 0x10000, 0x2005, 1, 1, 0)
    if page is None:
        page = bytes(range(size))
    struct.pack_into('<IHH', b, 0x178, 0, len(page), flags)
    if records is None:
        records = b'\x07\x00\x08\x00\x01\x20\x00'
    put32(b, 0x184, len(records))
    b[0x188:0x188 + len(records)] = records
    b[0x280:0x289] = b'\x08DOSCALLS'
    b[0x400:0x400 + len(page)] = page
    return b


def run(path, mode='--mixed-intake', expected=0, contains=None):
    r = subprocess.run([EXE, mode, str(path)], text=True,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    assert r.returncode == expected, (mode, r.returncode, r.stdout, r.stderr)
    if contains:
        assert contains in r.stdout + r.stderr, (contains, r.stdout, r.stderr)
    return r.stdout


def oracle(b):
    h = struct.unpack_from('<I', b, 0x3c)[0]
    u = lambda off: struct.unpack_from('<I', b, h + off)[0]
    objects = [struct.unpack_from('<6I', b, h + u(0x40) + i * 24)
               for i in range(u(0x44))]
    data, bases = [], []
    cursor = 0x1000000
    for size, pref, flags, first, count, _ in objects:
        cursor = (cursor + 65535) & ~65535
        bases.append(cursor)
        cursor += size
        obj = bytearray(size)
        for j in range(count):
            off, n, kind = struct.unpack_from('<IHH', b,
                h + u(0x48) + (first + j - 1) * 8)
            assert kind in (0, 3), 'specimen oracle needs normal/zero pages'
            if kind == 0:
                pos = u(0x80) + (off << u(0x2c))
                n = min(n, size - j * 4096)
                obj[j*4096:j*4096+n] = b[pos:pos+n]
        data.append(obj)
    internal = external = 0
    for pg in range(u(0x14)):
        a, z = struct.unpack_from('<II', b, h + u(0x68) + pg * 4)
        p, end = h + u(0x6c) + a, h + u(0x6c) + z
        while p < end:
            typ, flg = b[p:p+2]
            p += 2
            st, kind = typ & 15, flg & 3
            assert not flg & 12
            if typ & 32:
                count = b[p]
                p += 1
            else:
                count = 1
                sources = [struct.unpack_from('<h', b, p)[0]]
                p += 2
            n = 2 if flg & 64 else 1
            target = int.from_bytes(b[p:p+n], 'little')
            p += n
            value = 0
            if not (kind == 0 and st == 2):
                n = 1 if kind == 1 and flg & 128 else 4 if flg & 16 else 2
                value = int.from_bytes(b[p:p+n], 'little')
                p += n
            if typ & 32:
                sources = struct.unpack_from('<' + 'h' * count, b, p)
                p += 2 * count
            src = next(i for i, o in enumerate(objects)
                       if o[3] - 1 <= pg < o[3] - 1 + o[4])
            for rel in sources:
                pos = (pg - objects[src][3] + 1) * 4096 + rel
                if kind:
                    assert kind in (1, 2) and st == 8 and not typ & 16
                    external += 1
                    continue
                internal += 1
                dst = target - 1
                sel = 0x100 + 16 * dst + (8 if typ & 16 else 0)
                if st == 2:
                    patch = struct.pack('<H', sel)
                elif st == 6:
                    addr = value if typ & 16 or not objects[dst][2] & 0x2000 else bases[dst] + value
                    patch = struct.pack('<IH', addr & MASK, sel)
                else:
                    assert st in (7, 8) and not typ & 16
                    addr = bases[dst] + value
                    if st == 8:
                        addr -= bases[src] + pos + 4
                    patch = struct.pack('<I', addr & MASK)
                data[src][pos:pos+len(patch)] = patch
        assert p == end
    return [fnv(o) for o in data], internal, external


def main():
    checks = 0
    with tempfile.TemporaryDirectory(prefix='mixed-lx-test-') as td:
        path = Path(td) / 'fixture.exe'
        def test(b, expected=0, contains=None):
            nonlocal checks
            path.write_bytes(b)
            out = run(path, expected=expected, contains=contains)
            checks += 1
            return out
        # Verify normal OFF32 byte for byte, including neighbouring data.
        b = fixture()
        expected = bytearray(range(64))
        put32(expected, 8, 0x1000020)
        assert hashes(test(b)) == [fnv(expected)]
        run(path, '--scan', contains='Direct host path: supported')
        # Alias source list; selector/default and pointer relocation; REL32;
        # one external REL32 left as original bytes, not fake resolved.
        records = (b'\x32\x00\x02\x01\x08\x00\x0a\x00' +
                   b'\x02\x00\x0c\x00\x01' +
                   b'\x06\x10\x10\x00\x01\x20\x00\x00\x00' +
                   b'\x08\x00\x18\x00\x01\x30\x00' +
                   b'\x08\x81\x20\x00\x01\x6e')
        b = fixture(records)
        out = test(b, contains='internal-sites=5 external-deferred=1 nonflat-sites=4 alias-sites=2')
        assert hashes(out) == oracle(b)[0]
        run(path, '--run', 3, 'guest was NOT executed')
        run(path, '--fixups', 3, 'not in the current executable subset')
        # A fixup-attached far jump is reported as an edge, not permission to run.
        page = bytearray(range(64))
        page[20:26] = b'\x66\xea\x30\x00\x00\x00'
        b = fixture(b'\x12\x00\x18\x00\x01', page=page)
        out = test(b, contains='far-jump-edges=1')
        assert hashes(out) == oracle(b)[0]
        run(path, '--run', 3, 'guest was NOT executed')
        page[22] = 255
        test(fixture(b'\x12\x00\x18\x00\x01', page=page), 1,
             'far jump target outside object')
        page[20:22] = b'\x90\x90'
        test(fixture(b'\x12\x00\x18\x00\x01', page=page),
             contains='far-jump-edges=0')
        # Alias PTR16:32 encodes an object-relative offset, not a flat VA.
        b = fixture(b'\x16\x00\x10\x00\x01\x20\x00')
        assert hashes(test(b)) == oracle(b)[0]
        # Legal negative/bias OFF32 address constants must keep modular arithmetic.
        b = fixture(b'\x07\x10\x08\x00\x01\xf0\xff\xff\xff')
        assert hashes(test(b)) == oracle(b)[0]
        # Normal-page padding beyond object end and fully zero-filled object.
        assert len(hashes(test(fixture(size=35, page=bytes(range(128)))))) == 1
        assert hashes(test(fixture(b'', 3, b''))) == [fnv(bytes(64))]
        b = fixture(b'')
        put32(b, 0x170, 0)  # no pages, all BSS
        assert hashes(test(b)) == [fnv(bytes(64))]
        # Iterated pages, including zero repeat and expansion-overrun rejection.
        b = fixture(b'', 1, struct.pack('<HH', 32, 2) + b'AB')
        put32(b, 0x80 + 0x4c, 0x400)
        assert hashes(test(b)) == [fnv(b'AB' * 32)]
        b[0x400] = 0
        test(b, 1, 'invalid iterated expansion')
        b[0x400] = 33
        test(b, 1, 'invalid iterated expansion')
        # Decoder/geometry/relocation rejection cases.
        for off, value, reason in [
            (0xac, 32, 'invalid page geometry'),
            (0x178, MASK, 'page offset shift overflow'),
            (0x16c, 2, 'invalid object page range'),
            (0x170, 2, 'invalid object page range'),
            (0x160, 0x10000000, 'allocation exceeds'),
        ]:
            b = fixture()
            put32(b, off, value)
            test(b, 1, reason)
        b = fixture()
        struct.pack_into('<H', b, 0x17e, 5)
        test(b, 1, 'unsupported invalid/range/compressed page')
        b = fixture()
        struct.pack_into('<H', b, 0x17c, 4097)
        test(b, 1, 'stored page exceeds')
        test(fixture()[:0x410], 1, 'page data outside file')
        test(fixture(b'\x07\x00\x3e\x00\x01\x00\x00'), 1,
             'source lies outside object')
        test(fixture(b'\x07\x00\x08\x00\x02\x00\x00'), 1,
             'bad target object')
        test(fixture(b'\x06\x00\x08\x00\x01\xff\x00'), 1,
             'far pointer target outside object')
        test(fixture(b'\x05\x00\x08\x00\x01\x00\x00'), 1,
             'unsupported internal relocation model')
        test(fixture(b'\x07\x04\x08\x00\x01\x00\x00\x00\x00'), 1,
             'additive/chained model not implemented')
        test(fixture(b'\x07\x08'), 1, 'chained LX fixups')
        test(fixture(b'\x07'), 1, 'truncated LX fixup record')
        test(fixture(b'\x32\x00\x02\x01\x08\x00'), 1,
             'source list extends past page records')
        # Windows entry mode remains fail-closed even with executable bytes.
        b = fixture(b'')
        put32(b, 0x168, 0x1005)
        test(b)
        run(path, '--run', 3, 'guest was NOT executed')
    print('PASS: %d synthetic mixed-intake tests; native execution gate retained' % checks)
    if len(sys.argv) > 2:
        specimen = Path(sys.argv[2])
        b = specimen.read_bytes()
        out = run(specimen)
        expected, internal, external = oracle(b)
        assert hashes(out) == expected, 'specimen whole-object relocation mismatch'
        assert 'internal-sites=%d external-deferred=%d' % (internal, external) in out
        assert (internal, external) == (7779, 567)
        for token in ['file=00039E00 copied=35', 'file=0003A000 copied=48',
                      'obj=1+0001CF6E', 'obj=2+0000000A',
                      'obj=2+00000000', 'obj=1+0000B47C',
                      'obj=2+0000001B', 'obj=1+0001CF78',
                      'nonflat-sites=7 alias-sites=4 far-jump-edges=3']:
            assert token in out, token
        run(specimen, '--run', 1, 'TELNETPM native profile requires a 32-bit Win32 build')
        run(specimen, '--fixups', 3, 'not in the current executable subset')
        assert specimen.read_bytes() == b
        print('PASS: TELNETPM all seven object fingerprints match independent oracle')
        print('PASS: 7779 internal patches; 567 external sites unchanged; input unchanged')
        print('TELNETPM SHA256:', hashlib.sha256(b).hexdigest())


if __name__ == '__main__':
    main()
