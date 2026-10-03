"""Optional format regression against a user-supplied ZIP containing .MSG files.

No IBM messages are bundled. Builds the production portable decoder as a host
shared object and checks every indexed record in each MKMSGF file in the ZIP.
This is a binary-format check, not an execution oracle for OS/2 DosGetMessage.
"""
import ctypes as c
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zipfile

root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix="os2-msg-corpus-") as tmp:
    lib = Path(tmp) / "msg.so"
    subprocess.run([os.environ.get("HOSTCC", "cc"), "-shared", "-fPIC", "-O2",
                    "-I" + str(root / "common/include"),
                    str(root / "common/msg/os2_msg.c"), "-o", str(lib)], check=True)
    decode = c.CDLL(str(lib)).os2_msg_from_file
    decode.argtypes = [c.c_void_p, c.c_uint32, c.c_uint32, c.c_void_p,
                       c.c_uint32, c.c_void_p, c.c_uint32, c.POINTER(c.c_uint32)]
    decode.restype = c.c_uint32
    files = messages = absent = malformed = 0
    with zipfile.ZipFile(sys.argv[1]) as archive:
        for name in archive.namelist():
            if not name.upper().endswith(".MSG"):
                continue
            data = archive.read(name)
            if not data.startswith(b"\xffMKMSGF\0"):
                continue
            count, first = struct.unpack_from("<HH", data, 11)
            version = struct.unpack_from("<H", data, 16)[0]
            index = struct.unpack_from("<H", data, 18)[0] if version else 31
            end = struct.unpack_from("<I", data, 22)[0] if version else 0
            width, fmt = (2, "H") if data[15] else (4, "I")
            if index + count * width > len(data):
                src = c.create_string_buffer(data)
                actual = c.c_uint32()
                assert decode(src, len(data), first, None, 0, None, 0, c.byref(actual)) == 319, name
                print("Truncated input correctly rejected:", name)
                malformed += 1
                continue
            offsets = list(struct.unpack_from("<" + fmt * count, data, index))
            offsets.append(end or len(data))
            src = c.create_string_buffer(data)
            damaged = next((i for i, off in enumerate(offsets[:-1])
                            if data[off:off+1] not in (b"I", b"W", b"E", b"H", b"P", b"?")), None)
            if damaged is not None:
                actual = c.c_uint32()
                assert decode(src, len(data), first + damaged, None, 0, None, 0, c.byref(actual)) == 319, name
                print("Damaged message record correctly rejected:", name, first + damaged)
                malformed += 1
                continue
            for i in range(count):
                raw = data[offsets[i]:offsets[i + 1]]
                out = c.create_string_buffer(len(raw) + 32)
                actual = c.c_uint32(0xffffffff)
                rc = decode(src, len(data), first + i, None, 0, out, len(out), c.byref(actual))
                if raw[:1] == b"?":
                    assert rc == 317 and actual.value == 0, (name, first + i, rc)
                    absent += 1
                    continue
                prefix = data[8:11] + f"{first+i:04d}: ".encode() if raw[:1] in (b"E", b"W") else b""
                expected = prefix + raw[1:]
                assert rc == 0 and actual.value == len(expected), (name, first + i, rc, actual.value)
                assert out.raw[:actual.value] == expected, (name, first + i)
                messages += 1
            files += 1
    assert files and messages
    print(f"MKMSGF corpus: {files} intact files, {messages} messages, {absent} absent slots; {malformed} damaged inputs rejected PASS")
