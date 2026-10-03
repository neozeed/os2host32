#!/usr/bin/env python3
import pathlib
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[1]
exe = (pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else root / 'soft386_os2').resolve()
bridge_check = (pathlib.Path(sys.argv[2]) if len(sys.argv) > 2 else root / 'bridge-check').resolve()
fx = root / 'tests' / 'fixtures'


def run_exe(*args):
    p = subprocess.run([str(exe), *map(str,args)], stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE, text=True)
    if p.returncode != 0:
        sys.stderr.write(p.stdout)
        sys.stderr.write(p.stderr)
        raise SystemExit('FAILED: %s rc=%d' % (' '.join(map(str,args)), p.returncode))
    return p


def require(text, marker, where):
    if marker not in text:
        raise SystemExit('missing %s marker: %s' % (where, marker))

# Loader/fixup regressions inherited from R0.
for name, kind in [('hello-soft386.le','LE'),('hello-soft386.lx','LX'),
                   ('internal-soft386.le','LE'),('internal-soft386.lx','LX'),
                   ('memory-soft386.le','LE'),('sync-soft386.le','LE'),
                   ('native-file-soft386.le','LE')]:
    p=run_exe('--check',fx/name)
    require(p.stderr, 'CHECK: '+kind, name)

p=run_exe(fx/'hello-soft386.le')
assert p.stdout == 'soft386: untouched 32-bit OS/2 LE/LX says hello!\n'
p=run_exe(fx/'hello-soft386.lx')
assert p.stdout == 'soft386: untouched 32-bit OS/2 LE/LX says hello!\n'
p=run_exe(fx/'internal-soft386.le')
assert p.stdout == 'soft386: internal OFF32 fixup PASS\n'
p=run_exe(fx/'internal-soft386.lx')
assert p.stdout == 'soft386: internal OFF32 fixup PASS\n'

# Historical hi.exe import surface remains available without the native bridge.
p=run_exe(fx/'hi-surface-soft386.le')
assert p.stdout == 'soft386: historical hi.exe DOSCALLS surface PASS\n'
for ordinal in (224,234,256,282,299,304,305,348):
    require(p.stderr, 'DOSCALLS.%u' % ordinal, 'hi-surface')

# Saved CPU context switching/TIB-FS restoration from R0.
p=run_exe('--trace-hc',fx/'thread-soft386.le')
assert p.stdout == ('soft386: worker thread ran\n'
                    'soft386: main resumed after DosWaitThread\n')
for marker in ('create TID 2','TID 1 waits for TID 2',
               'scheduler -> TID 2','FS=0020','TID 2 exited',
               'wake TID 1','scheduler -> TID 1','FS=0018','rc=0 cycles=43'):
    require(p.stderr, marker, 'thread')

# R1: virtual event/mutex objects block and wake virtual guest contexts.
p=run_exe('--trace-hc',fx/'sync-soft386.le')
assert p.stdout == ('soft386: jar mutex/event worker PASS\n'
                    'soft386: jar semaphore main resumed PASS\n')
for marker in ('DOSCALLS.324','DOSCALLS.331','DOSCALLS.334',
               'scheduler -> TID 2','scheduler -> TID 1',
               'wake TID 1 on HEV','FS=0020','FS=0018','rc=0 cycles=111'):
    require(p.stderr, marker, 'sync')

# R1: memory object state stays in the guest kernel namespace.
p=run_exe('--trace-hc',fx/'memory-soft386.le')
assert p.stdout == 'soft386: jar memory semantics PASS\n'
for ordinal in (299,304,305,306):
    require(p.stderr, 'DOSCALLS.%u' % ordinal, 'memory')
require(p.stderr, 'native DOSCALLS bridge=OFF', 'memory')
require(p.stderr, 'rc=0 cycles=85', 'memory')

# Native pointer marshalling is independently testable without Win32 by injecting
# a fake DOSCALLS provider.  This verifies copy-in/copy-out and opaque HFILE flow.
bp = subprocess.run([str(bridge_check)], stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE, text=True)
if bp.returncode != 0:
    sys.stderr.write(bp.stdout); sys.stderr.write(bp.stderr)
    raise SystemExit('bridge-check failed rc=%d' % bp.returncode)
assert bp.stdout == 'Soft386 DOSCALLS bridge marshalling: PASS\n'

print('Soft386 R1 regression: PASS')
