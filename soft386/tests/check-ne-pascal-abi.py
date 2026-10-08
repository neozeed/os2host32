#!/usr/bin/env python3
"""Guard all implemented 16-bit DOSCALLS Pascal cleanup widths.

The names/prototypes are taken from sdk/os2h/bsedos16.h.  Widths are the
16-bit Pascal stack sizes of each formal argument (including four-byte far
pointers and LONG/ULONG), in declaration order.  Runtime synthetic NE tests
separately validate the actual SP restoration for the exercised APIs.
"""
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[2]
SOURCE = (ROOT/'soft386/src/soft386_os2.c').read_text()
HEADER = (ROOT/'sdk/os2h/bsedos16.h').read_text()
# ordinal: (16-bit SDK API name, argument byte widths in declaration order)
ABI = {
    5: ('DosExit', [2,2]),
    8: ('DosGetInfoSeg', [4,4]),
    14: ('DosSetSigHandler', [4,4,4,2,2]),
    33: ('DosGetDateTime', [4]),
    34: ('DosAllocSeg', [2,4,2]),
    38: ('DosReallocSeg', [2,2]),
    39: ('DosFreeSeg', [2]),
    41: ('DosGetHugeShift', [4]),
    43: ('DosCreateCSAlias', [2,4]),
    49: ('DosGetMachineMode', [4]),
    52: ('DosDevConfig', [4,2,2]),
    58: ('DosChgFilePtr', [2,4,2,4]),
    59: ('DosClose', [2]),
    60: ('DosDelete', [4,4]),
    61: ('DosDupHandle', [2,4]),
    68: ('DosNewSize', [2,4]),
    70: ('DosOpen', [4,4,4,4,2,2,2,4]),
    72: ('DosQCurDisk', [4,4]),
    75: ('DosQFileMode', [4,4,4]),
    77: ('DosQHandType', [2,4,4]),
    83: ('DosSetFileInfo', [2,2,4,2]),
    85: ('DosSetMaxFH', [2]),
    89: ('DosSetVec', [2,4,4]),
    91: ('DosGetEnv', [4,4]),
    92: ('DosGetVersion', [4]),
    94: ('DosGetPID', [4]),
    120: ('DosError', [2]),
    130: ('DosGetCp', [2,4,4]),
    137: ('DosRead', [2,4,2,4]),
    138: ('DosWrite', [2,4,2,4]),
    140: ('DosSemRequest', [4,4]),
    141: ('DosSemClear', [4]),
    142: ('DosSemWait', [4,4]),
    144: ('DosExecPgm', [4,2,2,4,4,4,4]),
}
body = SOURCE.split('static unsigned ne_dos_cleanup_bytes(uint32_t ord)', 1)[1]
body = body.split('static unsigned ne_import_cleanup_bytes', 1)[0]
entries = re.findall(r'case\s+(\d+)\s*:\s*return\s+(\d+)\s*;',body)
actual = {int(k): int(v) for k,v in entries}
assert len(actual)==len(entries), 'duplicate cleanup ordinals'
expected = {ordinal: sum(widths) for ordinal,(_,widths) in ABI.items()}
assert actual == expected, 'Pascal cleanup mismatch: expected=%r actual=%r' %(expected,actual)
for ordinal,(name,widths) in sorted(ABI.items()):
    assert re.search(r'\b'+name+r'\s*\(',HEADER), 'no SDK prototype for %s (%d)' %(name,ordinal)
    assert sum(widths) > 0 and all(w in (2,4) for w in widths)
assert 'default:return UINT_MAX;' in body, 'unsafe zero-byte fallback'
mod_body = SOURCE.split('static unsigned ne_import_cleanup_bytes(',1)[1].split('static uint16_t ne_alloc_segment',1)[0]
for module,ordinal,width in [('NLS',1,10),('NLS',4,10),('KBDCALLS',13,2),('MSG',2,26)]:
    assert ('"%s"'%module) in mod_body and ('ord==%du'%ordinal) in mod_body
    assert ('return %du'%width) in mod_body
assert 'return UINT_MAX;' in mod_body, 'other modules must not borrow DOSCALLS cleanup'
print('NE Pascal ABI audit PASS: 34 SDK DOSCALLS signatures/frames and 4 module-specific imports')
