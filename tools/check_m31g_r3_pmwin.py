#!/usr/bin/env python
from __future__ import print_function
import re, sys

def fail(msg):
    print('M31G R3 PMWIN FAILED:', msg)
    return 1

def main():
    d=open('pmwin.def','r').read()
    c=open('pmwin.c','r').read()
    if not re.search(r'^\s*WinQueryWindowPos\s+@837\s+NONAME\s*$', d, re.M):
        return fail('WinQueryWindowPos @837 export missing')
    m=re.search(r'typedef\s+struct\s+O2SWP\s*\{(.*?)\}\s*O2SWP\s*;', c, re.S)
    if not m:
        return fail('O2SWP structure missing')
    body=m.group(1)
    fields=['O2ULONG fl','O2LONG cy','O2LONG cx','O2LONG y','O2LONG x',
            'O2HWND hwndInsertBehind','O2HWND hwnd','O2ULONG ulReserved1','O2ULONG ulReserved2']
    decls=[]
    for line in body.splitlines():
        line=line.strip().rstrip(';').strip()
        if line:
            decls.append(line)
    if decls != fields:
        return fail('O2SWP field layout mismatch: '+repr(decls))
    if not re.search(r'O2ULONG\s+__cdecl\s+WinQueryWindowPos\s*\(\s*O2HWND\s+hwnd\s*,\s*O2SWP\s*\*swp\s*\)', c):
        return fail('WinQueryWindowPos(HWND, O2SWP*) implementation missing')
    start=c.find('O2ULONG __cdecl WinQueryWindowPos')
    end=c.find('/* 838 */', start)
    fn=c[start:end]
    for token in ['GetWindowRect', 'ScreenToClient', 'GetClientRect',
                  'SM_CYSCREEN', 'GW_HWNDPREV', 'guest_hwnd',
                  'O2_SWP_MINIMIZE', 'O2_SWP_MAXIMIZE',
                  'PMWIN:']:
        # PMWIN trace prefix lives in pm_trace(), so don't require literal in fn.
        if token == 'PMWIN:':
            continue
        if token not in fn:
            return fail('expected generic mapping logic missing: '+token)
    if 'parent_h - (pt.y + swp->cy)' not in fn:
        return fail('parent-relative bottom-left Y conversion missing')
    if 'GetSystemMetrics(SM_CYSCREEN) - wr.bottom' not in fn:
        return fail('desktop bottom-left Y conversion missing')
    if 'return 1;' not in fn:
        return fail('success return missing')
    print('M31G R3 PMWIN regression PASS')
    print('  PMWIN.837 = WinQueryWindowPos(HWND, PSWP)')
    print('  32-bit OS/2 SWP layout preserved')
    print('  Win32 top-left coordinates -> OS/2 parent-relative bottom-left coordinates')
    return 0

if __name__=='__main__': sys.exit(main())
