#!/usr/bin/env python3
"""Verify large-model OS/2 NE printf, DOS16 bootstrap and exit."""
import subprocess
import sys
r = subprocess.run([sys.argv[1], sys.argv[2]], stdout=subprocess.PIPE,
                   stderr=subprocess.PIPE, timeout=30)
expected = b'hello from tiny memory large\r\n'
if r.returncode or r.stdout != expected or b'rc=0' not in r.stderr or b'unsupported DOS16.' in r.stderr:
    sys.stderr.write('LARGE NE regression FAIL\nstdout=%r\nstderr=%r\nrc=%d\n' %
                     (r.stdout, r.stderr, r.returncode))
    raise SystemExit(1)
print('LARGE NE printf regression PASS (exact stdout, no unsupported calls, rc=0)')
