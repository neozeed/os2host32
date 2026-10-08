#!/usr/bin/env python3
"""NE-H3K: Microsoft 16-bit CRT argv bootstrap and quoting regression."""
import subprocess
import sys

binary, fixture = sys.argv[1:3]
for args, expected in [
    (['1', '2', '3'], ['got 4 args', '[1]\t[1]', '[2]\t[2]', '[3]\t[3]']),
    (['two words', '3'], ['got 3 args', '[1]\t[two words]', '[2]\t[3]']),
    ([], ['got 1 args']),
]:
    p = subprocess.run([binary, '--run', fixture, *args], capture_output=True, text=True, timeout=20)
    if p.returncode or any(x not in p.stdout for x in expected) or '[0]\t[' + fixture + ']' not in p.stdout:
        raise SystemExit('NE argv regression failed: args=%r rc=%d stdout=%r stderr=%r' % (args, p.returncode, p.stdout, p.stderr))
print('NE-H3K ARGS PASS: zero, multiple, and quoted arguments')
