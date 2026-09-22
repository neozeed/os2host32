#!/usr/bin/env python
from __future__ import print_function
import re, sys

def fail(msg):
    print('M31G R15 CHECK FAILED:', msg)
    return 1

def exported(text, name, ordinal):
    return re.search(r'^\s*'+re.escape(name)+r'\s+@'+str(ordinal)+r'\s+NONAME\s*$', text, re.M) is not None

def main():
    pmwin=open('pmwin.c').read()
    pdef=open('pmwin.def').read()
    host=open('os2host32.c').read()
    pmwp=open('pmwp.c').read()

    if not exported(pdef, 'WinQueryVersion', 833):
        return fail('PMWIN.833 WinQueryVersion export missing')
    m=re.search(r'O2ULONG\s+__cdecl\s+WinQueryVersion\s*\(\s*O2HAB\s+hab\s*\)\s*\{(.*?)\n\}', pmwin, re.S)
    if not m:
        return fail('WinQueryVersion one-argument HAB ABI missing')
    body=m.group(1)
    if '0x00020000UL' not in body:
        return fail('WinQueryVersion must advertise the OS/2 2.0 PM contract')
    if 'pm_trace("WinQueryVersion"' not in body:
        return fail('WinQueryVersion trace missing')

    # Preserve the R14 runtime fixes while extending only the e.exe PM surface.
    for tok in ['MAKEINTRESOURCEA(318)', '(const char *)(DWORD)arg3', '(DWORD *)(DWORD)arg2']:
        if tok not in pmwp:
            return fail('R14 PMWP.203 DosLoadModule bridge regressed: '+tok)
    for tok in ['SetWindowLongA(wh, GWL_WNDPROC', 'O2_SM_QUERYHANDLE', 'O2_WM_TIMER']:
        if tok not in pmwin:
            return fail('R14 subclass/timer bridge regressed: '+tok)
    if 'GetModuleHandleA("PMWIN.dll")' not in host:
        return fail('R14 resource-only module publication fallback regressed')

    print('M31G R15 PM-version regression PASS')
    print('  PMWIN.833 = WinQueryVersion(HAB), advertising OS/2 2.0 PM')
    print('  R14 NEKO PMWP/subclass/timer behavior remains intact')
    return 0

if __name__=='__main__':
    sys.exit(main())
