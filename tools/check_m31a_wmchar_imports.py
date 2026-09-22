#!/usr/bin/env python
"""Strict import regression for the original Beta-2 WMCHAR.EXE."""
from __future__ import print_function

import os
import re
import subprocess
import sys

EXPECTED = set([
    ("PMGPI", 453),
    ("PMSHAPI", 120),
    ("PMWIN", 703), ("PMWIN", 716), ("PMWIN", 726), ("PMWIN", 728),
    ("PMWIN", 738), ("PMWIN", 743), ("PMWIN", 757), ("PMWIN", 763),
    ("PMWIN", 765), ("PMWIN", 834), ("PMWIN", 838), ("PMWIN", 840),
    ("PMWIN", 841), ("PMWIN", 848), ("PMWIN", 849), ("PMWIN", 888),
    ("PMWIN", 899), ("PMWIN", 908), ("PMWIN", 911), ("PMWIN", 912),
    ("PMWIN", 913), ("PMWIN", 915), ("PMWIN", 920), ("PMWIN", 926),
    ("DOSCALLS", 224), ("DOSCALLS", 234), ("DOSCALLS", 256),
    ("DOSCALLS", 282), ("DOSCALLS", 299), ("DOSCALLS", 304),
    ("DOSCALLS", 305), ("DOSCALLS", 348),
])

DEF_FILES = {
    "PMWIN": "pmwin.def",
    "PMGPI": "pmgpi.def",
    "PMSHAPI": "pmshapi.def",
    "DOSCALLS": "doscalls.def",
}


def main():
    scanner = sys.argv[1] if len(sys.argv) > 1 else "os2host32.exe"
    image = sys.argv[2] if len(sys.argv) > 2 else os.path.join(
        "examples", "m31a-wmchar", "WMCHAR.EXE")
    out = subprocess.check_output([scanner, "--scan", image])
    if not isinstance(out, str):
        out = out.decode("latin1")

    found = set()
    for line in out.splitlines():
        m = re.match(r"\s+([A-Z0-9]+)\.(\d+) \(", line)
        if m:
            found.add((m.group(1), int(m.group(2))))

    missing = sorted(EXPECTED - found)
    unexpected = sorted(found - EXPECTED)
    if missing or unexpected:
        print("WMCHAR import regression FAILED")
        if missing:
            print("  missing:", missing)
        if unexpected:
            print("  unexpected:", unexpected)
        return 1

    def_ordinals = {}
    for module, path in DEF_FILES.items():
        values = set()
        with open(path, "r") as f:
            for line in f:
                m = re.search(r"@(\d+)", line)
                if m:
                    values.add(int(m.group(1)))
        def_ordinals[module] = values

    uncovered = sorted((module, ordinal) for module, ordinal in found
                       if ordinal not in def_ordinals.get(module, set()))
    if uncovered:
        print("WMCHAR compatibility surface FAILED")
        print("  imports absent from DEF files:", uncovered)
        return 1

    print("WMCHAR import regression PASS: %d ordinals, all covered" % len(found))
    return 0


if __name__ == "__main__":
    sys.exit(main())
