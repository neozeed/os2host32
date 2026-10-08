#!/usr/bin/env python3
"""End-to-end verification of a historical 16-bit OS/2 printf NE guest."""
import subprocess
import sys

if len(sys.argv) != 3:
    raise SystemExit('usage: check-ne-printf.py soft386_os2 tiny-ne.exe')
run = subprocess.run([sys.argv[1], sys.argv[2]], stdout=subprocess.PIPE,
                     stderr=subprocess.PIPE, timeout=30)
expected = b'hello from tiny memory model\r\n'
if run.returncode != 0 or run.stdout != expected or b'rc=0' not in run.stderr:
    sys.stderr.write('TINY NE printf regression FAIL\n')
    sys.stderr.write('stdout=%r\nstderr=%r\nreturncode=%r\n' %
                     (run.stdout, run.stderr, run.returncode))
    raise SystemExit(1)
print('TINY NE printf regression PASS (exact stdout, Dos16Exit rc=0)')
