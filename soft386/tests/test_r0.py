#!/usr/bin/env python3
import pathlib
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[1]
exe = (pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else root / 'soft386_os2').resolve()
fx = root / 'tests' / 'fixtures'


def run(*args):
    p = subprocess.run([str(exe), *map(str,args)], stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE, text=True)
    if p.returncode != 0:
        sys.stderr.write(p.stdout)
        sys.stderr.write(p.stderr)
        raise SystemExit('FAILED: %s rc=%d' % (' '.join(map(str,args)), p.returncode))
    return p

for name, kind in [('hello-soft386.le','LE'),('hello-soft386.lx','LX'),
                   ('internal-soft386.le','LE'),('internal-soft386.lx','LX')]:
    p=run('--check',fx/name)
    if ('CHECK: '+kind) not in p.stderr:
        raise SystemExit('missing %s check marker for %s' % (kind,name))

p=run(fx/'hello-soft386.le')
assert p.stdout == 'soft386: untouched 32-bit OS/2 LE/LX says hello!\n'
p=run(fx/'hello-soft386.lx')
assert p.stdout == 'soft386: untouched 32-bit OS/2 LE/LX says hello!\n'
p=run(fx/'internal-soft386.le')
assert p.stdout == 'soft386: internal OFF32 fixup PASS\n'
p=run(fx/'internal-soft386.lx')
assert p.stdout == 'soft386: internal OFF32 fixup PASS\n'
p=run(fx/'hi-surface-soft386.le')
assert p.stdout == 'soft386: historical hi.exe DOSCALLS surface PASS\n'
for ordinal in (224,234,256,282,299,304,305,348):
    if ('DOSCALLS.%u' % ordinal) not in p.stderr:
        raise SystemExit('historical surface fixture did not resolve DOSCALLS.%u' % ordinal)

p=run('--trace-hc',fx/'thread-soft386.le')
assert p.stdout == ('soft386: worker thread ran\n'
                    'soft386: main resumed after DosWaitThread\n')
for marker in ('create TID 2','TID 1 waits for TID 2',
               'scheduler -> TID 2','FS=0020','TID 2 exited',
               'wake TID 1','scheduler -> TID 1','FS=0018','rc=0 cycles=43'):
    if marker not in p.stderr:
        raise SystemExit('missing thread marker: '+marker)

print('Soft386 R0 regression: PASS')
