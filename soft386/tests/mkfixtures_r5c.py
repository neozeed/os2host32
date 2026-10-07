#!/usr/bin/env python3
from mkfixtures_r5 import Asm,image,ptr,var,cpu_abs,PAGE,DATA_BASE,exit_call,write_call
import pathlib,sys

def net_image():
 d=bytearray(PAGE);d[512:528]=bytes([2,0,0x5a,0xbe,127,0,0,1,0,0,0,0,0,0,0,0]);d[544:552]=b'jar\0data';d[560:570]=b'localhost\0'
 msg=b'Soft386 R5C TCP CPU PASS\n';d[768:768+len(msg)]=msg
 c=Asm();c.call(2,26,[]);c.check(0,1);c.call(3,11,[ptr(560)]);c.test_eax();c.jump('fail2',0x84);c.failures.append(2)
 c.call(2,16,[2,1,0]);c.store(0);c.cmp_eax_imm32(0xffffffff);c.jump('fail3',0x84);c.failures.append(3)
 c.call(2,3,[var(0),ptr(512),16]);c.check(0,4)
 c.call(1,311,[ptr(4),('code','reader'),0,0,0x4000]);c.check(0,5)
 c.call(1,229,[20]);c.call(2,13,[var(0),ptr(544),8,0]);c.check(8,6)
 c.call(1,349,[ptr(4),0]);c.check(0,7);c.load(8);c.check(1,8)
 c.call(2,17,[var(0)]);c.check(0,9);write_call(c,DATA_BASE+768,len(msg),DATA_BASE+12);exit_call(c,0)
 c.label('reader');c.u8(0xd9,0xe8,0xbb);c.u32(0x12345678)
 c.call(2,10,[var(0),ptr(576),8,0]);c.check(8,10)
 c.load(576);c.check(0x0072616a,11);c.load(580);c.check(0x61746164,12)
 c.u8(0x89,0xd8);c.check(0x12345678,13);c.u8(0x64,0xa1);c.u32(0x14);c.check(2,14)
 cpu_abs(c,[0xdb,0x1d],16);c.load(16);c.check(1,15);c.imm(1);c.store(8);c.u8(0xc3)
 return image(c.done(),d,c.fix,['DOSCALLS','SO32DLL','TCP32DLL'],name='NETJAR')

def exit_image():
 d=bytearray(PAGE);d[512:517]=b'PATH\0';d[528:538]=b'hello.txt\0';d[704:714]=b'exit.done\0';msg=b'Soft386 R5C exit/search CPU PASS\n';d[768:768+len(msg)]=msg
 c=Asm();c.call(1,228,[2,ptr(512),ptr(528),ptr(560),128]);c.check(0,1)
 c.call(1,296,[0x2001,('code','last')]);c.check(0,2)
 c.call(1,296,[0x1001,('code','first')]);c.check(0,3)
 exit_call(c,0)
 c.label('first');c.imm(0x12345678);c.store(0);c.call(1,296,[3,0]);exit_call(c,71)
 c.label('last');c.load(0);c.check(0x12345678,4);c.call(1,228,[2,ptr(512),ptr(704),ptr(560),128]);c.check(0,5);write_call(c,DATA_BASE+768,len(msg),DATA_BASE+4);c.call(1,296,[3,0]);exit_call(c,72)
 return image(c.done(),d,c.fix,['DOSCALLS'],name='EXITJAR')
if __name__=='__main__':
 out=pathlib.Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
 (out/'net-soft386.le').write_bytes(net_image());(out/'exit-soft386.le').write_bytes(exit_image())
