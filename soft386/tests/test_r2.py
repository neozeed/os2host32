#!/usr/bin/env python3
import pathlib, subprocess, sys

root=pathlib.Path(__file__).resolve().parents[1]
exe=(pathlib.Path(sys.argv[1]) if len(sys.argv)>1 else root/'soft386_os2').resolve()
doscheck=(pathlib.Path(sys.argv[2]) if len(sys.argv)>2 else root/'bridge-check').resolve()
syscheck=(pathlib.Path(sys.argv[3]) if len(sys.argv)>3 else root/'system-bridge-check').resolve()
fx=root/'tests'/'fixtures'

def run(*args):
    p=subprocess.run([str(exe),*map(str,args)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    if p.returncode:
        sys.stderr.write(p.stdout);sys.stderr.write(p.stderr)
        raise SystemExit('FAILED %s rc=%d'%(' '.join(map(str,args)),p.returncode))
    return p

def need(t,x):
    if x not in t: raise SystemExit('missing marker '+x)

# R1 inherited runtime acceptance.
for name,expected in [
 ('hello-soft386.le','soft386: untouched 32-bit OS/2 LE/LX says hello!\n'),
 ('hello-soft386.lx','soft386: untouched 32-bit OS/2 LE/LX says hello!\n'),
 ('internal-soft386.le','soft386: internal OFF32 fixup PASS\n'),
 ('internal-soft386.lx','soft386: internal OFF32 fixup PASS\n'),
 ('hi-surface-soft386.le','soft386: historical hi.exe DOSCALLS surface PASS\n'),
 ('memory-soft386.le','soft386: jar memory semantics PASS\n')]:
    p=run(fx/name);assert p.stdout==expected

p=run('--trace-hc',fx/'thread-soft386.le');need(p.stderr,'scheduler -> TID 2');need(p.stderr,'FS=0020');need(p.stderr,'FS=0018')
p=run('--trace-hc',fx/'sync-soft386.le');need(p.stderr,'wake TID 1 on HEV')

p=run('--trace-native','--no-doscalls-dll',fx/'hi-surface-soft386.le')
need(p.stderr,'image system-module mask=0 (VIO=no KBD=no SES=no)')
need(p.stderr,'native system bridges VIO=OFF KBD=OFF SES=OFF')

# R2: module resolver accepts system personalities even when native DLLs are absent.
p=run(fx/'system-modules-soft386.le')
assert p.stdout=='soft386: system module resolver PASS\n'
for m in ('VIOCALLS.19','KBDCALLS.13','SESMGR.8'):need(p.stderr,m)
need(p.stderr,'native system bridges VIO=OFF KBD=OFF SES=OFF')

# R2: common NLS state backs both DOSCALLS aliases and direct NLS.DLL ordinals.
p=run(fx/'nls-alias-soft386.le');assert p.stdout=='soft386: NLS aliases PASS\n'
for o in (289,291,395,396,397):need(p.stderr,'DOSCALLS.%u'%o)
p=run(fx/'nls-module-soft386.le');assert p.stdout=='ABZ';need(p.stderr,'NLS.7')

# Both marshalling layers are independently executable on non-Windows via fake providers.
for prog,expect in [(doscheck,'Soft386 DOSCALLS bridge marshalling: PASS\n'),
                    (syscheck,'Soft386 system DLL marshalling: PASS\n')]:
    q=subprocess.run([str(prog)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    if q.returncode:
        sys.stderr.write(q.stdout);sys.stderr.write(q.stderr);raise SystemExit('%s failed'%prog)
    assert q.stdout==expect

print('Soft386 R2C regression: PASS')
