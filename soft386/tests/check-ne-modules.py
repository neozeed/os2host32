#!/usr/bin/env python3
"""Assert that LINK386's NE-local module references are decoded by name."""
import subprocess
import sys
p = subprocess.run([sys.argv[1], '--trace-hc', '--run', sys.argv[2]], input=b'', capture_output=True, timeout=20)
log = (p.stdout + p.stderr).decode('latin1', errors='replace')
for i, name in enumerate(('DOSCALLS', 'KBDCALLS', 'NLS', 'MSG'), 1):
    assert 'soft386:   [%d] %s' % (i, name) in log, log
assert 'soft386: NLS16.4 ' in log, log
assert 'soft386: DOS16 GetEnv sel=' in log, log
assert 'NE unsupported imported module 3 ordinal 4' not in log, log
print('NE module-reference names PASS: DOSCALLS/KBDCALLS/NLS/MSG; NLS.4 dispatched')
