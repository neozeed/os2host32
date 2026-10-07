#!/usr/bin/env python3
"""R5A loader fixtures: no historical applications are redistributed."""
import pathlib, struct, sys
from mkfixtures import PAGE, exit_call
from mkfixtures_r5 import Asm, image, ptr, var

def resource_dll():
    b=bytearray(image(b'one!'*(PAGE//4),b'two?'*(PAGE//4),[],[],dll=True,lx=True,
        resources=[(5,1,0,4),(5,2,0,4)],name='RESOURCE'))
    h=struct.unpack_from('<I',b,60)[0]
    u=lambda off:struct.unpack_from('<I',b,h+off)[0]
    obj=h+u(0x40);maps=h+u(0x48);res=h+u(0x50)
    for i in range(2):
        struct.pack_into('<III',b,obj+i*24,PAGE,0,0x2039)
    struct.pack_into('<H',b,res+8,1)
    start=len(b);struct.pack_into('<I',b,h+0x4c,start)
    for i,pattern in enumerate([b'one!',b'two?']):
        encoded=struct.pack('<HH',PAGE//4,4)+pattern
        struct.pack_into('<IHH',b,maps+i*8,len(b)-start,len(encoded),1)
        b+=encoded
    return bytes(b)

def resource_caller():
    d=bytearray(PAGE);d[128:137]=b'RESOURCE\0'
    msg=b'Soft386 R5A iterated resource DLL + PMWP load PASS\n'
    d[256:256+len(msg)]=msg
    c=Asm();c.call(2,203,[0,ptr(0),ptr(128),0]);c.check(0,1)
    for rid,slot,expected in [(1,4,b'one!'),(2,8,b'two?')]:
        c.call(1,352,[var(0),5,rid,ptr(slot)]);c.check(0,2+rid)
        c.load(slot);c.u8(0x8b,0x00);c.check(int.from_bytes(expected,'little'),4+rid)
    c.call(1,282,[1,ptr(256),len(msg),ptr(16)]);exit_call(c,0)
    return image(c.done(),d,c.fix,['DOSCALLS','PMWP'],name='RESCALL')

def main(out):
    out=pathlib.Path(out);out.mkdir(parents=True,exist_ok=True)
    (out/'RESOURCE.DLL').write_bytes(resource_dll())
    (out/'resource-load-soft386.le').write_bytes(resource_caller())
    (out/'route-native-soft386.le').write_bytes(image(b'\xc3',bytes(PAGE),[],
        ['PMSHAPI','HELPMGR','PMWP'],name='ROUTE'))
    print('Soft386 R5A fixtures generated')

if __name__=='__main__':main(sys.argv[1] if len(sys.argv)>1 else pathlib.Path(__file__).parent/'fixtures')
