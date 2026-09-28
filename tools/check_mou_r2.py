#!/usr/bin/env python3
from pathlib import Path
import re, sys

root = Path(__file__).resolve().parents[1]
fail = []

def need(cond, msg):
    if not cond:
        fail.append(msg)

expected = [
    ("MouGetPtrShape",1),("MouSetPtrShape",2),("MouGetNumMickeys",3),
    ("MouGetThreshold",4),("MouGetScaleFact",6),("MouFlushQue",7),
    ("MouGetNumButtons",8),("MouClose",9),("MouSetThreshold",10),
    ("MouSetScaleFact",11),("MouGetNumQueEl",13),("MouDeRegister",14),
    ("MouGetEventMask",15),("MouSetEventMask",16),("MouOpen",17),
    ("MouRemovePtr",18),("MouGetPtrPos",19),("MouReadEventQue",20),
    ("MouSetPtrPos",21),("MouGetDevStatus",22),("MouSynch",23),
    ("MouRegister",24),("MouSetDevStatus",25),("MouDrawPtr",26),
    ("MouInitReal",27),
]

def_text=(root/'dlls/moucalls/moucalls.def').read_text()
for name,ordv in expected:
    need(re.search(r'^\s*%s\s+@%d\s+NONAME\s*$' % (name,ordv), def_text, re.M),
         '%s @%d missing from moucalls.def' % (name,ordv))
need(len(re.findall(r'^\s*Mou\w+\s+@\d+\s+NONAME\s*$', def_text, re.M)) == len(expected),
     'unexpected MOUCALLS export count')

common=(root/'common/mou/os2_mou.c').read_text()
header=(root/'common/include/os2_mou.h').read_text()
backend=(root/'common/win32/os2_mou_win32.c').read_text()
veneer=(root/'dlls/moucalls/moucalls.c').read_text()
loader=(root/'loader/os2host32.c').read_text()
make=(root/'Makefile').read_text()
catalog=(root/'common/api/os2_api_catalog.inc').read_text()

for token in ('windows.h','GetCursorPos','GetAsyncKeyState','SetCursorPos'):
    need(token not in common, 'Win32 token leaked into common mouse semantics: '+token)
need(re.search(r'\bHANDLE\b|\bHWND\b', common) is None,
     'Win32 handle/window type leaked into common mouse semantics')
need('struct Os2MouSession' in header and 'events[OS2_MOU_MAX_EVENTS]' in header,
     'common mouse session/event queue missing')
need('os2_mou_try_read' in common and 'os2_mou_try_read' in header,
     'nonblocking MOU scheduler primitive missing')
need('GetCursorPos' in backend and 'GetAsyncKeyState' in backend,
     'Win32 backend is not host pointer/button adapter')
need('ReadConsoleInput' not in backend and 'PeekConsoleInput' not in backend,
     'MOU backend must not consume KBD console INPUT_RECORD stream')
need('os2_mou_' in veneer and 'GetCursorPos' not in veneer,
     'MOUCALLS veneer is not thin/common-routed')
need('MOUCALLS.dll' in make and 'mou-win32-shim-check' in make,
     'Makefile MOU wiring/checks missing')
need('"MOUCALLS"' in loader and 'MouReadEventQue' in loader,
     'loader MOUCALLS personality/far16 wiring missing')
for name,ordv in expected:
    need(re.search(r'\{\s*"MOUCALLS",\s*%dUL,\s*"%s"' % (ordv, name), loader) is not None,
         'far16 descriptor missing for %s.%d' % (name, ordv))
for name,ordv in expected:
    need(('OS2_API("MOUCALLS", %du, "%s"' % (ordv,name)) in catalog,
         'API catalog missing %s.%d' % (name,ordv))

# Registration routing is intentionally not implemented in this milestone.
need('OS2_MOU_ERROR_MOUSE_REGISTER' in common and
     'OS2_MOU_ERROR_MOUSE_DEREGISTER' in common,
     'MouRegister/MouDeRegister limitation is not explicit')

if fail:
    for msg in fail:
        print('FAIL:', msg)
    sys.exit(1)
print('mou-static-check: PASS')
