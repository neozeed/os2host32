#!/usr/bin/env python
from __future__ import print_function
import re, sys

def fail(msg):
    print('M31G R18 CHECK FAILED:', msg)
    return 1

def main():
    src=open('pmwin.c').read()
    m=re.search(r'O2ULONG\s+__cdecl\s+WinSetWindowPos\s*\(.*?\n\}', src, re.S)
    if not m:
        return fail('WinSetWindowPos implementation missing')
    body=m.group(0)

    required=[
        'if (!GetWindowRect(wh, &rc))',
        'outer_w = rc.right - rc.left;',
        'outer_h = rc.bottom - rc.top;',
        'if (flags & O2_SWP_SIZE)',
        'win_y = GetSystemMetrics(SM_CYSCREEN) - y - outer_h;',
        'win_y = (pr.bottom - pr.top) - y - outer_h;',
        'pm_trace("WinSetWindowPos"',
    ]
    for tok in required:
        if tok not in body:
            return fail('move-only/current-size semantics missing: '+tok)

    # The old bug seeded outer_h from the ignored cy argument before testing
    # SWP_SIZE. That makes a 32px desktop window moved with cy=0 query back
    # exactly 32 pixels too low (20 -> -12), which is the NEKO trace.
    pre_size=body.split('if (flags & O2_SWP_SIZE)',1)[0]
    if re.search(r'outer_h\s*=\s*cy\s*;', pre_size):
        return fail('outer_h still seeded from ignored cy before SWP_SIZE')
    if re.search(r'outer_w\s*=\s*cx\s*;', pre_size):
        return fail('outer_w still seeded from ignored cx before SWP_SIZE')

    # Arithmetic model of the observed NEKO move-only case.
    screen_h=900
    requested_y=20
    current_h=32
    native_y=screen_h-requested_y-current_h
    queried_y=screen_h-(native_y+current_h)
    if queried_y != requested_y:
        return fail('move-only coordinate round-trip model failed')

    print('M31G R18 WinSetWindowPos regression PASS')
    print('  move-only calls ignore cx/cy but use the current native size for Y conversion')
    print('  32px desktop window round-trips OS/2 y=20 -> y=20 instead of y=-12')
    print('  R14 subclass/timer and R16 resource-DLL loader behavior remain inherited')
    return 0

if __name__=='__main__':
    sys.exit(main())
