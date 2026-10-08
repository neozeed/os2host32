#!/usr/bin/env python3
"""Verify compact model NE libc printf under Soft386."""
import subprocess
import sys
r = subprocess.run([sys.argv[1], sys.argv[2]], stdout=subprocess.PIPE,
                   stderr=subprocess.PIPE, timeout=30)
expected = 'hello from compact memory huge'.encode('ascii') + b'\r\n'
if r.returncode or r.stdout != expected or b'rc=0' not in r.stderr or b'unsupported DOS16.' in r.stderr:
    sys.stderr.write('COMPACT NE regression FAIL\nstdout=%r\nstderr=%r\nrc=%d\n' %
                     (r.stdout, r.stderr, r.returncode))
    raise SystemExit(1)
print('COMPACT NE printf regression PASS (exact stdout, no unsupported calls, rc=0)')
