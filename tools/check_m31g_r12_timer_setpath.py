#!/usr/bin/env python
from __future__ import print_function
import re, sys

def fail(msg):
    print('M31G R12 CHECK FAILED:', msg); return 1

def main():
    wd=open('pmwin.def').read(); wc=open('pmwin.c').read()
    dd=open('doscalls.def').read(); dc=open('doscalls.c').read()
    if not re.search(r'^\s*WinStartTimer\s+@884\s+NONAME\s*$', wd, re.M):
        return fail('PMWIN.884 export missing')
    fn=re.search(r'O2ULONG\s+__cdecl\s+WinStartTimer\s*\(\s*O2HAB\s+hab\s*,\s*O2HWND\s+hwnd\s*,\s*O2ULONG\s+idTimer\s*,\s*O2ULONG\s+timeout\s*\)\s*\{(.*?)\n\}', wc, re.S)
    if not fn: return fail('WinStartTimer(HAB,HWND,ULONG,ULONG) missing')
    for tok in ['SetTimer(wh', 'O2_WM_TIMER', 'case WM_TIMER']:
        if tok not in wc: return fail('timer translation missing: '+tok)
    if not re.search(r'^\s*DosSetPathInfo\s+@219\s+NONAME\s*$', dd, re.M):
        return fail('DOSCALLS.219 export missing')
    fn2=re.search(r'O2APIRET\s+__cdecl\s+DosSetPathInfo\s*\(\s*const char \*path\s*,\s*O2ULONG\s+level\s*,\s*const void \*buffer\s*,\s*O2ULONG\s+cb\s*,\s*O2ULONG\s+options\s*\)\s*\{(.*?)\n\}', dc, re.S)
    if not fn2: return fail('DosSetPathInfo five-argument ABI missing')
    body=fn2.group(1)
    for tok in ['O2_FIL_STANDARD', 'SetFileTime', 'SetFileAttributesA', 'get32(p + 20)']:
        if tok not in body: return fail('FIL_STANDARD metadata mapping missing: '+tok)
    blob=(fn.group(1)+'\n'+body).upper()
    if 'NEKO.EXE' in blob or 'E.EXE' in blob: return fail('application-specific behavior found')
    print('M31G R12 API regression PASS')
    print('  PMWIN.884 = WinStartTimer(HAB, HWND, ULONG, ULONG)')
    print('  Win32 WM_TIMER is translated to OS/2 WM_TIMER (0x24)')
    print('  DOSCALLS.219 = DosSetPathInfo(path, level, buffer, cb, options)')
    print('  FIL_STANDARD timestamps/attributes map to Win32 file metadata')
    return 0
if __name__=='__main__': sys.exit(main())
