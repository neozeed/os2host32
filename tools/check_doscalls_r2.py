#!/usr/bin/env python3
"""Static architecture checks for the DOSCALLS R2 split."""
from pathlib import Path
import hashlib
import re
import sys

root = Path(__file__).resolve().parents[1]
veneer = (root / "dlls/doscalls/doscalls.c").read_text()
def_text = (root / "dlls/doscalls/doscalls.def").read_bytes()
common_h = (root / "common/include/os2_doscalls.h").read_text()
backend_h = (root / "common/include/os2_doscalls_backend.h").read_text()
common_c = (root / "common/doscalls/os2_doscalls.c").read_text()
win32_c = (root / "common/win32/os2_doscalls_win32.c").read_text()
makefile = (root / "Makefile").read_text()

errors = []

def need(cond, msg):
    if not cond:
        errors.append(msg)

need(hashlib.sha256(def_text).hexdigest() ==
     "1675df8a6723f40a1344b90c9ebeb398312b9439cb8000008e3ff903ccb7d4c1",
     "DOSCALLS .def changed; historical export names/ordinals must remain frozen")
need("windows.h" not in veneer.lower(), "public DOSCALLS veneer includes windows.h")
for api in ("GetStdHandle", "CreateFileA", "CloseHandle", "VirtualAlloc",
            "CreateProcessA", "CreateEventA", "WaitForSingleObject"):
    need(api not in veneer, "public DOSCALLS veneer contains Win32 API %s" % api)
need("DosFlatToSel" in veneer and "DosSelToFlat" in veneer and
     '__asm__ __volatile__("ret")' in veneer,
     "selector-token register ABI helpers were not preserved in the veneer")
need("os2_doscalls_win32_session()" in veneer,
     "public veneer is not bound to the backend-neutral DOS session")

exports = []
for raw in def_text.decode("ascii").splitlines():
    line = raw.strip()
    if not line or line in ("LIBRARY DOSCALLS", "EXPORTS"):
        continue
    exports.append(line.split()[0])
for name in exports:
    need(re.search(r"\b%s\s*\(" % re.escape(name), veneer) is not None,
         "export %s is missing from the public veneer" % name)

for field in ("file_handles", "std_handles", "find_handles", "children",
              "events", "subpools", "exception_head",
              "signal_exception_focus_count", "error_flags", "max_file_handles",
              "struct Os2NlsState nls"):
    need(field in common_h, "Os2DosSession is missing common state field %s" % field)
for fn in ("os2_dos_alloc_hfile", "os2_dos_alloc_find_handle",
           "os2_dos_store_child", "os2_dos_DosCreateEventSem",
           "os2_dos_DosSubAllocMem", "os2_dos_DosSetExceptionHandler",
           "os2_dos_DosSetSigHandler", "os2_dos_DosSetVec",
           "os2_dos_DosSetRelMaxFH"):
    need(fn in common_c, "common DOSCALLS semantics missing %s" % fn)
need("#include <windows.h>" not in common_c and "#include <windows.h>" not in common_h,
     "common DOSCALLS layer has a Win32 header dependency")

for legacy in ("static HANDLE o2_handles[", "static HANDLE o2_std_handles[",
               "static HANDLE o2_find_handles[", "static struct O2ChildProc o2_children[",
               "struct O2SubPool", "struct O2SubRange", "o2_subpools",
               "o2_exception_head", "o2_signal_exception_focus_count"):
    need(legacy not in win32_c,
         "Win32 backend still owns duplicate common DOSCALLS state: %s" % legacy)

# DosGetInfoBlocks still needs concrete native TIB/PIB presentation layouts in
# the backend.  They are not personality ownership; only the exception head is
# common-owned.  Catch the R2 packaging bug where the declarations survived
# but their complete definitions were accidentally removed.
for layout in ("struct O2TibCompat {", "struct O2Tib2Compat {",
               "struct O2PibCompat {"):
    need(layout in win32_c,
         "Win32 backend is missing complete DosGetInfoBlocks layout: %s" % layout)

# APIs whose semantics/state moved fully to common code must not be routed back
# through the transitional backend dispatch seam.
for call_id in ("OS2_DOS_CALL_DOSSUBSETMEM", "OS2_DOS_CALL_DOSSUBALLOCMEM",
                "OS2_DOS_CALL_DOSSUBFREEMEM", "OS2_DOS_CALL_DOSSUBUNSETMEM",
                "OS2_DOS_CALL_DOSSETEXCEPTIONHANDLER",
                "OS2_DOS_CALL_DOSUNSETEXCEPTIONHANDLER",
                "OS2_DOS_CALL_DOSSETSIGNALEXCEPTIONFOCUS",
                "OS2_DOS_CALL_DOSSETRELMAXFH",
                "OS2_DOS_CALL_DOSACKNOWLEDGESIGNALEXCEPTION",
                "OS2_DOS_CALL_DOSSETPROCESSCP",
                "OS2_DOS_CALL_DOSQUERYCP",
                "OS2_DOS_CALL_DOSQUERYCTRYINFO",
                "OS2_DOS_CALL_DOSQUERYDBCSENV",
                "OS2_DOS_CALL_DOSMAPCASE"):
    need(("case " + call_id + ":") not in win32_c,
         "common-owned DOSCALLS API still has a dead backend dispatch case: %s" % call_id)
for helper in ("os2_dos_resolve_hfile", "os2_dos_alloc_hfile",
               "os2_dos_alloc_find_handle", "os2_dos_store_child",
               "os2_dos_take_child", "os2_dos_replace_hfile"):
    need(helper in win32_c, "Win32 backend is not using common helper %s" % helper)
need("static const struct Os2DosBackendOps r2_backend_ops" in win32_c,
     "Win32 DOSCALLS backend table is missing")
need("OS2_DOS_CALL_COUNT" in backend_h and "dispatch" in backend_h,
     "typed transitional DOSCALLS dispatch seam is missing")

for token in ("DOSCALLS_SESSION_SRC = common/doscalls/os2_doscalls.c",
              "DOSCALLS_WIN32_SRC = common/win32/os2_doscalls_win32.c",
              "doscalls-check", "doscalls-static-check"):
    need(token in makefile, "Makefile missing DOSCALLS R2 wiring: %s" % token)

loader = (root / "loader/os2host32.c").read_text()
imports = (root / "tests/doscalls/c386-signal-imports.def").read_text()
need(re.search(r'\{\s*"DOSCALLS",\s*14UL,\s*"DosSetSigHandler",\s*5,\s*\{\s*2,\s*2,\s*4,\s*4,\s*4', loader) is not None,
     "Microsoft C/386 DosSetSigHandler far16 descriptor is missing")
need(re.search(r'\{\s*"DOSCALLS",\s*89UL,\s*"DosSetVec",\s*3,\s*\{\s*4,\s*4,\s*2', loader) is not None,
     "Microsoft C/386 DosSetVec far16 descriptor is missing")
need("SYSSETSIGHANDLER=DOSCALLS.14" in imports and
     "SYSSETVEC=DOSCALLS.89" in imports,
     "Microsoft C/386 signal.asm SYS* import aliases are missing")

if errors:
    for e in errors:
        print("DOSCALLS-R2 CHECK FAIL: " + e, file=sys.stderr)
    sys.exit(1)
print("DOSCALLS-R2 static architecture check: PASS (%d frozen exports)" % len(exports))
