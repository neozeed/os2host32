#!/usr/bin/env python
from __future__ import print_function
import re,sys
EXPECTED={
'DOSCALLS':{220,228,255,257,263,264,265,273,275,299},
'PMGPI':{359,489,588},
'PMWIN':{729,736,757,781,789,821,828,840,841,843,848,866,877,878,883,899,903,910,920,923},
}
FILES={'DOSCALLS':'doscalls.def','PMGPI':'pmgpi.def','PMWIN':'pmwin.def'}
for mod,want in EXPECTED.items():
    got=set()
    for line in open(FILES[mod]):
        m=re.search(r'@(\d+)',line)
        if m: got.add(int(m.group(1)))
    miss=sorted(want-got)
    if miss:
        print('M31E OPENDLG export regression FAILED:',mod,miss); sys.exit(1)
text=open('os2host32.c').read()
pm=open('pmwin.c').read()
checks=[
 ('guest DLL resource publication','publish_guest_pm_resources(g);' in text and 'GUESTMOD RESOURCES:' in text),
 ('per-module resource unregister','OS2PM_UnregisterModuleResources' in text and 'OS2PM_UnregisterModuleResources' in pm),
 ('RT_STRING id-0 parser','pos=2;' in pm and 'id / 16U' in pm),
 ('dialog listbox control','cls == 7U' in pm and 'LBS_NOTIFY' in pm),
 ('WM_CONTROL listbox bridge','O2_WM_CONTROL' in pm and 'LBN_DBLCLK' in pm),
 ('window extra data','WinQueryWindowULong' in pm and 'WinSetWindowULong' in pm),
 ('Beta-2 FILEFINDBUF','fill_findbuf_beta2' in open('doscalls.c').read() and 'cbBuf == 279UL' in open('doscalls.c').read()),
 ('DosSearchPath implementation','DosSearchPath' in open('doscalls.c').read() and '@228' in open('doscalls.def').read()),
 ('HELLO GpiSetAttrs color path','GpiSetAttrs' in open('pmgpi.c').read() and '@588' in open('pmgpi.def').read()),
 ('FID_TITLEBAR constant','#define O2_FID_TITLEBAR        0x8003U' in pm and 'id == O2_FID_TITLEBAR' in pm),
]
for name,ok in checks:
    if not ok:
        print('M31E OPENDLG host regression FAILED:',name); sys.exit(1)
print('M31E OPENDLG export/host regression PASS: all DLL host ordinals covered')
print('  guest-DLL resources + id-0 RT_STRING + listboxes + Beta-2 FILEFINDBUF present')
