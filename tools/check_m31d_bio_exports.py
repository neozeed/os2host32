#!/usr/bin/env python
from __future__ import print_function
import re,sys
EXPECTED={
'PMGPI':{355,359,369,370,379,398,404,453,489,492,506,516,517,530,598,604,610},
'PMWIN':{701,703,707,710,716,726,728,729,733,738,743,757,763,765,767,781,789,793,814,829,834,840,848,849,854,858,875,883,888,892,899,903,908,910,911,912,915,919,920,923,926,929},
'PMSHAPI':{114,116},
'DOSCALLS':{224,230,234,256,282,299,304,305,348},
}
FILES={'PMGPI':'pmgpi.def','PMWIN':'pmwin.def','PMSHAPI':'pmshapi.def','DOSCALLS':'doscalls.def'}
for mod,want in EXPECTED.items():
    got=set()
    for line in open(FILES[mod]):
        m=re.search(r'@(\d+)',line)
        if m: got.add(int(m.group(1)))
    miss=sorted(want-got)
    if miss:
        print('M31D BIO export regression FAILED:',mod,miss); sys.exit(1)
print('M31D BIO export regression PASS: all BIO ordinals covered')
