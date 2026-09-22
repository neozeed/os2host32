#!/usr/bin/env python
from __future__ import print_function
import re, sys

def fail(msg):
    print('M31G R8 PMWIN FAILED:', msg)
    return 1

def main():
    d=open('pmwin.def','r').read()
    c=open('pmwin.c','r').read()
    if not re.search(r'^\s*WinDestroyPointer\s+@727\s+NONAME\s*$', d, re.M):
        return fail('WinDestroyPointer @727 export missing')
    m=re.search(r'O2ULONG\s+__cdecl\s+WinDestroyPointer\s*\(\s*O2ULONG\s+pointer\s*\)\s*\{(.*?)\n\}', c, re.S)
    if not m:
        return fail('WinDestroyPointer(HPOINTER) implementation missing')
    fn=m.group(1)
    for token in ['pm_is_owned_pointer(pointer)', 'DestroyIcon((HICON)(DWORD)pointer)',
                  'pm_forget_owned_pointer(pointer)', 'return ok ? 1UL : 0UL']:
        if token not in fn:
            return fail('expected owned-pointer destroy behavior missing: '+token)
    load=c[c.find('O2ULONG __cdecl WinLoadPointer('):c.find('/* 727 */')]
    if 'pm_remember_owned_pointer((O2ULONG)(DWORD)icon)' not in load:
        return fail('WinLoadPointer does not register owned HPOINTER handles')
    if 'PMCOMPAT_MAX_OWNED_POINTERS' not in c or 'g_owned_pointers' not in c:
        return fail('owned HPOINTER registry missing')
    if 'NEKO' in fn.upper() or 'NEKO' in load.upper():
        return fail('application-specific pointer behavior found')
    print('M31G R8 PMWIN regression PASS')
    print('  PMWIN.727 = WinDestroyPointer(HPOINTER)')
    print('  WinLoadPointer-created handles are tracked as compatibility-owned')
    print('  system/shared handles are refused rather than destroyed')
    print('  owned CreateIconIndirect handles are released with DestroyIcon')
    return 0

if __name__=='__main__': sys.exit(main())
