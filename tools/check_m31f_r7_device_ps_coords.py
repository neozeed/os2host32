#!/usr/bin/env python
from __future__ import print_function
import sys

text = open('pmgpi.c', 'r').read()

cs = text.find('O2HPS __cdecl GpiCreatePS(')
ce = text.find('\n/* 453 */', cs)
if cs < 0 or ce < 0:
    print('M31F R7 device-PS coordinate check FAILED: GpiCreatePS not found')
    sys.exit(1)
create = text[cs:ce]
if 'p->hwnd = p->dc ? WindowFromDC(p->dc) : NULL;' not in create:
    print('M31F R7 device-PS coordinate check FAILED: GpiCreatePS does not retain its HWND')
    sys.exit(1)

as_ = text.find('O2LONG __cdecl GpiAssociate(')
ae = text.find('\n/* 354 */', as_)
if as_ < 0 or ae < 0:
    print('M31F R7 device-PS coordinate check FAILED: GpiAssociate not found')
    sys.exit(1)
assoc = text[as_:ae]
if 'p->hwnd = p->dc ? WindowFromDC(p->dc) : NULL;' not in assoc:
    print('M31F R7 device-PS coordinate check FAILED: GpiAssociate does not refresh its HWND')
    sys.exit(1)

hs = text.find('static int gpi_surface_height(')
he = text.find('\nstatic DWORD gpi_rop', hs)
height = text[hs:he]
for required in ['if (p->bitmap) return (int)p->bitmap->height;',
                 'if (p->hwnd && GetClientRect(p->hwnd, &r))']:
    if required not in height:
        print('M31F R7 device-PS coordinate check FAILED: surface-height rule missing:', required)
        sys.exit(1)

bs = text.find('O2LONG __cdecl GpiBitBlt(')
be = text.find('\n/* 356 */', bs)
blt = text[bs:be]
for required in ['dy = dhgt - (y0 > y1 ? y0 : y1);',
                 'sy = shgt - sy - sh;',
                 'StretchBlt(dst->dc, dx, dy, dw, dh,']:
    if required not in blt:
        print('M31F R7 device-PS coordinate check FAILED: height-aware blit rule missing:', required)
        sys.exit(1)
if 'DIB-SRCCOPY' in blt or 'StretchDIBits(' in blt:
    print('M31F R7 device-PS coordinate check FAILED: legacy screen-only blit branch remains')
    sys.exit(1)

print('M31F R7 device-PS coordinate check PASS')
print('  GpiCreatePS/GpiAssociate retain the native HWND for device surface height')
print('  bitmap-to-device blits use the unified bottom-left -> top-left StretchBlt path')
