#!/usr/bin/env python
"""Host-independent regression for M31A WMCHAR PM stack headroom."""
from __future__ import print_function

import os
import struct
import sys

PM_NATIVE_MIN_STACK = 0x00040000
GCXMAXCHAR_OFF = 0x00000BD4


def u32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        "examples", "m31a-wmchar", "WMCHAR.EXE")
    blob = open(path, "rb").read()
    h = u32(blob, 0x3c)
    if blob[h:h + 2] != b"LE":
        print("WMCHAR stack regression FAILED: image is not LE")
        return 1

    stack_object = u32(blob, h + 0x20)
    stack_esp = u32(blob, h + 0x24)
    object_table = h + u32(blob, h + 0x40)
    object_count = u32(blob, h + 0x44)

    objects = []
    for i in range(object_count):
        o = object_table + i * 24
        size = u32(blob, o)
        base = u32(blob, o + 4)
        objects.append((base, size))

    if stack_object != 2:
        print("WMCHAR stack regression FAILED: expected stack object 2, got %d" % stack_object)
        return 1
    base, size = objects[stack_object - 1]
    if base != 0x00020000 or size != 0x00002C80 or stack_esp != 0x00002C80:
        print("WMCHAR stack regression FAILED: historical stack layout changed")
        print("  base=%08X size=%08X esp=%08X" % (base, size, stack_esp))
        return 1

    headroom = stack_esp - GCXMAXCHAR_OFF
    if headroom != 0x000020AC:
        print("WMCHAR stack regression FAILED: expected 0x20AC bytes above gcxMaxChar")
        print("  got 0x%08X" % headroom)
        return 1

    promoted_end = base + PM_NATIVE_MIN_STACK
    for i, (other_base, other_size) in enumerate(objects):
        if i == stack_object - 1 or other_size == 0:
            continue
        other_end = other_base + other_size
        if base < other_end and other_base < promoted_end:
            print("WMCHAR stack regression FAILED: 256 KB promotion overlaps object %d" % (i + 1))
            return 1

    print("WMCHAR stack regression PASS")
    print("  historical stack : object 2 + 00002C80 (11 KB object)")
    print("  gcxMaxChar/gacm  : object 2 + 00000BD4..00000BDE")
    print("  old headroom     : 000020AC bytes (%d bytes)" % headroom)
    print("  PM runtime stack : object 2 + 00040000 (256 KB, no object overlap)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
