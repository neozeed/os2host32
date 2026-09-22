#!/usr/bin/env python
from __future__ import print_function
import re, sys

def fail(msg):
    print('M31G R17 CHECK FAILED:', msg)
    return 1

def main():
    src=open('nls.c').read()
    ndef=open('nls.def').read()
    host=open('os2host32.c').read()

    if not re.search(r'^\s*DosQueryCtryInfo\s+@5\s+NONAME\s*$', ndef, re.M):
        return fail('NLS.5 DosQueryCtryInfo export missing')
    m=re.search(r'O2APIRET\s+__cdecl\s+DosQueryCtryInfo\s*\(\s*O2ULONG\s+cb\s*,\s*const\s+O2COUNTRYCODE\s*\*pcc\s*,\s*O2COUNTRYINFO\s*\*pci\s*,\s*O2ULONG\s*\*pcbActual\s*\)\s*\{(.*?)\n\}', src, re.S)
    if not m:
        return fail('DosQueryCtryInfo 4-argument 32-bit ABI missing')
    body=m.group(1)
    for tok in ['GetOEMCP()', 'LOCALE_ICOUNTRY', 'LOCALE_IDATE',
                'LOCALE_SCURRENCY', 'LOCALE_STHOUSAND', 'LOCALE_SDECIMAL',
                'LOCALE_SDATE', 'LOCALE_STIME', 'LOCALE_ICURRENCY',
                'LOCALE_ICURRDIGITS', 'LOCALE_ITIME', 'LOCALE_SLIST',
                'O2_ERROR_NO_COUNTRY_OR_CODEPAGE',
                'O2_ERROR_NLS_TABLE_TRUNCATED']:
        if tok not in src:
            return fail('real country-info mapping missing: '+tok)

    fields=['O2ULONG country;', 'O2ULONG codepage;', 'O2ULONG fsDateFmt;',
            'char szCurrency[5];', 'char szThousandsSeparator[2];',
            'char szDecimal[2];', 'char szDateSeparator[2];',
            'char szTimeSeparator[2];', 'O2UCHAR fsCurrencyFmt;',
            'O2UCHAR cDecimalPlace;', 'O2UCHAR fsTimeFmt;',
            'O2USHORT abReserved1[2];', 'char szDataSeparator[2];',
            'O2USHORT abReserved2[5];']
    for tok in fields:
        if tok not in src:
            return fail('32-bit COUNTRYINFO layout field missing: '+tok)

    # Preserve R16 loader behavior and, importantly, do not "fix" OS/2 module
    # lookup by silently searching beside the EXE. Bare module names are
    # resolved through OS2LIBPATH; a '.' element is how callers request cwd.
    for tok in ['libpath = getenv("OS2LIBPATH")', 'if (!module_has_dll_suffix(leaf))',
                'strcat(leaf, ".dll")']:
        if tok not in host:
            return fail('R16/guest module lookup regressed: '+tok)
    if 'g_main_guest_program' in re.search(r'static int locate_guest_module\(.*?\n\}', host, re.S).group(0):
        return fail('locate_guest_module must not silently add the EXE directory')

    print('M31G R17 NLS.5 regression PASS')
    print('  NLS.5 = DosQueryCtryInfo(ULONG, PCOUNTRYCODE, PCOUNTRYINFO, PULONG)')
    print('  current country/codepage map to host locale + OEM CP; foreign requests reject cleanly')
    print('  44-byte OS/2 32-bit COUNTRYINFO shape and truncation semantics retained')
    print('  NEKO module lookup remains OS2LIBPATH-driven; use a dot element for cwd')
    return 0

if __name__=='__main__':
    sys.exit(main())
