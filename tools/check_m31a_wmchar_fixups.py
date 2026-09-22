#!/usr/bin/env python
"""Regression for LINK386 biased internal OFF32 fixups in Beta-2 WMCHAR.EXE."""
from __future__ import print_function

import os
import struct
import sys

SRC_MASK = 0x0f
SRC_SEL16 = 0x02
SRC_OFF32 = 0x07
SRC_ALIAS = 0x10
SRC_LIST = 0x20
TGT_MASK = 0x03
TGT_INTERNAL = 0x00
TGT_OFF32 = 0x10
TGT_OBJ16 = 0x40
TGT_ORD8 = 0x80
TGT_EXT_ORD = 0x01
TGT_EXT_NAME = 0x02


def u16(b, o):
    return struct.unpack_from("<H", b, o)[0]


def s16(b, o):
    return struct.unpack_from("<h", b, o)[0]


def u32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        "examples", "m31a-wmchar", "WMCHAR.EXE")
    blob = open(path, "rb").read()
    h = u32(blob, 0x3c)
    if blob[h:h + 2] != b"LE":
        print("WMCHAR fixup regression FAILED: image is not LE")
        return 1

    pages = u32(blob, h + 0x14)
    page_size = u32(blob, h + 0x28)
    object_table = h + u32(blob, h + 0x40)
    object_count = u32(blob, h + 0x44)
    fix_page = h + u32(blob, h + 0x68)
    fix_rec = h + u32(blob, h + 0x6c)
    data_pages = u32(blob, h + 0x80)

    objects = []
    for i in range(object_count):
        o = object_table + i * 24
        objects.append((u32(blob, o), u32(blob, o + 4)))  # size, preferred base

    sites = 0
    biased = []
    indexed_vk = []
    gacm_refs = []
    mismatches = []

    for page in range(1, pages + 1):
        p = fix_rec + u32(blob, fix_page + (page - 1) * 4)
        end = fix_rec + u32(blob, fix_page + page * 4)
        while p < end:
            typ = blob[p] if not isinstance(blob[p], str) else ord(blob[p])
            flags = blob[p + 1] if not isinstance(blob[p + 1], str) else ord(blob[p + 1])
            p += 2
            st = typ & SRC_MASK
            kind = flags & TGT_MASK

            if typ & SRC_LIST:
                count = blob[p] if not isinstance(blob[p], str) else ord(blob[p])
                p += 1
                source = None
            else:
                count = 1
                source = s16(blob, p)
                p += 2

            if flags & TGT_OBJ16:
                first = u16(blob, p)
                p += 2
            else:
                first = blob[p] if not isinstance(blob[p], str) else ord(blob[p])
                p += 1

            target_off = None
            if kind == TGT_INTERNAL:
                if st != SRC_SEL16:
                    if flags & TGT_OFF32:
                        target_off = u32(blob, p)
                        p += 4
                    else:
                        target_off = u16(blob, p)
                        p += 2
            elif kind == TGT_EXT_ORD:
                if flags & TGT_ORD8:
                    p += 1
                elif flags & TGT_OFF32:
                    p += 4
                else:
                    p += 2
            elif kind == TGT_EXT_NAME:
                p += 4 if (flags & TGT_OFF32) else 2
            else:
                print("WMCHAR fixup regression FAILED: unexpected target kind")
                return 1

            # WMCHAR has no additive/chained records; keep this regression narrow.
            if flags & 0x0c:
                print("WMCHAR fixup regression FAILED: unexpected additive/chained record")
                return 1

            if typ & SRC_LIST:
                sources = [s16(blob, p + i * 2) for i in range(count)]
                p += count * 2
            else:
                sources = [source]

            if kind == TGT_INTERNAL and st == SRC_OFF32 and not (typ & SRC_ALIAS):
                size, preferred = objects[first - 1]
                expected = (preferred + target_off) & 0xffffffff
                if target_off >= size:
                    biased.append((first, target_off, expected, tuple(sources)))
                if page == 2 and first == 2 and target_off == 0x000002ec:
                    indexed_vk.append((page, first, target_off, expected, tuple(sources)))
                if page == 2 and first == 2 and 0x00000bd8 <= target_off <= 0x00000bde:
                    gacm_refs.append((target_off, tuple(sources)))
                for source_off in sources:
                    source_file = data_pages + (page - 1) * page_size + source_off
                    actual = u32(blob, source_file)
                    sites += 1
                    if actual != expected:
                        mismatches.append((page, source_off, expected, actual))

    wanted = (2, 0xfffff9f0, 0x0001f9f0, (0x06cf, 0x07c4))
    if wanted not in biased:
        print("WMCHAR fixup regression FAILED: LINK386 biased fixup not found")
        print("  biased records:", biased)
        return 1
    if mismatches:
        print("WMCHAR fixup regression FAILED: preferred internal values differ")
        print("  first mismatch:", mismatches[0])
        return 1

    wanted_vk = (2, 2, 0x000002ec, 0x000202ec, (0x017c,))
    if wanted_vk not in indexed_vk:
        print("WMCHAR fixup regression FAILED: indexed VK table fixup not found")
        print("  indexed records:", indexed_vk)
        return 1

    wanted_gacm = {
        0x00000bd8: (0x0491, 0x0516, 0x05d8),
        0x00000bda: (0x0522,),
        0x00000bdb: (0x052e,),
        0x00000bdc: (0x0539,),
        0x00000bde: (0x0546,),
    }
    got_gacm = {}
    for target_off, sources in gacm_refs:
        got_gacm.setdefault(target_off, []).extend(sources)
    got_gacm = dict((k, tuple(sorted(v))) for k, v in got_gacm.items())
    wanted_gacm = dict((k, tuple(sorted(v))) for k, v in wanted_gacm.items())
    if got_gacm != wanted_gacm:
        print("WMCHAR fixup regression FAILED: gacm field fixups changed")
        print("  expected:", wanted_gacm)
        print("  got     :", got_gacm)
        return 1

    # The source at page 2 + 017c is the disp32 in:
    #   FF 34 85 EC 02 02 00   push dword ptr [eax*4+000202EC]
    # It must become 010202EC when object 2 moves to 01020000.
    phys_page2 = data_pages + (2 - 1) * page_size
    if blob[phys_page2 + 0x0179:phys_page2 + 0x017c] != b"\xff\x34\x85":
        print("WMCHAR fixup regression FAILED: VK indexed operand signature changed")
        return 1
    vk_relocated = (0x01020000 + 0x000002ec) & 0xffffffff
    if vk_relocated != 0x010202ec:
        print("WMCHAR fixup regression FAILED: VK relocation arithmetic")
        return 1

    relocated = (0x01020000 + 0xfffff9f0) & 0xffffffff
    if relocated != 0x0101f9f0:
        print("WMCHAR fixup regression FAILED: relocation arithmetic")
        return 1

    print("WMCHAR fixup regression PASS: %d internal OFF32 sites" % sites)
    print("  LINK386 biased target: object 2 + FFFFF9F0 -> preferred 0001F9F0")
    print("  relocated object base 01020000 -> biased value 0101F9F0")
    print("  indexed VK operand: page 2 + 017C, 000202EC -> 010202EC")
    print("  gacm field relocations: object 2 + 0BD8..0BDE verified")
    return 0


if __name__ == "__main__":
    sys.exit(main())
