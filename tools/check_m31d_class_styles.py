#!/usr/bin/env python3
from pathlib import Path
s=Path('pmwin.c').read_text(errors='replace')
need=[
 '#define O2_CS_CLIPCHILDREN     0x20000000UL',
 'O2ULONG style;',
 'g_classes[ci].style=style',
 'classStyle = pm_find_class_style(clientClass);',
 'if (classStyle & O2_CS_CLIPCHILDREN)',
 'style |= WS_CLIPCHILDREN;'
]
missing=[x for x in need if x not in s]
if missing:
    raise SystemExit('M31D class-style regression FAIL: missing ' + repr(missing))
print('M31D class-style regression PASS')
print('  OS/2 CS_CLIPCHILDREN preserved in class registry')
print('  WinCreateStdWindow maps it to native WS_CLIPCHILDREN')
