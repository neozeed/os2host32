#!/usr/bin/env python3
from pathlib import Path
import struct
import sys

root = Path(__file__).resolve().parent.parent
cmd = (root / 'cmd32os2.c').read_text(errors='replace')
hdr = (root / 'cmdos2.h').read_text(errors='replace')
win = (root / 'cmdos2_win32.c').read_text(errors='replace')
os2 = (root / 'cmdos2_os2.c').read_text(errors='replace')
dos = (root / 'doscalls.c').read_text(errors='replace')
defs = (root / 'doscalls.def').read_text(errors='replace')
doscalls = (root / 'doscalls.c').read_text(errors='replace')
imports = (root / 'analysis' / 'CMD_200_GA_IMPORTS.tsv').read_text(errors='replace')

checks = [
    ('historical CMD imports DOSQAPPTYPE.163', 'DOSCALLS\t163\tDOSQAPPTYPE\t2' in imports),
    ('DOSCALLS keeps ordinal 163 compatibility export', 'DosQAppType          @163 NONAME' in defs),
    ('DOSCALLS exports 32-bit ordinal 323', 'DosQueryAppType      @323 NONAME' in defs),
    ('DOSCALLS.323 wrapper shares classifier', 'DosQueryAppType(const char *path, O2ULONG *appType)' in doscalls and 'return DosQAppType(path, appType);' in doscalls),
    ('DOSCALLS decodes LE/LX PM flags', 'flags >> 8' in dos and "hdr[0] == 'L'" in dos),
    ('CMD boundary exposes app type query', 'CmdO2QueryAppType' in hdr),
    ('native backend binds DOSCALLS.163', 'PFN_QAPPTYPE' in win and 'PFN_QAPPTYPE, 163' in win),
    ('OS/2 backend calls historical DosQAppType', 'DosQAppType((PSZ)program' in os2),
    ('plain sync execution auto-routes WINDOWAPI', 'start_pm_resolved(typed_program, program, tail)' in cmd),
    ('auto route is sync-only', 'exec_flag == CMDO2_EXEC_SYNC && !keep_pm_console' in cmd),
    ('START /PMC override exists', 'ci_cmp(opt, "PMC") == 0' in cmd and 'PM attached to this console' in cmd),
    ('PMC uses synchronous DosExecPgm helper', 'direct_rc = exec_resolved_mode(typed_program, program, args,' in cmd),
    ('VER no longer claims M29P', 'OS/2 CMD32 personality bootstrap - M31G QoL' in cmd and 'bootstrap - Milestone 29P' not in cmd),
]

failed = [name for name, ok in checks if not ok]

# Prove the preserved PM regression binaries are actually tagged WINDOWAPI in
# their LE/LX module flags.  This also pins the exact header interpretation.
examples = [
    root / 'examples/m31a-wmchar/WMCHAR.EXE',
    root / 'examples/m31c-hanoi/HANOI.EXE',
    root / 'examples/m31d-bio/BIO.EXE',
    root / 'examples/m31f-jigsaw/JIGSAW.EXE',
    root / 'examples/m31g-neko/NEKO.EXE',
]
for path in examples:
    data = path.read_bytes()
    if len(data) < 0x40 or data[:2] != b'MZ':
        failed.append(f'{path.name}: missing MZ header')
        continue
    off = struct.unpack_from('<I', data, 0x3c)[0]
    if off + 0x14 > len(data) or data[off:off+2] not in (b'LE', b'LX'):
        failed.append(f'{path.name}: missing LE/LX header')
        continue
    flags = struct.unpack_from('<I', data, off + 0x10)[0]
    apptype = (flags >> 8) & 3
    if apptype != 3:
        failed.append(f'{path.name}: expected WINDOWAPI, flags=0x{flags:08X}')

if failed:
    for item in failed:
        print('FAIL:', item)
    sys.exit(1)

print('CMD32 PM auto-session regression PASS')
print('  DOSCALLS.163 compatibility + DOSCALLS.323 32-bit DosQueryAppType exported')
print('  plain WINDOWAPI executables route through START /PM semantics')
print('  START /PMC preserves synchronous attached-console execution')
print('  WMCHAR/HANOI/BIO/JIGSAW/NEKO all verify as WINDOWAPI images')
