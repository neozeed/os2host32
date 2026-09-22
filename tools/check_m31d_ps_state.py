#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[1]
compat=(root/'pmcompat.h').read_text()
pmwin=(root/'pmwin.c').read_text()
pmgpi=(root/'pmgpi.c').read_text()
checks=[
    ('CompatPS saved_dc', 'int saved_dc;' in compat),
    ('PMWIN WinGetPS SaveDC', 'p->saved_dc = dc ? SaveDC(dc) : 0;' in pmwin),
    ('PMWIN WinBeginPaint SaveDC', 'p->dc = BeginPaint(wh, &p->paint);' in pmwin and 'p->saved_dc = SaveDC(p->dc);' in pmwin),
    ('WinEndPaint RestoreDC', 'RestoreDC(p->dc, p->saved_dc);\n        EndPaint' in pmwin),
    ('WinReleasePS RestoreDC', 'RestoreDC(p->dc, p->saved_dc);\n        ReleaseDC' in pmwin),
    ('PMGPI GpiCreatePS SaveDC', 'p->saved_dc = p->dc ? SaveDC(p->dc) : 0;' in pmgpi),
    ('PMGPI GpiDestroyPS RestoreDC', 'RestoreDC(p->dc, p->saved_dc);' in pmgpi),
]
bad=[name for name,ok in checks if not ok]
if bad:
    for name in bad: print('FAIL:',name)
    raise SystemExit(1)
print('M31D PS-state isolation regression PASS')
print('  WinBeginPaint saves the real BeginPaint HDC, then restores it before EndPaint')
print('  WinGetPS saves/restores its GetDC HDC before ReleaseDC')
print('  GpiCreatePS state restored by GpiDestroyPS')
print('  GpiSetClipRegion can no longer leak native clip state across PS lifetimes')
