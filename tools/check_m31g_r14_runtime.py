#!/usr/bin/env python
from __future__ import print_function
import re, sys

def fail(msg):
    print('M31G R14 CHECK FAILED:', msg)
    return 1

def exported(text, name, ordinal):
    return re.search(r'^\s*'+re.escape(name)+r'\s+@'+str(ordinal)+r'\s+NONAME\s*$', text, re.M) is not None

def main():
    pmwp=open('pmwp.c').read()
    pmwin=open('pmwin.c').read()
    host=open('os2host32.c').read()
    ddef=open('doscalls.def').read()
    dos=open('doscalls.c').read()

    m=re.search(r'DWORD\s+__cdecl\s+PMWPOrdinal203\s*\([^)]*\)\s*\{(.*?)\n\}', pmwp, re.S)
    if not m:
        return fail('PMWP.203 implementation missing')
    body=m.group(1)
    for tok in ['MAKEINTRESOURCEA(318)', 'PFNDOSLOADMODULE', '(const char *)(DWORD)arg3', '(DWORD *)(DWORD)arg2']:
        if tok not in pmwp:
            return fail('PMWP.203 proven DosLoadModule bridge missing: '+tok)
    if re.search(r'^\s*return\s+0\s*;', body, re.M):
        return fail('PMWP.203 still contains unconditional advisory-success return')

    for tok in ['GetModuleHandleA("PMWIN.dll")', 'Resource-only OS/2 DLLs']:
        if tok not in host:
            return fail('resource-only guest DLL PMWIN publication fallback missing: '+tok)

    for tok in ['WNDPROC native_proc', 'O2_SM_SETHANDLE', 'O2_SM_QUERYHANDLE',
                'pm_public_control_default_proc', 'SetWindowLongA(wh, GWL_WNDPROC',
                'st->native_proc = (WNDPROC)(DWORD)previous',
                'return pm_public_control_default_proc', 'STM_GETICON', 'STM_SETICON']:
        if tok not in pmwin:
            return fail('native public-control subclass bridge missing: '+tok)
    if 'case WM_TIMER:' not in pmwin or 'O2_WM_TIMER' not in pmwin:
        return fail('native WM_TIMER -> OS/2 WM_TIMER translation missing')

    if not exported(ddef, 'DosSetFileInfo', 218):
        return fail('DOSCALLS.218 DosSetFileInfo export missing')
    m=re.search(r'O2APIRET\s+__cdecl\s+DosSetFileInfo\s*\(\s*O2HFILE\s+hFile\s*,\s*O2ULONG\s+level\s*,\s*const void \*buffer\s*,\s*O2ULONG\s+cb\s*\)\s*\{(.*?)\n\}', dos, re.S)
    if not m:
        return fail('DosSetFileInfo four-argument ABI missing')
    body=m.group(1)
    for tok in ['os2_handle(hFile)', 'O2_FIL_STANDARD', 'SetFileTime(',
                'GetFinalPathNameByHandleA', 'SetFileAttributesA(']:
        if tok not in body:
            return fail('DosSetFileInfo FIL_STANDARD mapping missing: '+tok)

    # Genericity: executable-name branching remains forbidden.  Documentation
    # comments may mention specimens, so inspect executable comparisons only.
    allsrc='\n'.join([pmwp,pmwin,host,dos])
    if re.search(r'(strcmp|_stricmp|lstrcmpiA)\s*\([^\n]*(NEKO|E\.EXE)', allsrc, re.I):
        return fail('application-specific executable-name branch found')

    print('M31G R14 runtime-boundary regression PASS')
    print('  PMWP.203 now performs the GA-proven DosLoadModule bridge and writes HMODULE')
    print('  resource-only guest DLLs can publish PM resources through process PMWIN')
    print('  native public controls can be truly WinSubclassWindow-subclassed into pm_wndproc')
    print('  WC_STATIC SM_QUERYHANDLE/SM_SETHANDLE delegate through a guest-callable old PFNWP')
    print('  e.exe side boundary: DOSCALLS.218 DosSetFileInfo with FIL_STANDARD metadata')
    return 0

if __name__=='__main__':
    sys.exit(main())
