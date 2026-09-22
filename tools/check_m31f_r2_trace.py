from pathlib import Path
p = Path('pmgpi.c').read_text(errors='replace')
d = Path('doscalls.c').read_text(errors='replace')
need_p = [
    'PMGPI: GpiCreateBitmap', 'PMGPI: GpiSetBitmapBits',
    'PMGPI: GpiSetDefaultViewMatrix', 'PMGPI: GpiConvert',
    'PMGPI: GpiBeginPath', 'PMGPI: GpiEndPath',
    'PMGPI: GpiSetClipPath', 'PMGPI: GpiPolySpline',
    'PMGPI: GpiBitBlt'
]
need_d = ['DOSCALLS IO: OPEN', 'DOSCALLS IO: QUERYFILEINFO', 'DOSCALLS IO: READ']
missing = [x for x in need_p if x not in p] + [x for x in need_d if x not in d]
if missing:
    raise SystemExit('M31F R2 trace regression FAIL: ' + ', '.join(missing))
print('M31F R2 trace regression PASS')
