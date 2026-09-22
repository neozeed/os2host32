#!/usr/bin/env python
from __future__ import print_function
import re, sys

def fail(msg):
    print('M31G R9 PMWIN FAILED:', msg)
    return 1

def main():
    d=open('pmwin.def','r').read()
    c=open('pmwin.c','r').read()
    if not re.search(r'^\s*WinQueryCapture\s+@804\s+NONAME\s*$', d, re.M):
        return fail('WinQueryCapture @804 export missing')
    if not re.search(r'^\s*WinIsWindowEnabled\s+@773\s+NONAME\s*$', d, re.M):
        return fail('WinIsWindowEnabled @773 export missing')

    q=re.search(r'O2HWND\s+__cdecl\s+WinQueryCapture\s*\(\s*O2HWND\s+desktop\s*\)\s*\{(.*?)\n\}', c, re.S)
    if not q:
        return fail('WinQueryCapture(HWND_DESKTOP) implementation missing')
    qfn=q.group(1)
    for token in ['GetCapture()', 'guest_hwnd(wh)', 'return wh ? guest_hwnd(wh) : 0']:
        if token not in qfn:
            return fail('WinQueryCapture native capture mapping missing: '+token)

    e=re.search(r'O2ULONG\s+__cdecl\s+WinIsWindowEnabled\s*\(\s*O2HWND\s+hwnd\s*\)\s*\{(.*?)\n\}', c, re.S)
    if not e:
        return fail('WinIsWindowEnabled(HWND) implementation missing')
    efn=e.group(1)
    for token in ['native_hwnd(hwnd)', 'IsWindow(wh)', 'IsWindowEnabled(wh) ? 1UL : 0UL']:
        if token not in efn:
            return fail('WinIsWindowEnabled native mapping missing: '+token)

    blob=(qfn+'\n'+efn).upper()
    if 'NEKO' in blob or 'E.EXE' in blob:
        return fail('application-specific behavior found')

    print('M31G R9 PMWIN regression PASS')
    print('  PMWIN.804 = WinQueryCapture(HWND_DESKTOP)')
    print('  capture state is paired with the existing WinSetCapture native path')
    print('  PMWIN.773 = WinIsWindowEnabled(HWND)')
    print('  enabled state comes from the native HWND without app-specific behavior')
    return 0

if __name__=='__main__': sys.exit(main())
