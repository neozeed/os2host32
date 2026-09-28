#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]

def read(rel):
    return (ROOT / rel).read_text(encoding='utf-8', errors='replace')

def need(cond, msg):
    if not cond:
        print('c386-hack-bridge-check: FAIL: ' + msg)
        sys.exit(1)

loader = read('loader/os2host32.c')
dosc = read('dlls/doscalls/doscalls.c')
catalog = read('common/api/os2_api_catalog.inc')
viodef = read('dlls/viocalls/viocalls.def')
dosdef = read('dlls/doscalls/doscalls.def')
kbddef = read('dlls/kbdcalls/kbdcalls.def')
vioh = read('common/include/os2_vio.h')
vioc = read('common/vio/os2_vio.c')
viow = read('common/win32/os2_vio_win32.c')

# These are the C/386 migration APIs exposed by the hack.exe scan which were
# not covered by the pre-HACK descriptor set.  Widths are low-to-high Pascal
# frame order (rightmost C source argument first).
descs = [
    ('DOSCALLS', 50, 'Dos16Beep',       [2, 2]),
    ('KBDCALLS', 10, 'KbdGetStatus',    [2, 4]),
    ('KBDCALLS', 11, 'KbdSetStatus',    [2, 4]),
    ('VIOCALLS', 24, 'VioReadCellStr',  [2, 2, 2, 4, 4]),
    ('VIOCALLS', 26, 'VioWrtNAttr',     [2, 2, 2, 2, 4]),
    ('VIOCALLS', 52, 'VioWrtNCell',     [2, 2, 2, 2, 4]),
]

for module, ordinal, name, widths in descs:
    prefix = r'\{\s*"%s"\s*,\s*%dUL\s*,\s*"%s"\s*,\s*%d\s*,\s*\{\s*' % (
        re.escape(module), ordinal, re.escape(name), len(widths))
    width_pat = r'\s*,\s*'.join(str(x) for x in widths)
    need(re.search(prefix + width_pat, loader) is not None,
         '%s.%d %s descriptor/widths missing or changed' % (module, ordinal, name))

# DosSelToFlat is deliberately a register-ABI helper, not a Pascal frame API.
need(re.search(r'DosSelToFlat\s+@426\s+NONAME', dosdef, re.I) is not None,
     'DOSCALLS.426 DosSelToFlat export missing')
need('DosSelToFlat' in dosc, 'DOSCALLS veneer no longer implements DosSelToFlat register helper')
need(re.search(r'OS2_API\("DOSCALLS"\s*,\s*426u\s*,\s*"DosSelToFlat"', catalog) is not None,
     'API catalogue no longer exposes DOSCALLS.426 DosSelToFlat')
need(re.search(r'\{\s*"DOSCALLS"\s*,\s*426UL\s*,', loader) is None,
     'DosSelToFlat must not be turned into an ordinary far16 Pascal descriptor')

# Existing target exports used by the new descriptors.
need(re.search(r'Dos16Beep\s+@50\s+NONAME', dosdef, re.I) is not None,
     'DOSCALLS.50 Dos16Beep export missing')
need(re.search(r'KbdGetStatus\s+@10\s+NONAME', kbddef, re.I) is not None,
     'KBDCALLS.10 KbdGetStatus export missing')
need(re.search(r'KbdSetStatus\s+@11\s+NONAME', kbddef, re.I) is not None,
     'KBDCALLS.11 KbdSetStatus export missing')
for ordinal, name in [(24, 'VioReadCellStr'), (26, 'VioWrtNAttr'), (52, 'VioWrtNCell')]:
    need(re.search(r'%s\s+@%d\s+NONAME' % (name, ordinal), viodef, re.I) is not None,
         'VIOCALLS.%d %s export missing' % (ordinal, name))

# The VIO targets must be real common/backend operations, not success stubs.
for symbol in ['os2_vio_read_cell_str', 'os2_vio_write_n_attr', 'os2_vio_write_n_cell']:
    need(symbol in vioh, symbol + ' public common declaration missing')
    need(symbol in vioc, symbol + ' common implementation missing')
need('FillConsoleOutputAttribute' in viow,
     'Win32 VIO backend no longer implements attribute fill')
need('FillConsoleOutputCharacterA' in viow,
     'Win32 VIO backend no longer implements repeated-cell character fill')

# Fail closed remains an architectural requirement: recognized migration
# thunks are patched; arbitrary 16-bit LE/LX code is not executed.
need('supported via native far16 bridges' in loader,
     'native far16 bridge scan status missing')
need('needs additional execution machinery' in loader,
     'fail-closed unsupported 16-bit scan status missing')
need('The 16-bit thunk is metadata only and is never entered' in loader,
     'metadata-only C/386 bridge invariant comment missing')


# Relocated C/386 code can contain compiler-generated absolute switch tables
# whose table-reference operand has an LE OFF32 fixup but whose entries do not.
# Keep the repair narrow and tied to recognized indirect CS: jump patterns.
for token in ('preferred_exec_address_to_actual',
              'c386_switch_operand_pattern',
              'maybe_relocate_c386_switch_table',
              'C/386 switch tbl'):
    need(token in loader, 'missing C/386 switch-table relocation support: ' + token)
need('code[source_off - 4UL] == 0x2e' in loader and
     'code[source_off - 3UL] == 0xff' in loader and
     'code[source_off - 2UL] == 0x24' in loader,
     'C/386 indexed switch-jump signature guard missing')
need('count < 2UL' in loader,
     'C/386 switch repair lost minimum-table-size guard')


# Microsoft C/386 LIBC signal.asm has its own direct 32-bit Pascal ABI for
# SYSSETSIGHANDLER/SYSSETVEC: 32-bit slots, left-to-right pushes, callee
# cleanup.  Keep loader-local adapters so the DOSCALLS exports remain cdecl.
for token in ('make_c386_pascal32_signal_adapter',
              'c386_direct_import_target',
              'c386_pascal32_sig_stub',
              'c386_pascal32_vec_stub',
              'C/386 signal ABI'):
    need(token in loader, 'missing C/386 signal.asm Pascal32 adapter support: ' + token)
need('ordinal == 14UL' in loader and 'ordinal == 89UL' in loader,
     'C/386 signal adapter is not restricted to DOSCALLS.14/.89')
need('src_obj->mapped[source_off - 1UL] != 0xe8' in loader,
     'C/386 signal adapter lost direct CALL-site guard')
need('frame_off = 4UL' in loader and 'frame_off += 4UL' in loader,
     'C/386 signal adapter no longer treats every CRT argument as a 32-bit slot')
need('stub[p++] = 0xc2' in loader,
     'C/386 signal adapter lost callee-cleanup RET n')

print('c386-hack-bridge-check: PASS (far16 coverage, switch relocation, signal.asm Pascal32 adapters)')
