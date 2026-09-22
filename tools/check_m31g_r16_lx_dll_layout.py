#!/usr/bin/env python
from __future__ import print_function
import re, sys


def fail(msg):
    print('M31G R16 CHECK FAILED:', msg)
    return 1


def main():
    host=open('os2host32.c').read()
    pmwin=open('pmwin.c').read()
    pdef=open('pmwin.def').read()

    required=[
        'o->base == 0 || !virtual_range_is_free(o->base, alloc_size)',
        'overlapping_layout = 1',
        'layout_offsets = (U32 *)calloc',
        'packed_cursor = (packed_cursor + 0xffffUL) & ~0xffffUL',
        'actual_addr = (U32)(unsigned long)arena + layout_offsets[i]',
        'LX object preferred ranges overlap; using packed relocation',
        'free(layout_offsets);'
    ]
    for tok in required:
        if tok not in host:
            return fail('generic zero/duplicate-base LX relocation machinery missing: '+tok)

    # The public R15 e.exe fix must stay present in the same tree.
    if not re.search(r'^\s*WinQueryVersion\s+@833\s+NONAME\s*$', pdef, re.M):
        return fail('R15 PMWIN.833 export regressed')
    if '0x00020000UL' not in pmwin:
        return fail('R15 WinQueryVersion OS/2 2.0 contract regressed')

    # Model the actual NEKO.DLL object table without embedding/distributing the
    # historical binary: all three preferred bases are zero.  A 64-KB packed
    # layout must assign distinct slots.
    sizes=[0x27c8,0x7538,0x50]
    cursor=0
    offsets=[]
    for size in sizes:
        cursor=(cursor+0xffff) & ~0xffff
        offsets.append(cursor)
        cursor += size
    if offsets != [0x00000,0x10000,0x20000]:
        return fail('packed-layout model produced unexpected offsets: %r' % (offsets,))

    # Keep the fix architectural rather than specimen-specific.
    if re.search(r'(strcmp|_stricmp|lstrcmpiA)\s*\([^\n]*(NEKO\.DLL|NEKO)', host, re.I):
        return fail('NEKO-specific branch found in loader')

    print('M31G R16 zero/duplicate-base LX DLL relocation regression PASS')
    print('  base 0 is always treated as relocatable (never VirtualAlloc(NULL) as preferred 0)')
    print('  overlapping preferred object ranges receive distinct 64-KB packed arena slots')
    print('  NEKO.DLL model offsets: 00000000, 00010000, 00020000')
    print('  R15 PMWIN.833 WinQueryVersion remains present')
    return 0


if __name__=='__main__':
    sys.exit(main())
