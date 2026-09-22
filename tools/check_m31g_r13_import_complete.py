#!/usr/bin/env python
from __future__ import print_function
import re, subprocess, sys


def fail(msg):
    print('M31G R13 CHECK FAILED:', msg)
    return 1


def exported(text, name, ordinal):
    pat = r'^\s*' + re.escape(name) + r'\s+@' + str(ordinal) + r'\s+NONAME\s*$'
    return re.search(pat, text, re.M) is not None


def main():
    pwdef = open('pmwin.def').read(); pwc = open('pmwin.c').read()
    pgdef = open('pmgpi.def').read(); pgc = open('pmgpi.c').read()
    psdef = open('pmshapi.def').read(); psc = open('pmshapi.c').read()
    hmdef = open('helpmgr.def').read(); hmc = open('helpmgr.c').read()
    dddef = open('doscalls.def').read(); ddc = open('doscalls.c').read()

    pmwin = [
        ('WinBeginEnumWindows', 702), ('WinDrawBitmap', 730),
        ('WinEndEnumWindows', 737), ('WinGetNextWindow', 756),
        ('WinIsWindowVisible', 775), ('WinQueryPointerInfo', 822),
        ('WinQueryPointerPos', 823), ('WinSetSysModalWindow', 872),
    ]
    for name, ordinal in pmwin:
        if not exported(pwdef, name, ordinal):
            return fail('PMWIN.%d %s export missing' % (ordinal, name))
    for tok in ['PMCOMPAT_ENUM_MAGIC', 'GetWindow(parent, GW_CHILD)',
                'StretchBlt(', 'IsWindowVisible(', 'GetIconInfo(',
                'GetCursorPos(', 'g_sys_modal_hwnd']:
        if tok not in pwc:
            return fail('PMWIN semantic path missing: ' + tok)
    if 'OS2PM_QueryResource' not in pwdef or 'OS2PM_QueryResource' not in pwc:
        return fail('named PMWIN resource bridge missing')

    if not exported(pgdef, 'GpiLoadBitmap', 399):
        return fail('PMGPI.399 GpiLoadBitmap export missing')
    for tok in ['O2HBITMAP __cdecl GpiLoadBitmap', 'OS2PM_QueryResource',
                'O2_RT_BITMAP', 'O2_BFT_BITMAPARRAY', 'GpiCreateBitmap(',
                'gpi_apply_resource_palette', 'StretchBlt(']:
        if tok not in pgc:
            return fail('GpiLoadBitmap semantic path missing: ' + tok)

    for name, ordinal in [('PrfQueryProfileData',117),
                          ('PrfWriteProfileData',118)]:
        if not exported(psdef, name, ordinal):
            return fail('PMSHAPI.%d %s export missing' % (ordinal, name))
    for tok in ['@HEX:', 'pmsh_profile_path', 'WritePrivateProfileStringA']:
        if tok not in psc:
            return fail('binary profile bridge missing: ' + tok)

    for name, ordinal in [('WinCreateHelpInstance',51),
                          ('WinDestroyHelpInstance',52),
                          ('WinAssociateHelpInstance',54)]:
        if not exported(hmdef, name, ordinal):
            return fail('HELPMGR.%d %s export missing' % (ordinal, name))
    for tok in ['O2HELPINIT', 'ulReturnCode = 0', 'g_next_help_instance',
                'associated = hwndApp']:
        if tok not in hmc:
            return fail('HELPMGR lifetime semantics missing: ' + tok)

    if not exported(dddef, 'DosError', 212):
        return fail('DOSCALLS.212 DosError export missing')
    fn = re.search(r'O2APIRET\s+__cdecl\s+DosError\s*\(\s*O2ULONG\s+flags\s*\)\s*\{(.*?)\n\}', ddc, re.S)
    if not fn:
        return fail('DosError(ULONG) ABI missing')
    if 'flags & ~3UL' not in fn.group(1) or 'O2_ERROR_INVALID_PARAMETER' not in fn.group(1):
        return fail('DosError flag validation missing')

    combined = '\n'.join([pwc, pgc, psc, hmc, ddc]).upper()
    if 'NEKO.EXE' in combined or 'E.EXE' in combined:
        return fail('application-specific behavior found')

    try:
        out = subprocess.check_output([sys.executable, 'tools/check_m31g_neko.py'],
                                      stderr=subprocess.STDOUT)
        if not isinstance(out, str):
            out = out.decode('utf-8', 'replace')
    except subprocess.CalledProcessError as exc:
        out = exc.output
        if not isinstance(out, str):
            out = out.decode('utf-8', 'replace')
        print(out)
        return fail('NEKO base/import audit failed')
    if '0 remain outside current .def coverage' not in out:
        print(out)
        return fail('NEKO is not import-complete')

    print('M31G R13 import-complete regression PASS')
    print('  NEKO: all 63 distinct ordinal imports are covered')
    print('  PMWIN: 702/730/737/756/775/822/823/872')
    print('  PMGPI: 399 GpiLoadBitmap with OS/2 RT_BITMAP decoding')
    print('  PMSHAPI: 117/118 binary profile data')
    print('  HELPMGR: 51/52/54 create/destroy/associate lifetime')
    print('  e.exe side boundary: DOSCALLS.212 DosError')
    return 0

if __name__ == '__main__':
    sys.exit(main())
