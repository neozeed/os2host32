#!/usr/bin/env python
from __future__ import print_function
import re, sys
expected={
 'PMGPI': {356:'GpiBox',517:'GpiSetColor',519:'GpiSetCurrentPosition'},
 'PMWIN': {701:'WinAlarm',703:'WinBeginPaint',716:'WinCreateMsgQueue',726:'WinDestroyMsgQueue',728:'WinDestroyWindow',729:'WinDismissDlg',738:'WinEndPaint',757:'WinGetPS',763:'WinInitialize',765:'WinInvalidateRect',789:'WinMessageBox',814:'WinQueryDlgItemShort',834:'WinQueryWindow',840:'WinQueryWindowRect',848:'WinReleasePS',858:'WinSetDlgItemShort',888:'WinTerminate',899:'WinWindowFromID',903:'WinSendDlgItemMsg',908:'WinCreateStdWindow',910:'WinDefDlgProc',911:'WinDefWindowProc',912:'WinDispatchMsg',915:'WinGetMsg',919:'WinPostMsg',920:'WinSendMsg',923:'WinDlgBox',926:'WinRegisterClass'},
 'DOSCALLS': {224:'DosQueryHType',232:'DosEnterCritSec',234:'DosExit',256:'DosSetFilePtr',282:'DosWrite',286:'DosBeep',299:'DosAllocMem',304:'DosFreeMem',305:'DosSetMem',311:'DosCreateThread',348:'DosQuerySysInfo'}
}
files={'PMGPI':'pmgpi.def','PMWIN':'pmwin.def','DOSCALLS':'doscalls.def'}
for mod, need in expected.items():
    text=open(files[mod],'r').read()
    got={int(n):name for name,n in re.findall(r'^\s*([A-Za-z0-9_]+)\s+@(\d+)\s+NONAME',text,re.M)}
    missing=[]
    for n,name in sorted(need.items()):
        if got.get(n)!=name: missing.append((n,name,got.get(n)))
    if missing:
        print('M31C HANOI export regression FAILED:',mod,missing); sys.exit(1)
print('M31C HANOI export regression PASS')
print('  PMGPI: 356 GpiBox, 517 GpiSetColor, 519 GpiSetCurrentPosition')
print('  PMWIN: HANOI paint/dialog/post/message surface present')
print('  DOSCALLS: 232 DosEnterCritSec and 234 DosExit thread/process path present')
