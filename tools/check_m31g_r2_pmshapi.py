#!/usr/bin/env python
from __future__ import print_function
import re, sys

def fail(msg):
    print('M31G R2 PMSHAPI FAILED:', msg)
    return 1

def main():
    d=open('pmshapi.def','r').read()
    c=open('pmshapi.c','r').read()
    if not re.search(r'^\s*WinRemoveSwitchEntry\s+@129\s+NONAME\s*$', d, re.M):
        return fail('WinRemoveSwitchEntry @129 export missing')
    if not re.search(r'DWORD\s+__cdecl\s+WinRemoveSwitchEntry\s*\(\s*O2HSWITCH\s+hswitch\s*\)', c):
        return fail('WinRemoveSwitchEntry one-HSWITCH implementation missing')
    start=c.find('DWORD __cdecl WinRemoveSwitchEntry')
    tail=c[start:]
    if 'OS2_PM_TRACE' not in tail or 'PMSHAPI: WinRemoveSwitchEntry' not in tail or 'return 0;' not in tail:
        return fail('expected trace/success behavior missing')
    print('M31G R2 PMSHAPI regression PASS')
    print('  PMSHAPI.129 = WinRemoveSwitchEntry(HSWITCH)')
    print('  native host semantics: advisory removal, return 0 (success)')
    return 0

if __name__=='__main__': sys.exit(main())
