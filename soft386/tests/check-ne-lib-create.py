#!/usr/bin/env python3
"""Real 16-bit OS/2 LIB.EXE create-path smoke test. Host sandbox only."""
import pathlib
import subprocess
import sys
import tempfile

exe = pathlib.Path(sys.argv[1]).resolve()
lib = pathlib.Path(sys.argv[2]).resolve()
with tempfile.TemporaryDirectory(prefix='soft386-ne-lib-') as t:
    p = subprocess.run([str(exe), '--run', str(lib)], input=b'NEH3H.LIB\ny\n\n\n\n',
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                       cwd=t, timeout=20)
    artifact = pathlib.Path(t) / 'NEH3H.LIB'
    assert p.returncode == 0, (p.returncode, p.stdout[-2000:], p.stderr[-2000:])
    assert artifact.is_file() and artifact.stat().st_size >= 512, (p.stdout, p.stderr)
    assert b'Library Manager' in p.stdout, p.stdout
    assert b'unsupported DOS16' not in p.stderr, p.stderr
    print('LIB NE creation regression PASS (new .LIB created, bytes written, rc=0)')
