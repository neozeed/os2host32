#!/usr/bin/env python
from __future__ import print_function
import re, sys

def fail(msg):
    print('M31G R10 API CHECK FAILED:', msg)
    return 1

def main():
    sd=open('pmshapi.def','r').read()
    sc=open('pmshapi.c','r').read()
    wd=open('pmwin.def','r').read()
    wc=open('pmwin.c','r').read()

    if not re.search(r'^\s*PrfOpenProfile\s+@102\s+NONAME\s*$', sd, re.M):
        return fail('PrfOpenProfile @102 export missing')
    p=re.search(r'DWORD\s+__cdecl\s+PrfOpenProfile\s*\(\s*DWORD\s+hab\s*,\s*const char \*fileName\s*\)\s*\{(.*?)\n\}', sc, re.S)
    if not p:
        return fail('PrfOpenProfile(HAB,PCSZ) implementation missing')
    pfn=p.group(1)
    for token in ['PMCOMPAT_MAX_PROFILES', 'g_next_profile_handle', 'strncpy(g_profiles[i].path, fileName']:
        if token not in sc and token not in pfn:
            return fail('private profile handle/path registry missing: '+token)
    if 'pmsh_profile_path(hini' not in sc:
        return fail('existing profile calls do not resolve private HINI paths')

    if not re.search(r'^\s*WinQueryWindowUShort\s+@844\s+NONAME\s*$', wd, re.M):
        return fail('WinQueryWindowUShort @844 export missing')
    q=re.search(r'O2USHORT\s+__cdecl\s+WinQueryWindowUShort\s*\(\s*O2HWND\s+hwnd\s*,\s*O2LONG\s+index\s*\)\s*\{(.*?)\n\}', wc, re.S)
    if not q:
        return fail('WinQueryWindowUShort(HWND,LONG) implementation missing')
    qfn=q.group(1)
    for token in ['native_hwnd(hwnd)', 'O2_QWS_ID', 'GetDlgCtrlID(wh)']:
        if token not in qfn:
            return fail('QWS_ID native window-ID mapping missing: '+token)

    blob=(pfn+'\n'+qfn).upper()
    if 'NEKO' in blob or 'E.EXE' in blob:
        return fail('application-specific behavior found')

    print('M31G R10 API regression PASS')
    print('  PMSHAPI.102 = PrfOpenProfile(HAB, PCSZ)')
    print('  private HINI values retain the requested host-side INI path')
    print('  PMWIN.844 = WinQueryWindowUShort(HWND, LONG)')
    print('  QWS_ID returns the native child/control identifier')
    return 0

if __name__=='__main__': sys.exit(main())
