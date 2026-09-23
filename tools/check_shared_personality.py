#!/usr/bin/env python3
"""Static wiring checks for the shared native/WHP OS/2 personality layer.

This is intentionally toolchain-independent: it catches a common failure mode
where an API is marked shared in the ordinal catalogue but one loader silently
falls back to a private implementation.
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SHARED_DOSCALLS = {
    224: "DosQueryHType",
    230: "DosGetDateTime",
    256: "DosSetFilePtr",
    282: "DosWrite",
    299: "DosAllocMem",
    304: "DosFreeMem",
    305: "DosSetMem",
    348: "DosQuerySysInfo",
}


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8", errors="replace")


def exported_function_body(source: str, name: str) -> str | None:
    match = re.search(
        rf"O2APIRET\s+__cdecl\s+{re.escape(name)}\s*\([^)]*\)\s*\{{",
        source,
        re.MULTILINE,
    )
    if match is None:
        return None
    start = match.end()
    depth = 1
    index = start
    while index < len(source) and depth:
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
        index += 1
    return source[start : index - 1] if depth == 0 else None


def main() -> int:
    catalog = read("common/api/os2_api_catalog.inc")
    core_header = read("common/include/os2_doscalls_core.h")
    core_source = read("common/doscalls/os2_doscalls_core.c")
    native = read("dlls/doscalls/doscalls.c")
    nls_veneer = read("dlls/nls/nls.c")
    nls_core = read("common/nls/os2_nls.c")
    nls_api = read("common/nls/os2_nls_api.c")
    whp = read("whp/src/whp_os2_v2_hi.c")
    whp_fileio = read("whp/src/v2_fileio.h")
    root_make = read("Makefile")
    whp_make = read("whp/Makefile")
    transformer = read("transformer/le2pe386.c")

    errors: list[str] = []

    for ordinal, name in SHARED_DOSCALLS.items():
        catalog_pattern = re.compile(
            rf'OS2_API\("DOSCALLS",\s*{ordinal}u,\s*"{re.escape(name)}",'
            rf".*OS2_API_ROUTE_SHARED\)"
        )
        if catalog_pattern.search(catalog) is None:
            errors.append(f"catalogue does not mark DOSCALLS.{ordinal} {name} shared")

        core_name = f"os2_core_{name}"
        if core_name not in core_header:
            errors.append(f"missing declaration for {core_name}")
        if re.search(rf"\b{re.escape(core_name)}\s*\(", core_source) is None:
            errors.append(f"missing implementation for {core_name}")

        body = exported_function_body(native, name)
        if body is None:
            errors.append(f"native DOSCALLS export {name} was not found")
        elif core_name not in body:
            errors.append(f"native {name} does not call {core_name}")

        whp_case = re.search(
            rf"case\s+{ordinal}\s*:.*?{re.escape(core_name)}\s*\(",
            whp,
            re.DOTALL,
        )
        if whp_case is None:
            errors.append(f"WHP DOSCALLS.{ordinal} does not call {core_name}")

    if "case 230:" in whp_fileio or "GetLocalTime" in whp_fileio:
        errors.append("WHP v2_fileio.h still contains a private DosGetDateTime path")

    required_root_fragments = (
        "common/doscalls/os2_doscalls_core.c",
        "common/win32/os2_win32_services.c",
        "dlls/doscalls/doscalls.c $(DOSCALLS_CORE_SRC) $(WIN32_COMMON_SRC)",
    )
    for fragment in required_root_fragments:
        if fragment not in root_make:
            errors.append(f"root Makefile is missing shared wiring: {fragment}")

    required_whp_fragments = (
        "TARGET = whp_os2_v2_hi.exe",
        "../common/doscalls/os2_doscalls_core.c",
        "../common/api/os2_api_catalog.c",
        "../common/win32/os2_win32_services.c",
    )
    for fragment in required_whp_fragments:
        if fragment not in whp_make:
            errors.append(f"WHP Makefile is missing shared wiring: {fragment}")

    if "os2_api_name" not in transformer:
        errors.append("LE transformer is not using the canonical ordinal catalogue")
    if "os2_api_lookup" not in whp:
        errors.append("WHP import diagnostics are not using the canonical ordinal catalogue")

    nls_required = (
        "os2_nls_api_DosSetProcessCp",
        "os2_nls_api_DosQueryCp",
        "os2_nls_api_DosQueryCtryInfo",
        "os2_nls_api_DosQueryDBCSEnv",
        "os2_nls_api_DosMapCase",
    )
    for name in nls_required:
        if name not in nls_api:
            errors.append(f"missing shared NLS ABI implementation: {name}")
    if "GetLocaleInfo" in nls_veneer or "GetOEMCP" in nls_veneer:
        errors.append("NLS.DLL veneer still reaches Win32 locale/codepage APIs directly")
    for helper in ("O2NlsQueryCtryInfo", "O2NlsQueryDBCSEnv", "O2NlsMapCase"):
        if helper not in nls_veneer or helper not in native:
            errors.append(f"native NLS veneer is not routed through DOSCALLS helper {helper}")
    for fragment in (
        'else if (_stricmp(mod, "NLS") == 0)',
        "HC_NLS",
        "dispatch_nls",
        "os2_personality_context_set_nls(&personality, &rt->nls_state)",
    ):
        if fragment not in whp:
            errors.append(f"WHP NLS wiring is missing: {fragment}")
    for fragment in ("NLS_COMMON_SRC", "libdoscalls.a -Wl,--out-implib,libnls.a"):
        if fragment not in root_make:
            errors.append(f"root Makefile NLS wiring is missing: {fragment}")
    for fragment in ("../common/nls/os2_nls.c", "../common/nls/os2_nls_api.c"):
        if fragment not in whp_make:
            errors.append(f"WHP Makefile NLS wiring is missing: {fragment}")
    if "os2_cp437_upper" not in nls_core or "os2_cp850_upper" not in nls_core:
        errors.append("NLS core is not consuming both built-in SBCS case tables")

    if errors:
        print("Shared personality wiring check failed:")
        for error in errors:
            print(f"  {error}")
        return 1

    print(
        "PASS: native DOSCALLS/WHP core plus process-owned NLS wiring; "
        f"{len(SHARED_DOSCALLS)} original shared DOSCALLS implementations preserved"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
