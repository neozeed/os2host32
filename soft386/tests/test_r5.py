#!/usr/bin/env python3
import pathlib, subprocess, sys, tempfile
from mkfixtures_r5 import image
root=pathlib.Path(__file__).resolve().parent
exe=pathlib.Path(sys.argv[1]).resolve(); runtime=pathlib.Path(sys.argv[2]).resolve(); boundary=pathlib.Path(sys.argv[3]).resolve()
fx=root/'fixtures'
def run(command,rc=0):
    p=subprocess.run(list(map(str,command)),capture_output=True,text=True,timeout=12)
    if p.returncode!=rc:raise AssertionError(f'{command}: expected {rc}, got {p.returncode}\n{p.stdout}\n{p.stderr}')
    return p
for name in ['dll-static-soft386.le','dll-static-soft386.lx']:
    p=run([exe,'--run-quiet',fx/name]);assert p.stdout=='Soft386 R5 transitive DLL imports PASS\n' and not p.stderr
for name,expected in [('dll-api-soft386.le','module API'),('dll-legacy-soft386.le','legacy INITINSTANCE')]:
    p=run([exe,'--run-quiet',fx/name]);assert p.stdout==f'Soft386 R5 {expected} PASS\n' and not p.stderr
p=run([runtime,'--trace-hc',fx/'pmjar-soft386.exe']);assert 'acceptance PASS' in p.stdout and 'headless PM runtime PASS' in p.stdout
assert 'depth=2' in p.stderr and 'FS=0020' in p.stderr and 'FS=0018' in p.stderr
assert 'callback enter TID=1 entry=0300' in p.stderr
run([boundary])
# Failures must unwind the native callback instead of resuming a dead frame or
# executing a malformed return. The normal provider/CPU loop handles these.
with tempfile.TemporaryDirectory(prefix='soft386-r5-') as tmp:
    tmp=pathlib.Path(tmp);(tmp/'pmjar.exe').write_bytes((fx/'pmjar-soft386.exe').read_bytes())
    for name,callback,rc,marker in [
        ('bad-ret',b'\x31\xc0\xc2\x04\x00',1,'invalid callback return frame'),
        ('halt',b'\xf4',1,'halted without process exit'),
        ('cycle-limit',b'\xeb\xfe',124,'cycle limit reached')]:
        # The query returns a harmless state address; WM_CREATE then exercises
        # the bad callback through the same native registration thunk.
        code=bytearray(callback);off=len(code);code+=b'\x68\0\0\0\0\x58\xc3'
        data=bytes(4096)
        (tmp/'PMJAR.DLL').write_bytes(image(code,data,[('internal',off+1,2,0)],[],dll=True,
            exports=[(1,'JarWndProc',1,0),(2,'JarState',1,off)],name='PMJAR'))
        p=run([runtime,'--run-quiet','--max-cycles','10000',tmp/'pmjar.exe'],rc)
        assert marker in p.stderr,(name,p.stderr)
        if name=='cycle-limit':
            assert 'forced process termination reason=cycle-limit rc=124' in p.stderr,p.stderr
print('Soft386 R5 regression PASS: DLL imports, PM callback runtime, marshal boundaries, callback failure unwind')
