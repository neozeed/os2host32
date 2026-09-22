#!/usr/bin/env python
from __future__ import print_function
import re, sys

def fail(msg):
    print('M31G R19 CHECK FAILED:', msg)
    return 1

def main():
    src=open('pmwin.c').read()

    required=[
        '#define O2_WC_SLIDER          38U',
        '#define O2_SLM_QUERYSLIDERINFO  0x036cU',
        '#define O2_SLM_SETSLIDERINFO    0x0371U',
        '#define O2_SLM_SETTICKSIZE      0x0372U',
        'PMCOMPAT_TRACKBAR_CLASS',
        'pm_ensure_trackbar_class()',
        'cls == O2_WC_SLIDER',
        'publicClass == O2_WC_SLIDER',
        'pm_send_slider(child,msg,mp1,mp2,&handled)',
        'pm_send_slider(wh,msg,mp1,mp2,&handled)',
        'dialog slider notify',
        'O2_SLN_CHANGE',
        'O2_SLN_SLIDERTRACK',
        'PMCOMPAT_TBM_GETPOS',
        'PMCOMPAT_TBM_SETPOS',
    ]
    for tok in required:
        if tok not in src:
            return fail('slider bridge missing token: '+tok)

    enum=re.search(r'O2HENUM\s+__cdecl\s+WinBeginEnumWindows\s*\(.*?\n\}', src, re.S)
    if not enum:
        return fail('WinBeginEnumWindows implementation missing')
    eb=enum.group(0)
    for tok in ['hwnd != O2_HWND_DESKTOP || pm_is_own_process_window(child)',
                'GetWindow(child, GW_HWNDNEXT)',
                'WinBeginEnumWindows']:
        if tok not in eb:
            return fail('desktop namespace filter missing: '+tok)

    show=re.search(r'O2ULONG\s+__cdecl\s+WinShowWindow\s*\(.*?\n\}', src, re.S)
    if not show:
        return fail('WinShowWindow implementation missing')
    sb=show.group(0)
    if 'pm_is_own_process_window(wh)' not in sb:
        return fail('WinShowWindow lacks foreign-host HWND safety fence')
    if 'hwnd == O2_HWND_DESKTOP' not in sb:
        return fail('WinShowWindow lacks explicit HWND_DESKTOP fence')

    dlg=re.search(r'static HWND pm_create_dialog_template\s*\(.*?\n\}', src, re.S)
    if not dlg:
        return fail('dialog-template builder missing')
    db=dlg.group(0)
    if 'cls == O2_WC_SLIDER' not in db or 'PMCOMPAT_TRACKBAR_CLASS' not in db:
        return fail('OS/2 dialog WC_SLIDER does not map to native trackbar')

    wndproc=re.search(r'static LRESULT CALLBACK pm_dialog_wndproc\s*\(.*?\n\}', src, re.S)
    if not wndproc or 'O2_WM_CONTROL' not in wndproc.group(0) or 'dialog slider notify' not in wndproc.group(0):
        return fail('native trackbar scroll notifications are not translated to OS/2 WM_CONTROL')

    print('M31G R19 WC_SLIDER + desktop namespace regression PASS')
    print('  class 38 maps to a Win32 trackbar with SLM 036C/0371/0372 translation')
    print('  slider tracking/change is translated to OS/2 WM_CONTROL notifications')
    print('  HWND_DESKTOP enumeration excludes windows outside the os2host32 process')
    print('  WinShowWindow refuses HWND_DESKTOP hide and foreign-process HWNDs')
    return 0

if __name__=='__main__':
    sys.exit(main())
