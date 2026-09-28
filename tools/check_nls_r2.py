#!/usr/bin/env python3
from pathlib import Path
import hashlib
import sys

root = Path(__file__).resolve().parents[1]
errors = []

def text(path):
    return (root / path).read_text(encoding="utf-8", errors="replace")

def require(cond, message):
    if not cond:
        errors.append(message)

nls_h = text("common/include/os2_nls.h")
backend_h = text("common/include/os2_nls_backend.h")
nls_c = text("common/nls/os2_nls.c")
dos_h = text("common/include/os2_doscalls.h")
dos_c = text("common/doscalls/os2_doscalls.c")
win_dos = text("common/win32/os2_doscalls_win32.c")
win_services = text("common/win32/os2_win32_services.c")
win_nls = text("common/win32/os2_nls_win32.c")
whp = text("whp/src/whp_os2_v2_hi.c")

require("backend_opaque" in nls_h and "const struct Os2NlsBackendOps *backend" in nls_h,
        "Os2NlsState must carry explicit backend binding")
require("os2_nls_session_init" in nls_h and "os2_nls_session_init" in nls_c,
        "common NLS must expose state/session initialization")
require("query_initial_profile" in backend_h,
        "NLS backend must be limited to an explicit bootstrap profile service")
require("GetLocaleInfoA" in win_nls and "GetOEMCP" in win_nls,
        "Win32 NLS bootstrap mechanics must live in os2_nls_win32.c")
require("GetLocaleInfoA" not in nls_c and "GetOEMCP" not in nls_c,
        "common NLS must not call Win32 locale APIs")
require("initialize_nls" not in win_services,
        "generic Win32 services must not own NLS initialization")
require("struct Os2NlsState nls;" in dos_h,
        "Os2DosSession must own per-process NLS state")
for api in ("DosSetProcessCp", "DosQueryCp", "DosQueryCtryInfo",
            "DosQueryDBCSEnv", "DosMapCase"):
    require(("os2_dos_" + api) in dos_c,
            api + " must terminate in common DOS/NLS semantics")
    require(("OS2_DOS_CALL_" + api.upper()) not in win_dos,
            api + " must not remain in Win32 DOS dispatch")
require("static struct Os2NlsState" not in win_dos,
        "Win32 DOS backend must not own a separate NLS state")
require("os2_nls_win32_init_session(&session->nls)" in win_dos,
        "Win32 DOS session must bootstrap its common NLS state explicitly")
require("os2_nls_win32_init_session(&rt.nls_state)" in whp,
        "WHP must use the explicit NLS backend bootstrap seam")

# Frozen public NLS ABI.
def_bytes = (root / "dlls/nls/nls.def").read_bytes()
require(hashlib.sha256(def_bytes).hexdigest() ==
        "8e1b567043be8767d6c93eee386ad21c57270eaa8421e8639315a874e998aef4",
        "nls.def changed; public NLS ordinals must remain frozen")

if errors:
    for error in errors:
        print("FAIL:", error)
    sys.exit(1)
print("nls-static-check: PASS")
