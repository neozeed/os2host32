from __future__ import print_function
import re
import sys
from pathlib import Path

if len(sys.argv) != 2:
    raise SystemExit('usage: summarize_m31f_jigsaw_trace.py jigsaw-r5-err.txt')
p = Path(sys.argv[1])
text = p.read_text(errors='replace').splitlines()
gpi = [ln for ln in text if 'PMGPI:' in ln]
io = [ln for ln in text if 'DOSCALLS IO:' in ln]
print('JIGSAW trace summary')
print('  total lines :', len(text))
print('  PMGPI lines :', len(gpi))
print('  DOS IO lines:', len(io))
for key in ['GpiCreateBitmap OK', 'GpiSetBitmapBits OK', 'GpiBitBlt PAT OK',
            'GpiBitBlt OK', 'GpiBeginPath OK', 'GpiEndPath OK',
            'GpiPolySpline OK', 'GpiSetClipPath OK', 'GpiSetClipPath RESET OK']:
    print('  %-24s %d' % (key + ':', sum(key in ln for ln in gpi)))
fails = [ln for ln in gpi if ' FAIL ' in ln or ln.endswith(' FAIL')]
print('  explicit FAIL lines:', len(fails))
for ln in fails[-20:]:
    print('FAIL>', ln)
print('\nLast 80 PMGPI lines:')
for ln in gpi[-80:]:
    print(ln)
