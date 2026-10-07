#!/usr/bin/env python3
import os, pathlib, struct, subprocess, sys, tempfile
from mkfixtures_r5 import image, PAGE
from mkfixtures_r5a import resource_dll

exe=pathlib.Path(sys.argv[1]).resolve();fx=pathlib.Path(__file__).parent/'fixtures'
def run(args,rc=0,env=None):
    p=subprocess.run([str(exe),*map(str,args)],capture_output=True,text=True,env=env,timeout=12)
    assert p.returncode==rc,(args,p.returncode,p.stdout,p.stderr)
    return p
p=run(['--run-quiet',fx/'resource-load-soft386.le'])
assert p.stdout=='Soft386 R5A iterated resource DLL + PMWP load PASS\n' and not p.stderr
p=run(['--check',fx/'RESOURCE.DLL']);assert '2 resource copies' in p.stderr
with tempfile.TemporaryDirectory(prefix='soft386-r5a-') as temp:
    temp=pathlib.Path(temp);pe=bytearray(512);pe[:2]=b'MZ';struct.pack_into('<I',pe,60,64);pe[64:68]=b'PE\0\0'
    for name in ['PMSHAPI','HELPMGR','PMWP','HOSTLIB']:(temp/(name+'.DLL')).write_bytes(pe)
    env=os.environ.copy();env['OS2LIBPATH']=str(temp)
    run(['--check',fx/'route-native-soft386.le'],env=env)
    caller=temp/'caller.le';caller.write_bytes(image(b'\xc3',bytes(PAGE),[],['HOSTLIB']))
    p=run(['--check',caller],1,env);assert 'HOSTLIB.DLL' in p.stderr and 'native PE image' in p.stderr
    bad=temp/'bad.dll';base=resource_dll();h=struct.unpack_from('<I',base,60)[0]
    stream=struct.unpack_from('<I',base,h+0x4c)[0];maps=h+struct.unpack_from('<I',base,h+0x48)[0]
    for field,value,fmt,marker in [(stream,0,'<H','invalid LX iterated expansion'),
            (stream,4097,'<H','invalid LX iterated expansion'),
            (maps+4,5,'<H','invalid LX iterated expansion'),
            (maps,0xffffffff,'<I','extends past EOF')]:
        data=bytearray(base);struct.pack_into(fmt,data,field,value);bad.write_bytes(data)
        p=run(['--check',bad],1);assert marker in p.stderr and 'bad.dll' in p.stderr
print('Soft386 R5A loader PASS: native routing, PE diagnostics, iterated bounds, independent DLL resources, PMWP jar loading')
