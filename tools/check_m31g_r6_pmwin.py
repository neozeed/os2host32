#!/usr/bin/env python
from __future__ import print_function
import re, sys

def fail(msg):
    print('M31G R6 PMWIN FAILED:', msg)
    return 1

def main():
    d=open('pmwin.def','r').read()
    c=open('pmwin.c','r').read()
    if not re.search(r'^\s*WinLoadPointer\s+@780\s+NONAME\s*$', d, re.M):
        return fail('WinLoadPointer @780 export missing')
    m=re.search(r'O2ULONG\s+__cdecl\s+WinLoadPointer\s*\(\s*O2HWND\s+desktop\s*,\s*O2ULONG\s+module\s*,\s*O2ULONG\s+idres\s*\)\s*\{(.*?)\n\}', c, re.S)
    if not m:
        return fail('WinLoadPointer(HWND, HMODULE, ULONG) implementation missing')
    fn=m.group(1)
    for token in ['pm_find_resource(module, O2_RT_POINTER',
                  'pm_create_os2_pointer_icon(rr->data, rr->size, &is_pointer)',
                  'WinLoadPointer missing', 'WinLoadPointer OK',
                  '(O2ULONG)(DWORD)icon', 'return 0;']:
        if token not in fn:
            return fail('expected generic pointer-resource logic missing: '+token)
    if 'NEKO' in fn.upper():
        return fail('application-specific WinLoadPointer behavior found')
    for token in ['O2_BFT_POINTER', 'O2_BFT_COLORPOINTER',
                  'ii.fIcon = is_pointer ? FALSE : TRUE',
                  'ii.xHotspot', 'ii.yHotspot']:
        if token not in c:
            return fail('generic icon/pointer converter support missing: '+token)
    if 'pm_create_os2_color_icon' not in c:
        return fail('existing frame/dialog icon path was not preserved')
    print('M31G R6 PMWIN regression PASS')
    print('  PMWIN.780 = WinLoadPointer(HWND, HMODULE, ULONG)')
    print('  reuses registered RT_POINTER resources and OS/2 bitmap-array conversion')
    print('  IC/CI icons remain supported; PT/CP pointer signatures preserve hotspots')
    print('  missing/unparseable resources return NULLHANDLE; no app-specific fallback')
    return 0

if __name__=='__main__': sys.exit(main())
