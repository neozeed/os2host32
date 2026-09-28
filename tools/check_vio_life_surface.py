#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
failures = []

def need(cond, msg):
    if not cond:
        failures.append(msg)

bseord = (ROOT / 'sdk/os2h/bseord.h').read_text(errors='replace')
defs = (ROOT / 'dlls/viocalls/viocalls.def').read_text(errors='replace')
doscalls_def = (ROOT / 'dlls/doscalls/doscalls.def').read_text(errors='replace')
doscalls = (ROOT / 'dlls/doscalls/doscalls.c').read_text(errors='replace')
doscalls_common = (ROOT / 'common/doscalls/os2_doscalls.c').read_text(errors='replace')
bridge_test_def = (ROOT / 'tests/legacy/c386-far16-vio-life-test.def').read_text(errors='replace')
loader = (ROOT / 'loader/os2host32.c').read_text(errors='replace')
facade = (ROOT / 'dlls/viocalls/viocalls.c').read_text(errors='replace')
common = (ROOT / 'common/vio/os2_vio.c').read_text(errors='replace')
backend = (ROOT / 'common/win32/os2_vio_win32.c').read_text(errors='replace')
kbd = (ROOT / 'dlls/kbdcalls/kbdcalls.c').read_text(errors='replace')
kbd_common = (ROOT / 'common/kbd/os2_kbd.c').read_text(errors='replace')
kbd_backend = (ROOT / 'common/win32/os2_kbd_win32.c').read_text(errors='replace')
life = (ROOT / 'tests/vio/lifeos2.c').read_text(errors='replace')

apis = {
    'VioScrollUp': (7, '{ 2, 4, 2, 2, 2, 2, 2, 0 }'),
    'VioGetCurPos': (9, '{ 2, 4, 4, 0, 0, 0, 0, 0 }'),
    'VioWrtCharStr': (13, '{ 2, 2, 2, 2, 4, 0, 0, 0 }'),
    'VioSetCurPos': (15, '{ 2, 2, 2, 0, 0, 0, 0, 0 }'),
    'VioWrtTTY': (19, '{ 2, 2, 4, 0, 0, 0, 0, 0 }'),
    'VioGetMode': (21, '{ 2, 4, 0, 0, 0, 0, 0, 0 }'),
    'VioReadCellStr': (24, '{ 2, 2, 2, 4, 4, 0, 0, 0 }'),
    'VioWrtNAttr': (26, '{ 2, 2, 2, 2, 4, 0, 0, 0 }'),
    'VioGetCurType': (27, '{ 2, 4, 0, 0, 0, 0, 0, 0 }'),
    'VioSetCurType': (32, '{ 2, 4, 0, 0, 0, 0, 0, 0 }'),
    'VioWrtCharStrAtt': (48, '{ 2, 4, 2, 2, 2, 4, 0, 0 }'),
    'VioWrtNCell': (52, '{ 2, 2, 2, 2, 4, 0, 0, 0 }'),
}

ord_macro = {
    'VioScrollUp': 'ORD_VIOSCROLLUP',
    'VioGetCurPos': 'ORD_VIOGETCURPOS',
    'VioWrtCharStr': 'ORD_VIOWRTCHARSTR',
    'VioSetCurPos': 'ORD_VIOSETCURPOS',
    'VioWrtTTY': 'ORD_VIOWRTTTY',
    'VioGetMode': 'ORD_VIOGETMODE',
    'VioReadCellStr': 'ORD_VIOREADCELLSTR',
    'VioWrtNAttr': 'ORD_VIOWRTNATTR',
    'VioGetCurType': 'ORD_VIOGETCURTYPE',
    'VioSetCurType': 'ORD_VIOSETCURTYPE',
    'VioWrtCharStrAtt': 'ORD_VIOWRTCHARSTRATT',
    'VioWrtNCell': 'ORD_VIOWRTNCELL',
}

for name, (ordinal, widths) in apis.items():
    macro = ord_macro[name]
    need(re.search(r'#define\s+%s\s+%d\b' % (macro, ordinal), bseord) is not None,
         '%s ordinal %d not confirmed by bseord.h' % (name, ordinal))
    need(re.search(r'^\s*%s\s+@%d\s+NONAME\s*$' % (name, ordinal), defs,
                   re.MULTILINE) is not None,
         '%s @%d missing from viocalls.def' % (name, ordinal))
    pattern = r'\{\s*"VIOCALLS",\s*%dUL,\s*"%s",\s*\d+,\s*%s\s*\}' % (
        ordinal, name, re.escape(widths))
    need(re.search(pattern, loader) is not None,
         '%s descriptor/frame widths missing' % name)
    need(name in facade, '%s public facade missing' % name)

