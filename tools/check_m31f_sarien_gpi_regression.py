#!/usr/bin/env python
from __future__ import print_function
import sys

text = open('pmgpi.c', 'r').read()
start = text.find('O2LONG __cdecl GpiBitBlt(')
end = text.find('\n/* 356 */', start)
if start < 0 or end < 0:
    print('M31F Sarien GPI regression FAILED: GpiBitBlt not found')
    sys.exit(1)
body = text[start:end]
need = [
    'dhgt = gpi_surface_height(dst)',
    'dy = dhgt - (y0 > y1 ? y0 : y1)',
    'shgt = gpi_surface_height(src)',
    'sy = shgt - sy - sh',
    'StretchBlt'
]
missing = [x for x in need if x not in body]
if missing:
    print('M31F Sarien GPI regression FAILED:', missing)
    sys.exit(1)
if 'DIB-SRCCOPY' in body or 'StretchDIBits(' in body:
    print('M31F Sarien GPI regression FAILED: obsolete special screen-present path returned')
    sys.exit(1)
print('M31F Sarien GPI regression PASS')
print('  three-point framebuffer presents now use the same height-aware native HDC path')
print('  Sarien remains protected by the bitmap palette round-trip check below')


# Sarien/FastGPI performs a QueryBitmapBits -> SetBitmapBits round trip and
# reuses the returned BITMAPINFO2.  QueryBitmapBits must therefore populate
# the RGB table before SetBitmapBits is allowed to consume it.
qstart = text.find('O2LONG __cdecl GpiQueryBitmapBits(')
qend = text.find('\n/* 601 */', qstart)
sstart = text.find('O2LONG __cdecl GpiSetBitmapBits(')
send = text.find('\n/* 588 */', sstart)
if qstart < 0 or qend < 0 or sstart < 0 or send < 0:
    print('M31F Sarien palette regression FAILED: bitmap bits functions not found')
    sys.exit(1)
qbody = text[qstart:qend]
sbody = text[sstart:send]
need_query = [
    'gpi_store_bitmap_info(b, info)',
    'c[i * 4UL + 0UL] = b->palette[i].rgbBlue',
    'c[i * 4UL + 1UL] = b->palette[i].rgbGreen',
    'c[i * 4UL + 2UL] = b->palette[i].rgbRed'
]
missing = [x for x in need_query if x not in text]
if missing:
    print('M31F Sarien palette regression FAILED:', missing)
    sys.exit(1)
if 'gpi_apply_bitmap_palette(p, b, info)' not in sbody:
    print('M31F Sarien palette regression FAILED: SetBitmapBits no longer consumes returned bitmap info')
    sys.exit(1)
print('M31F Sarien palette round-trip PASS')
print('  GpiQueryBitmapBits now returns the RGB table consumed by GpiSetBitmapBits')
