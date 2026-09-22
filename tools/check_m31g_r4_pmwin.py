#!/usr/bin/env python
from __future__ import print_function
import re, sys

def fail(msg):
    print('M31G R4 PMWIN FAILED:', msg)
    return 1

def main():
    d=open('pmwin.def','r').read()
    c=open('pmwin.c','r').read()
    if not re.search(r'^\s*WinLoadMenu\s+@778\s+NONAME\s*$', d, re.M):
        return fail('WinLoadMenu @778 export missing')
    m=re.search(r'O2HWND\s+__cdecl\s+WinLoadMenu\s*\(\s*O2HWND\s+parent\s*,\s*O2ULONG\s+module\s*,\s*O2ULONG\s+idMenu\s*\)\s*\{(.*?)\n\}', c, re.S)
    if not m:
        return fail('WinLoadMenu(HWND, HMODULE, ULONG) implementation missing')
    fn=m.group(1)
    for token in ['pm_find_resource(module, O2_RT_MENU',
                  'pm_parse_menu_template(rr->data, rr->size, &used, 1)',
                  'WinLoadMenu missing', 'WinLoadMenu OK', 'return 0;',
                  '(O2HWND)(DWORD)menu']:
        if token not in fn:
            return fail('expected generic resource/menu logic missing: '+token)
    if 'SetMenu(' in fn or 'NEKO' in fn.upper():
        return fail('WinLoadMenu contains speculative attachment/app-specific behavior')
    if 'O2_RT_MENU' not in c or 'pm_parse_menu_template' not in c:
        return fail('pre-existing generic menu infrastructure missing')
    print('M31G R4 PMWIN regression PASS')
    print('  PMWIN.778 = WinLoadMenu(HWND, HMODULE, ULONG)')
    print('  reuses registered RT_MENU resources and the existing OS/2 menu parser')
    print('  missing/unparseable resources return NULLHANDLE; no app-specific fallback')
    return 0

if __name__=='__main__': sys.exit(main())