life_calls = ['VioScrollUp', 'VioGetCurPos', 'VioSetCurPos', 'VioGetCurType',
              'VioSetCurType', 'VioWrtCharStr', 'VioWrtCharStrAtt', 'VioWrtTTY',
              'KbdCharIn', 'DosSleep']
for name in life_calls:
    need(name in life, 'Life acceptance source no longer uses expected %s' % name)

# Life is deliberately INCL_16, so its DosSleep is historical DOSCALLS.32,
# not the flat 32-bit DOSCALLS.229 entry.  The far16 bridge widens the ULONG
# argument and lands on the compatibility export, which reuses DosSleep.
need(re.search(r'#define\s+ORD_DOSSLEEP\s+32\b', bseord) is not None,
     'historical DosSleep ordinal 32 not confirmed by bseord.h')
need(re.search(r'^\s*Dos16Sleep\s+@32\s+NONAME\s*$', doscalls_def,
               re.MULTILINE) is not None,
     'DOSCALLS.32 Dos16Sleep compatibility export missing')
need('O2APIRET __cdecl Dos16Sleep(O2ULONG milliseconds)' in doscalls and
     'os2_dos_Dos16Sleep(dos_session(), milliseconds)' in doscalls and
     'return os2_dos_DosSleep(session, milliseconds);' in doscalls_common,
     'Dos16Sleep does not reuse the common DosSleep implementation')
need(re.search(r'\{\s*"DOSCALLS",\s*32UL,\s*"DosSleep",\s*1,\s*\{\s*4,', loader) is not None,
     'C/386 far16 descriptor for DOSCALLS.32 DosSleep is missing')
need('DOSSLEEP=DOSCALLS.32' in bridge_test_def,
     'C/386 Life bridge regression does not cover DOSCALLS.32')
need(re.search(r'\{\s*"DOSCALLS",\s*50UL,\s*"Dos16Beep",\s*2,\s*\{\s*2,\s*2,', loader) is not None,
     'C/386 far16 descriptor for DOSCALLS.50 Dos16Beep is missing')
need(re.search(r'\{\s*"KBDCALLS",\s*10UL,\s*"KbdGetStatus",\s*2,\s*\{\s*2,\s*4,', loader) is not None,
     'C/386 far16 descriptor for KBDCALLS.10 is missing')
need(re.search(r'\{\s*"KBDCALLS",\s*11UL,\s*"KbdSetStatus",\s*2,\s*\{\s*2,\s*4,', loader) is not None,
     'C/386 far16 descriptor for KBDCALLS.11 is missing')

need('#include <windows.h>' not in common,
     'common VIO semantics unexpectedly includes windows.h')
need('struct Os2VioCell' in (ROOT / 'common/include/os2_vio.h').read_text(errors='replace') and
     'cells_valid' in common and 'shadow_scroll_up' in common,
     'R2 common authoritative cell-image state is missing')
need('#include <windows.h>' not in facade,
     'public VIO facade unexpectedly includes windows.h')
need('ReadConsoleOutputCharacterA' in backend and
     'ReadConsoleOutputAttribute' in backend and 'win32_read_cells' in backend,
     'Win32 backend cannot seed/resynchronize common VIO cell state')
need('WriteConsoleOutputCharacterA' in backend,
     'Win32 direct-cell backend is not using WriteConsoleOutputCharacterA')
need('FillConsoleOutputAttribute' in backend,
     'Win32 attributed write backend is missing FillConsoleOutputAttribute')
need('WriteFile' in backend,
     'Win32 TTY backend no longer preserves stream WriteFile path')
need('OS2_KBD_IO_NOWAIT' in kbd_common and 'GetNumberOfConsoleInputEvents' in kbd_backend,
     'KbdCharIn IO_NOWAIT nonblocking path is missing')
need('info->fbStatus = OS2_KBD_STATUS_CHAR_IN' in kbd_common,
     'KbdCharIn does not mark returned key status')
need('event->ch_scan = (unsigned char)key->wVirtualScanCode' in kbd_backend and
     'info->chScan = event->ch_scan' in kbd_common,
     'KbdCharIn scan-code propagation is missing')
need('OS2_VIO_ERROR_INVALID_VIO_HANDLE' in common,
     'HVIO 0-only policy is not enforced in common VIO semantics')

if failures:
    for failure in failures:
        print('FAIL:', failure)
    sys.exit(1)
print('check_vio_life_surface: PASS')
