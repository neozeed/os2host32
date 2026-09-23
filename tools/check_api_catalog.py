#!/usr/bin/env python3
"""Check that ordinal exports remain represented by the shared API catalogue."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ENTRY_RE = re.compile(
    r'^OS2_API\("([^"]+)",\s*(\d+)u,\s*"([^"]+)",\s*'
    r'(\d+)u,\s*(OS2_API_ROUTE_[A-Z_]+)\)$'
)


def read_catalog() -> dict[tuple[str, int], tuple[str, int, str]]:
    result: dict[tuple[str, int], tuple[str, int, str]] = {}
    path = ROOT / "common/api/os2_api_catalog.inc"
    for number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("/*"):
            continue
        match = ENTRY_RE.match(line)
        if not match:
            raise SystemExit(f"{path}:{number}: malformed catalogue entry")
        module, ordinal_text, name, argbytes_text, route = match.groups()
        key = (module.upper(), int(ordinal_text))
        if key in result:
            raise SystemExit(f"duplicate catalogue key {module}.{ordinal_text}")
        argbytes = int(argbytes_text)
        if argbytes and argbytes % 4:
            raise SystemExit(f"{module}.{ordinal_text}: argument byte count is not DWORD aligned")
        result[key] = (name, argbytes, route)
    return result


def read_def_exports() -> list[tuple[str, int, str, Path]]:
    exports: list[tuple[str, int, str, Path]] = []
    for path in sorted((ROOT / "dlls").glob("*/*.def")):
        module: str | None = None
        for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
            line = raw.split(";", 1)[0].strip()
            if not line:
                continue
            library = re.match(r"LIBRARY\s+(\S+)", line, re.IGNORECASE)
            if library:
                module = library.group(1).upper()
                continue
            ordinal = re.search(r"@\s*(\d+)", line)
            if module is None or ordinal is None:
                continue
            symbol = line[: ordinal.start()].strip().split()[0]
            if "=" in symbol:
                _public, symbol = symbol.split("=", 1)
            exports.append((module, int(ordinal.group(1)), symbol, path))
    return exports


def main() -> int:
    catalog = read_catalog()
    exports = read_def_exports()
    errors: list[str] = []
    for module, ordinal, symbol, path in exports:
        key = (module, ordinal)
        entry = catalog.get(key)
        if entry is None:
            errors.append(f"missing {module}.{ordinal} ({symbol}) from {path.relative_to(ROOT)}")
            continue
        if entry[0].lower() != symbol.lower():
            errors.append(
                f"name mismatch {module}.{ordinal}: catalogue={entry[0]} def={symbol}"
            )

    required_shared = {
        230: "DosGetDateTime",
        224: "DosQueryHType",
        256: "DosSetFilePtr",
        282: "DosWrite",
        299: "DosAllocMem",
        304: "DosFreeMem",
        305: "DosSetMem",
        348: "DosQuerySysInfo",
        289: "DosSetProcessCp",
        291: "DosQueryCp",
        395: "DosQueryCtryInfo",
        396: "DosQueryDBCSEnv",
        397: "DosMapCase",
    }
    for ordinal, name in required_shared.items():
        entry = catalog.get(("DOSCALLS", ordinal))
        if entry is None or entry[0] != name or entry[2] != "OS2_API_ROUTE_SHARED":
            errors.append(f"DOSCALLS.{ordinal} must identify {name} as shared")

    for ordinal, name in {5: "DosQueryCtryInfo", 6: "DosQueryDBCSEnv", 7: "DosMapCase"}.items():
        entry = catalog.get(("NLS", ordinal))
        if entry is None or entry[0] != name or entry[2] != "OS2_API_ROUTE_SHARED":
            errors.append(f"NLS.{ordinal} must identify {name} as shared")

    if errors:
        print("API catalogue check failed:")
        for error in errors:
            print(f"  {error}")
        return 1
    print(
        f"PASS: {len(catalog)} catalogue entries cover "
        f"{len(exports)} ordinal DLL exports"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
