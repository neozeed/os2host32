#!/usr/bin/env python3
"""Static contract check for the PMWIN.885 SimCity import boundary."""
from __future__ import print_function

import re
import sys


def fail(message):
    print("SIMCITY PMWIN.885 CHECK FAILED: " + message)
    return 1


def main():
    pmwin_def = open("dlls/pmwin/pmwin.def").read()
    pmwin_c = open("dlls/pmwin/pmwin.c").read()
    catalog = open("common/api/os2_api_catalog.inc").read()

    if not re.search(r"^\s*WinStopTimer\s+@885\s+NONAME\s*$",
                     pmwin_def, re.M):
        return fail("PMWIN.885 export missing")
    function = re.search(
        r"O2ULONG\s+__cdecl\s+WinStopTimer\s*\(\s*"
        r"O2HAB\s+hab\s*,\s*O2HWND\s+hwnd\s*,\s*"
        r"O2ULONG\s+idTimer\s*\)\s*\{(.*?)\n\}",
        pmwin_c, re.S)
    if not function:
        return fail("WinStopTimer(HAB, HWND, ULONG) implementation missing")
    body = function.group(1)
    for token in ("native_hwnd(hwnd)", "KillTimer(wh, (UINT_PTR)idTimer)",
                  "idTimer == 0UL", "idTimer > 0xffffUL"):
        if token not in body:
            return fail("timer-stop contract missing: " + token)
    if not re.search(r'OS2_API\("PMWIN",\s*885u,\s*"WinStopTimer",',
                     catalog):
        return fail("API catalogue entry missing")
    print("SIMCITY PMWIN.885 static contract PASS")
    print("  PMWIN.885 = WinStopTimer(HAB, HWND, USHORT)")
    print("  implementation = Win32 KillTimer(window, timer-id)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
