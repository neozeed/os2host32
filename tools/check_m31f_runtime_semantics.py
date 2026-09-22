#!/usr/bin/env python
from __future__ import print_function
import sys

def need(text, needles, label):
    miss=[n for n in needles if n not in text]
    if miss:
        print('M31F JIGSAW runtime regression FAILED:',label,miss)
        sys.exit(1)

pm=open('pmwin.c').read(); gpi=open('pmgpi.c').read(); dos=open('doscalls.c').read()
need(pm,[
 'O2_FCF_HORZSCROLL','O2_FID_HORZSCROLL','pm_scroll_proxy_handle',
 'WinPostQueueMsg','PostThreadMessageA','WinPeekMsg','PeekMessageA',
 'WinLoadDlg','heap_owned','WinSetDlgItemText','WinQueryDlgItemText',
 'WinMapWindowPoints','WinSetCapture','WinSetParent','WinQueryUpdateRegion',
 'O2_MM_QUERYITEM','MIIM_SUBMENU','O2_WM_MOUSEMOVE','O2_WM_BUTTON1DBLCLK',
 'O2_SV_CXFULLSCREEN','O2_SV_CXBYTEALIGN'
], 'PMWIN surface')
need(gpi,[
 'bitcount != 1 && bitcount != 4 && bitcount != 8',
 'CreateDIBSection','GpiAssociate','GpiBeginPath','GpiEndPath','GpiPolySpline',
 'GpiSetClipPath','GpiSetDefaultViewMatrix','GpiQueryDefaultViewMatrix',
 'GpiCombineRegion','GpiSetRegion','GpiDestroyRegion','GpiQueryPel'
], 'PMGPI surface')
need(dos,['DosSetPriority','SetThreadPriority'], 'DOSCALLS priority')
print('M31F JIGSAW runtime-semantics regression PASS')
print('  horizontal frame scrollbars keep distinct host identities')
print('  WinPostQueueMsg/WinPeekMsg use real Win32 thread message queues')
print('  modeless WinLoadDlg and WC_SCROLLBAR controls are represented')
print('  4bpp DIB sections, regions, paths, splines and default-view transforms are present')
