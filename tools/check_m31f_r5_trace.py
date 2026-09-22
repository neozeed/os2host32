from pathlib import Path
p = Path('pmgpi.c').read_text(errors='replace')
need = [
    'GpiCreatePS tid=',
    'GpiSetBitmap ENTER tid=',
    'GpiSetBitmapBits palette tid=',
    'GpiBitBlt ENTER tid=',
    'GpiBitBlt PAT ',
    'GpiBeginPath ENTER tid=',
    'GpiEndPath ENTER tid=',
    'GpiPolySpline ENTER tid=',
    'GpiSetClipPath ENTER tid=',
    'GpiConvert OK tid=',
    'GpiSetDefaultViewMatrix OK tid=',
]
missing = [x for x in need if x not in p]
if missing:
    raise SystemExit('M31F R5 deep trace regression FAIL: ' + ', '.join(missing))
print('M31F R5 deep trace regression PASS')
