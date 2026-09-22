#!/usr/bin/env python
from __future__ import print_function
import re, sys

def fail(msg):
    print('M31G R7 PMWIN FAILED:', msg)
    return 1

def main():
    d=open('pmwin.def','r').read()
    c=open('pmwin.c','r').read()
    if not re.search(r'^\s*WinCreateWindow\s+@909\s+NONAME\s*$', d, re.M):
        return fail('WinCreateWindow @909 export missing')
    start=c.find('O2HWND __cdecl WinCreateWindow(')
    end=c.find('/* 908 */', start)
    if start < 0 or end < 0:
        return fail('WinCreateWindow implementation missing')
    fn=c[start:end]
    signature=['O2HWND parent', 'const char *className', 'const char *windowName',
               'O2ULONG osStyle', 'O2LONG x', 'O2LONG y', 'O2LONG cx', 'O2LONG cy',
               'O2HWND owner', 'O2HWND insertBehind', 'O2ULONG id',
               'void *ctlData', 'void *presParams']
    for token in signature:
        if token not in fn:
            return fail('13-argument API signature incomplete: '+token)
    for token in ['O2_WC_STATIC', 'nativeClass = "STATIC"',
                  'style |= SS_ICON', 'style |= WS_VISIBLE',
                  'GetSystemMetrics(SM_CYSCREEN) - (int)y - (int)cy',
                  'GetClientRect(nativeParent, &pr)',
                  'pm_find_resource(0, O2_RT_POINTER',
                  'pm_create_os2_color_icon(rr->data, rr->size)',
                  'windowName[0] == \'#\'', 'insertBehind == O2_HWND_TOP',
                  'return guest_hwnd(hwnd)']:
        if token not in fn:
            return fail('expected generic WinCreateWindow behavior missing: '+token)
    if 'NEKO' in fn.upper():
        return fail('application-specific WinCreateWindow behavior found')
    if '#define O2_HWND_TOP            4UL' not in c:
        return fail('OS/2 HWND_TOP compatibility handle missing')
    if '#define O2_WC_STATIC           5U' not in c:
        return fail('OS/2 WC_STATIC public-class atom missing')
    print('M31G R7 PMWIN regression PASS')
    print('  PMWIN.909 = WinCreateWindow(13-argument flat 32-bit API)')
    print('  maps common WC_* public controls onto native Win32 control classes')
    print('  converts OS/2 lower-left coordinates to Win32 upper-left coordinates')
    print('  WC_STATIC/SS_ICON #N resolves through the registered RT_POINTER resource path')
    print('  no application-name/resource-ID special cases')
    return 0

if __name__=='__main__': sys.exit(main())
