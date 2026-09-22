#!/usr/bin/env python3
from pathlib import Path
checks = [
    (Path('pmwin.c'), 'p->color = -1; /* OS/2 GPI default foreground is CLR_BLACK */'),
    (Path('pmgpi.c'), 'p->color = -1; /* OS/2 GPI default foreground is CLR_BLACK */'),
]
for path, needle in checks:
    text = path.read_text(errors='replace')
    if needle not in text:
        raise SystemExit(f'M31D GPI default regression FAIL: {path} does not initialize PS color to CLR_BLACK')
print('M31D GPI default regression PASS: PMWIN and PMGPI presentation spaces start at CLR_BLACK')
