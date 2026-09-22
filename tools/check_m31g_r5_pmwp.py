#!/usr/bin/env python
from __future__ import print_function
import re, sys

def fail(msg):
    print('M31G R5 PMWP FAILED:', msg)
    return 1

def main():
    d=open('pmwp.def','r').read()
    c=open('pmwp.c','r').read()
    h=open('os2host32.c','r').read()
    mk=open('Makefile','r').read()
    if not re.search(r'^\s*PMWPOrdinal203\s+@203\s+NONAME\s*$', d, re.M):
        return fail('ordinal-only PMWP @203 export missing')
    m=re.search(r'DWORD\s+__cdecl\s+PMWPOrdinal203\s*\(\s*DWORD\s+arg1\s*,\s*DWORD\s+arg2\s*,\s*DWORD\s+arg3\s*,\s*DWORD\s+arg4\s*\)\s*\{(.*?)\n\}', c, re.S)
    if not m:
        return fail('four-argument 32-bit PMWP.203 implementation missing')
    fn=m.group(1)
    # R5 originally proved an advisory zero-success stub.  Later milestones
    # may refine the same proven 4x32-bit ABI when stronger GA evidence is
    # available; accept either the historical R5 stub or the R14 DosLoadModule
    # bridge without making this old regression gate non-monotonic.
    if 'return 0;' not in fn and 'MAKEINTRESOURCEA(318)' not in c:
        return fail('PMWP.203 needs either the R5 zero-success path or later proven loader bridge')
    if 'NEKO' in fn.upper():
        return fail('application-specific PMWP behavior found')
    pers=re.search(r'static int is_personality_module.*?\{(.*?)\n\}', h, re.S)
    if not pers or '"PMWP"' not in pers.group(1):
        return fail('PMWP is not routed as a native compatibility personality')
    for token in ['compat:', 'PMWP.dll: pmwp.c pmwp.def', 'libpmwp.a']:
        if token not in mk:
            return fail('Makefile PMWP integration missing: '+token)
    print('M31G R5 PMWP regression PASS')
    print('  PMWP is now a native compatibility personality, not a guest WPS DLL')
    print('  PMWP.203 is exported ordinal-only with the proven 4x32-bit ABI')
    print('  ordinal 203 preserves the proven ABI; later milestones may refine its GA semantics')
    return 0

if __name__=='__main__': sys.exit(main())
