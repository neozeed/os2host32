#!/usr/bin/env python3
"""Run unmodified Microsoft OS/2 NE librarian to create and add a real OMF OBJ."""
import pathlib
import subprocess
import sys
import tempfile

exe = pathlib.Path(sys.argv[1]).resolve()
libexe = pathlib.Path(sys.argv[2]).resolve()
obj = pathlib.Path(sys.argv[3]).resolve()
with tempfile.TemporaryDirectory(prefix='soft386-ne-lib-add-') as temp:
    target = pathlib.Path(temp)
    (target / 'HUGE.OBJ').write_bytes(obj.read_bytes())
    proc = subprocess.run([str(exe), '--run', str(libexe)],
                          input=b'huge.lib\ny\n+HUGE.OBJ\n\n\n',
                          cwd=target, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, timeout=30)
    result = (target / 'huge.lib').read_bytes() if (target / 'huge.lib').exists() else b''
    assert proc.returncode == 0, (proc.returncode, proc.stdout, proc.stderr)
    assert b'cannot access file' not in proc.stdout, proc.stdout
    assert b'unsupported DOS16' not in proc.stderr, proc.stderr
    assert len(result) > 1033, len(result)
    assert result.startswith(b'\xf0'), result[:16]
    assert b'HUGE' in result, result[:128]
    print('LIB NE object-add regression PASS (HUGE.OBJ opened, LIB produced, rc=0)')
