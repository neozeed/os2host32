#!/usr/bin/env python
"""M31B regression: decode WMCHAR's real LE resource table and PM menu/icon."""
from __future__ import print_function

import os
import sys


def u16(b, o):
    return b[o] | (b[o + 1] << 8)


def u32(b, o):
    return (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) |
            (b[o + 3] << 24))


def object_bytes(data, h, objnum):
    objtab = h + u32(data, h + 0x40)
    objcnt = u32(data, h + 0x44)
    pagemap = h + u32(data, h + 0x48)
    pagesize = u32(data, h + 0x28)
    datapage = u32(data, h + 0x80)
    last = u32(data, h + 0x2c)
    if not (1 <= objnum <= objcnt):
        raise ValueError("bad object")
    o = objtab + (objnum - 1) * 24
    size = u32(data, o)
    first = u32(data, o + 12)
    count = u32(data, o + 16)
    out = bytearray()
    for i in range(count):
        m = pagemap + (first - 1 + i) * 4
        phys = (data[m] << 16) | (data[m + 1] << 8) | data[m + 2]
        flags = data[m + 3]
        if flags == 3:
            out.extend(b"\0" * pagesize)
            continue
        if flags != 0 or phys == 0:
            raise ValueError("unsupported LE page-map entry")
        n = pagesize
        # LE last-page size refers to the last physical page in the file.
        if last and phys == u32(data, h + 0x14):
            n = last
        off = datapage + (phys - 1) * pagesize
        chunk = data[off:off + n]
        if len(chunk) != n:
            raise ValueError("short physical page")
        out.extend(chunk)
        if n < pagesize:
            out.extend(b"\0" * (pagesize - n))
    return bytes(out[:size])


def parse_menu(blob, base=0):
    if base + 10 > len(blob):
        raise ValueError("short menu header")
    total = u32(blob, base)
    cp = u16(blob, base + 4)
    reserved = u16(blob, base + 6)
    count = u16(blob, base + 8)
    if total < 10 or base + total > len(blob):
        raise ValueError("bad menu length")
    p = base + 10
    items = []
    for _ in range(count):
        if p + 6 > base + total:
            raise ValueError("short menu item")
        style = u16(blob, p)
        attr = u16(blob, p + 2)
        ident = u16(blob, p + 4)
        p += 6
        text = None
        child = None
        if not (style & 0x0004):  # MIS_SEPARATOR
            end = blob.find(b"\0", p, base + total)
            if end < 0:
                raise ValueError("unterminated menu text")
            text = blob[p:end].decode("latin1")
            p = end + 1
            if style & 0x0010:  # MIS_SUBMENU
                child, used = parse_menu(blob, p)
                p += used
        items.append((style, attr, ident, text, child))
    if p > base + total:
        raise ValueError("menu overrun")
    return {"length": total, "codepage": cp, "reserved": reserved,
            "items": items}, total


def flatten(menu, depth=0):
    out = []
    for style, attr, ident, text, child in menu["items"]:
        out.append((depth, style, attr, ident, text))
        if child:
            out.extend(flatten(child, depth + 1))
    return out


def main():
    exe_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        "examples", "m31a-wmchar", "WMCHAR.EXE")
    ico_path = sys.argv[2] if len(sys.argv) > 2 else os.path.join(
        "examples", "m31a-wmchar", "WMCHAR.ICO")
    data = open(exe_path, "rb").read()
    h = u32(data, 0x3c)
    if data[h:h + 2] != b"LE":
        print("M31B resource regression FAILED: not LE")
        return 1
    rtab = h + u32(data, h + 0x50)
    rcnt = u32(data, h + 0x54)
    resources = {}
    for i in range(rcnt):
        p = rtab + i * 14
        typ, name = u16(data, p), u16(data, p + 2)
        size, obj, off = u32(data, p + 4), u16(data, p + 8), u32(data, p + 10)
        ob = object_bytes(data, h, obj)
        if off + size > len(ob):
            print("M31B resource regression FAILED: resource outside object")
            return 1
        resources[(typ, name)] = ob[off:off + size]

    if set(resources) != set([(1, 1), (3, 1)]):
        print("M31B resource regression FAILED: unexpected resource set", sorted(resources))
        return 1

    icon = resources[(1, 1)]
    ico = open(ico_path, "rb").read()
    if icon != ico or len(icon) != 1010 or u16(icon, 0) != 0x4142:
        print("M31B resource regression FAILED: icon resource mismatch")
        return 1

    menu_blob = resources[(3, 1)]
    menu, used = parse_menu(menu_blob)
    if used != len(menu_blob) or menu["length"] != 213 or menu["codepage"] != 850:
        print("M31B resource regression FAILED: menu header")
        return 1
    flat = flatten(menu)
    expected = [
        (0, 0x0011, 0x0000, 0x0100, "~Actions"),
        (1, 0x0001, 0x0000, 0x0101, "~Clear List"),
        (1, 0x0001, 0x0000, 0x0102, "Log ~KeyUps"),
        (1, 0x0004, 0x4000, 0xffff, None),
        (1, 0x0040, 0x0000, 0x8004, "E~xit"),
        (0, 0x0011, 0x0000, 0x0110, "~Display"),
        (1, 0x0001, 0x0000, 0x0111, "~Number"),
        (1, 0x0001, 0x0000, 0x0112, "~Virtual Key"),
        (1, 0x0001, 0x0000, 0x0113, "~Character"),
        (1, 0x0001, 0x0000, 0x0114, "~Scancode"),
        (1, 0x0001, 0x0000, 0x0115, "~Repeat Count"),
        (1, 0x0001, 0x0000, 0x0116, "~Flags"),
    ]
    if flat != expected:
        print("M31B resource regression FAILED: decoded menu differs")
        print("decoded:")
        for row in flat:
            print(" ", row)
        return 1

    print("M31B WMCHAR resource regression PASS")
    print("  LE resources: type 1/id 1 icon = 1010 bytes, byte-identical WMCHAR.ICO")
    print("  LE resources: type 3/id 1 menu = 213 bytes, codepage 850")
    print("  menu items: 12 including 2 submenus, separator, and SC_CLOSE syscommand")
    return 0


if __name__ == "__main__":
    sys.exit(main())
