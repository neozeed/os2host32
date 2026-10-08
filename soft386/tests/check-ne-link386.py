#!/usr/bin/env python3
"""NE-H3L: LINK386 reaches normal command parser through NLS.4."""
import subprocess
import sys
if len(sys.argv) != 3:
    raise SystemExit('usage: check-ne-link386.py SOFT386 LINK386.EXE')
p = subprocess.run([sys.argv[1], '--run', sys.argv[2]], stdin=subprocess.DEVNULL,
                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=20)
t = (p.stdout + p.stderr).decode('latin-1', 'replace')
assert 'type=NE' in t and 'entry=0108:514E' in t, t
assert 'Linear-Executable Linker  Version 1.01.015' in t, t
assert 'Object Modules [.obj]:' in t, t
assert 'fatal error L1020: no object modules specified' in t, t
assert 'unsupported imported module 3 ordinal 4' not in t, t
assert 'NE exception' not in t and 'NE runtime failure' not in t, t
assert 'termination=Dos16Exit(EXIT_PROCESS) rc=2' in t, t
print('NE-H3L LINK386 NLS.4 bridge PASS (DOS16.91 remains pending)')
