#!/usr/bin/env python
from __future__ import print_function
import re, sys

def fail(msg):
    print('M31G R11 HELPMGR CHECK FAILED:', msg)
    return 1

def main():
    d=open('helpmgr.def','r').read()
    c=open('helpmgr.c','r').read()
    host=open('os2host32.c','r').read()
    mk=open('Makefile','r').read()

    if not re.search(r'^\s*WinDestroyHelpInstance\s+@52\s+NONAME\s*$', d, re.M):
        return fail('HELPMGR.52 WinDestroyHelpInstance export missing')
    fn=re.search(r'BOOL\s+__cdecl\s+WinDestroyHelpInstance\s*\(\s*O2HWND\s+hwndHelpInstance\s*\)\s*\{(.*?)\n\}', c, re.S)
    if not fn:
        return fail('BOOL WinDestroyHelpInstance(HWND) implementation missing')
    body=fn.group(1)
    if 'helpmgr_remove_instance(hwndHelpInstance)' not in body:
        return fail('destroy does not validate/remove compatibility-owned help instance')
    for token in ['PMCOMPAT_MAX_HELP_INSTANCES', 'g_help_instances', 'return FALSE', 'return TRUE']:
        if token not in c:
            return fail('help-instance ownership registry missing: '+token)
    if not re.search(r'"HELPMGR"', host):
        return fail('HELPMGR is not routed as a native compatibility personality')
    if 'HELPMGR.dll: helpmgr.c helpmgr.def' not in mk:
        return fail('HELPMGR.dll Makefile rule missing')
    if not re.search(r'^compat:.*\bHELPMGR\.dll\b', mk, re.M):
        return fail('HELPMGR.dll missing from compat target')
    blob=(body+'\n'+host).upper()
    if 'NEKO.EXE' in blob or 'NEKO' in body.upper():
        return fail('application-specific behavior found')

    print('M31G R11 HELPMGR regression PASS')
    print('  HELPMGR is a native compatibility personality (real GA DLL remains mixed-mode)')
    print('  HELPMGR.52 = WinDestroyHelpInstance(HWND), BOOL result')
    print('  destroy only accepts compatibility-owned help-instance handles')
    print('  later milestones may extend HELPMGR without invalidating the R11 boundary')
    return 0

if __name__=='__main__': sys.exit(main())
