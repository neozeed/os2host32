#!/usr/bin/env python3
import pathlib
import subprocess
import sys

if len(sys.argv) != 4:
    raise SystemExit('usage: test_r3.py SOFT386 BRIDGE_CHECK SYSTEM_BRIDGE_CHECK')

exe = pathlib.Path(sys.argv[1]).resolve()
bridge = pathlib.Path(sys.argv[2]).resolve()
sysbridge = pathlib.Path(sys.argv[3]).resolve()
root = pathlib.Path(__file__).resolve().parent
fx = root / 'fixtures'

# Preserve the complete R2C acceptance suite.
p = subprocess.run([sys.executable, str(root/'test_r2.py'), str(exe), str(bridge), str(sysbridge)])
if p.returncode:
    raise SystemExit(p.returncode)

# R3 child-vessel command line compatibility.  Native DOSCALLS.283 launches
# the current executable with the historical loader switches below.
p = subprocess.run([str(exe), '--run-quiet', '--argv0', 'typed-child-name',
                    str(fx/'hello-soft386.le')],
                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
if p.returncode != 0:
    sys.stderr.write(p.stderr)
    raise SystemExit('R3 child-vessel loader switches failed')
if p.stdout != 'soft386: untouched 32-bit OS/2 LE/LX says hello!\n':
    raise SystemExit('R3 child-vessel output mismatch: %r' % p.stdout)
if 'scheduler ->' in p.stderr or 'resolve DOSCALLS' in p.stderr:
    raise SystemExit('R3 --run-quiet leaked loader/scheduler diagnostics')

p = subprocess.run([str(exe), '--run-quiet', str(fx/'thread-soft386.le')],
                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
if p.returncode != 0:
    sys.stderr.write(p.stderr)
    raise SystemExit('R3 quiet threaded child failed')
if p.stdout != ('soft386: worker thread ran\n'
                'soft386: main resumed after DosWaitThread\n'):
    raise SystemExit('R3 quiet threaded output mismatch')
if p.stderr:
    raise SystemExit('R3 quiet threaded child leaked diagnostics: %r' % p.stderr)

# R4: optional Tiny386 80387 is enabled and executes real x87 arithmetic/transcendentals.
p = subprocess.run([str(exe), '--run-quiet', str(fx/'fpu-soft386.le')],
                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
if p.returncode != 0 or p.stdout != 'soft386: 80387 x87 math PASS\n':
    sys.stderr.write(p.stderr)
    raise SystemExit('R4 x87 arithmetic/transcendental fixture failed: rc=%d out=%r' % (p.returncode,p.stdout))

# FPU bytes are part of CPUI386_State, so guest thread switching must isolate x87 stacks.
p = subprocess.run([str(exe), '--run-quiet', str(fx/'fpu-thread-soft386.le')],
                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
if p.returncode != 0 or p.stdout != 'soft386: per-thread 80387 state PASS\n':
    sys.stderr.write(p.stderr)
    raise SystemExit('R4 per-thread x87 state fixture failed: rc=%d out=%r' % (p.returncode,p.stdout))

print('Soft386 R3 regression: PASS')
print('Soft386 R4 regression: PASS')
