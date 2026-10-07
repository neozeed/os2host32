#!/usr/bin/env python3
import os, pathlib, struct, subprocess, sys, tempfile
from mkfixtures_r5 import image, Asm, ptr, var, PAGE, exit_call
exe,runtime,boundary=map(lambda x:pathlib.Path(x).resolve(),sys.argv[1:4]);fx=pathlib.Path(__file__).parent/'fixtures'
def run(command,env=None):
    p=subprocess.run(list(map(str,command)),capture_output=True,text=True,env=env,timeout=12)
    assert p.returncode==0,(command,p.returncode,p.stdout,p.stderr)
    return p
run([boundary])
for file in ['pm-r5b-soft386.le','queue-soft386.le']:
    p=run([runtime,'--trace-hc',fx/file]);assert 'CPU PASS' in p.stdout and 'provider PASS' in p.stdout
    assert 'unsupported' not in p.stderr and 'rejected' not in p.stderr
    assert 'FS=0020' in p.stderr and 'FS=0018' in p.stderr
# An explicit native service must bypass OS2LIBPATH's PE files, including name
# imports and DosQueryProcAddr. Unknown PE DLLs remain rejected (R5A regression).
with tempfile.TemporaryDirectory(prefix='soft386-r5b-') as temp:
    temp=pathlib.Path(temp);pe=bytearray(128);pe[:2]=b'MZ';struct.pack_into('<I',pe,60,64);pe[64:68]=b'PE\0\0'
    (temp/'QUECALLS.DLL').write_bytes(pe);env=os.environ.copy();env['OS2LIBPATH']=str(temp)
    run([exe,'--check',fx/'queue-soft386.le'],env)
    d=bytearray(PAGE);d[128:137]=b'QUECALLS\0';d[160:173]=b'DosReadQueue\0'
    c=Asm();c.call(1,318,[0,0,ptr(128),ptr(0)]);c.check(0,1);c.load(0);c.check(0x10f,2)
    c.call(1,321,[var(0),0,ptr(160),ptr(4)]);c.check(0,3);exit_call(c,0)
    f=temp/'api.le';f.write_bytes(image(c.done(),d,c.fix,['DOSCALLS']));run([exe,'--run-quiet',f])
    f.write_bytes(image(b'\xe8\0\0\0\0\xc3',bytes(PAGE),[('name',1,1,'DosReadQueue')],['QUECALLS']))
    run([exe,'--check',f],env)
print('Soft386 R5B PASS: PE service routing, queue wait/producer scheduling, opaque values, PM callback veneers and dialog state')
