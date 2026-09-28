#!/usr/bin/env python3
from pathlib import Path
import hashlib, re, sys

root = Path(__file__).resolve().parents[1]
def fail(msg):
    print('KBD-R2 static architecture check: FAIL:', msg)
    sys.exit(1)

def read(rel): return (root / rel).read_text(errors='replace')

def sha(rel): return hashlib.sha256((root / rel).read_bytes()).hexdigest()

expected_def_sha = '46e62bde47fa01017fcaf929a6c881720dbcc08649bb2dc30b376da05aa9a7ab'
# Corrected below on first run if baseline hash differs; fail makes drift explicit.
if sha('dlls/kbdcalls/kbdcalls.def') != expected_def_sha:
    fail('kbdcalls.def changed from frozen baseline: ' + sha('dlls/kbdcalls/kbdcalls.def'))

veneer = read('dlls/kbdcalls/kbdcalls.c')
common = read('common/kbd/os2_kbd.c')
backend = read('common/win32/os2_kbd_win32.c')
header = read('common/include/os2_kbd.h')
loader = read('loader/os2host32.c')

deftext = read('dlls/kbdcalls/kbdcalls.def')
exports = re.findall(r'^\s*(Kbd\w+)\s+@(\d+)\s+NONAME\s*$', deftext, re.M)
expected = [('KbdCharIn','4'),('KbdStringIn','9'),('KbdGetStatus','10'),
            ('KbdSetStatus','11'),('KbdFlushBuffer','13'),('KbdPeek','22')]
if exports != expected:
    fail('six frozen exports/ordinals changed: %r' % (exports,))
if '#include <windows.h>' in veneer or 'GetStdHandle' in veneer or 'ReadConsoleInputA' in veneer:
    fail('Win32 mechanics leaked into exported veneer')
if '#include <windows.h>' in common or 'INPUT_RECORD' in common or re.search(r'\bHANDLE\s+[A-Za-z_]', common):
    fail('Win32 types/mechanics leaked into common keyboard semantics')
for token in ['status.fsMask', 'os2_kbd_KbdStringIn', 'os2_kbd_try_char']:
    if token not in common:
        fail('common keyboard ownership missing token ' + token)
for token in ['ReadConsoleInputA', 'PeekConsoleInputA', 'FlushConsoleInputBuffer',
              'SetConsoleCtrlHandler', 'PeekNamedPipe', 'ReadFile', 'WriteConsoleA']:
    if token not in backend:
        fail('Win32 backend missing host mechanic ' + token)
for token in ['KBDCALLS",  4UL, "KbdCharIn"',
              'KBDCALLS",  9UL, "KbdStringIn"',
              'KBDCALLS", 13UL, "KbdFlushBuffer"',
              'KBDCALLS", 22UL, "KbdPeek"']:
    if token not in loader:
        fail('existing far16 descriptor/reference changed or missing: ' + token)
for name in [x[0] for x in expected]:
    if ('%s(' % name) not in veneer:
        fail('veneer missing ' + name)
if 'struct Os2KbdInfo status;' not in header:
    fail('KBDINFO status is not common session state')
print('KBD-R2 static architecture check: PASS (6 frozen exports)')
