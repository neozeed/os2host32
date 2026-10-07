#!/usr/bin/env python3
"""Focused real-CPU regressions for the reported R5A runtime gaps."""
from mkfixtures_r5 import Asm, image, ptr, var, cpu_abs, PAGE, DATA_BASE, exit_call, write_call
import pathlib, sys

def callptr(c,off,args):
    # Arguments here are constants or data variables, so no changing ESP loads.
    for a in reversed(args):
        if isinstance(a,tuple):
            if a[0]=='var':c.load(a[1]);c.u8(0x50)
            else:c.push_internal(2,a[1])
        else:c.push32(a)
    c.load(off);c.u8(0xff,0xd0);c.addesp(4*len(args))

def queue_image():
    d=bytearray(PAGE);d[512:524]=b'\\QUEUES\\jar\0';msg=b'Soft386 R5B queue CPU PASS\n';d[768:768+len(msg)]=msg
    c=Asm();c.call(2,16,[ptr(0),0,ptr(512)]);c.check(0,1)
    c.call(1,311,[ptr(4),('code','reader'),0,0,0x4000]);c.check(0,2)
    c.call(1,229,[20]) # reader has to park before producer runs
    c.call(2,14,[var(0),12345,4,0xffff1234,3]);c.check(0,3)
    c.call(1,349,[ptr(4),0]);c.check(0,4);c.load(8);c.check(1,5)
    write_call(c,DATA_BASE+768,len(msg),DATA_BASE+12);exit_call(c,0)
    c.label('reader');c.u8(0xd9,0xe8);c.call(2,15,[ptr(16),ptr(20),ptr(512)]);c.check(0,6)
    c.load(16);c.check(1,7);c.call(2,9,[var(20),ptr(24),ptr(32),ptr(36),0,0,ptr(40),0]);c.check(0,8)
    for off,val in [(24,1),(28,12345),(32,4),(36,0xffff1234),(40,3)]:c.load(off);c.check(val,9+off)
    c.u8(0x64,0xa1);c.u32(0x14);c.check(2,51)
    cpu_abs(c,[0xdb,0x1d],44);c.load(44);c.check(1,52);c.imm(1);c.store(8);c.u8(0xc3)
    return image(c.done(),d,c.fix,['DOSCALLS','QUECALLS'],name='QUEJAR')

def pm_image():
    d=bytearray(PAGE);d[512:516]=b'jar\0';d[640:647]=b'nested\0';msg=b'Soft386 R5B PM CPU PASS\n';d[768:768+len(msg)]=msg
    # 0 HAB,4 HMQ,8 HWND,12 oldproc,16 icon,20 bitmap,24 worker,
    # 28 worker-pass,32 dialog,36 short,40 pid,44 tid,48 queryinfo[28],
    # 80 callback mp1,84 callback mp2,88 HPS,96 rectangle,112 point.
    c=Asm();c.call(2,763,[0]);c.store(0);c.call(2,716,[var(0),0]);c.store(4)
    c.call(2,909,[1,0xffff0005,ptr(512),0,0,0,10,10,0,0,0,0,0]);c.store(8)
    c.call(2,929,[var(8),('code','subclass')]);c.store(12);c.test_eax();c.jump('fail1',0x84);c.failures.append(1)
    # A callable guest veneer queries the old native control proc. Never execute
    # the host function address as Tiny386 code, even on 32-bit hosts.
    callptr(c,12,[var(8),0x101,0,0]);c.store(16)
    c.call(2,822,[var(16),ptr(48)]);c.check(1,2);c.load(60);c.store(20)
    c.call(2,838,[var(8),ptr(40),ptr(44)]);c.check(1,3);c.load(40);c.check(1,4);c.load(44);c.check(1,5)
    c.call(2,757,[var(8)]);c.store(88)
    c.call(2,730,[var(88),var(20),0,ptr(112),0,0,0]);c.check(1,6)
    c.call(2,849,[var(8),0,12,0,0,0,ptr(96),2]);c.check(2,7)
    c.call(1,311,[ptr(24),('code','worker'),0,0,0x4000]);c.check(0,8)
    c.u8(0xd9,0xe8,0xfd,0xbb);c.u32(0x13579bdf)
    c.call(2,923,[1,var(8),('code','dialog'),0,1,ptr(512)]);c.check(42,9)
    c.u8(0x89,0xd8);c.check(0x13579bdf,10);c.u8(0x9c,0x58,0x25);c.u32(0x400);c.check(0x400,11);c.u8(0xfc)
    cpu_abs(c,[0xdb,0x1d],120);c.load(120);c.check(1,12)
    c.call(1,349,[ptr(24),0]);c.check(0,13);c.load(28);c.check(1,14)
    c.call(2,728,[var(8)]);c.check(1,15)
    callptr(c,12,[var(8),0x101,0,0]);c.check(0,16) # stale saved proc
    write_call(c,DATA_BASE+768,len(msg),DATA_BASE+124);exit_call(c,0)
    c.label('subclass');c.stack(12);c.store(80);c.stack(16);c.store(84);c.stack(8);c.cmp_eax_imm32(0x1001);c.jump('scalar',0x84)
    c.stack(8);c.store(128);callptr(c,12,[var(8),var(128),var(80),var(84)]);c.u8(0xc3)
    c.label('scalar');c.load(80);cpu_abs(c,[0x03,0x05],84);c.u8(0xc3)
    c.label('dialog');c.stack(4);c.store(32);c.stack(8);c.cmp_eax_imm32(0x3b);c.jump('dialog_other',0x85)
    c.u8(0xbb);c.u32(0xdeadbeef);c.u8(0x9c,0x58,0x25);c.u32(0x400);c.check(0,17)
    c.stack(16);c.u8(0x8b,0x00);c.check(0x0072616a,18)
    c.call(2,858,[var(32),10,123,0]);c.check(1,19)
    c.call(2,814,[var(32),10,ptr(36),0]);c.check(1,20);c.load(36);c.check(123,21)
    c.call(2,903,[var(32),10,0x143,8,0]);c.check(1,22)
    c.call(2,923,[1,var(32),('code','dialog_inner'),0,2,ptr(640)]);c.check(43,26)
    c.call(1,229,[30]);c.load(28);c.check(1,23)
    c.call(2,920,[var(8),0x1001,19,23]);c.check(42,24)
    c.call(2,729,[var(32),42]);c.check(1,25)
    c.label('dialog_other');c.imm(0);c.u8(0xc3)
    c.label('dialog_inner');c.stack(4);c.store(140);c.stack(16);c.u8(0x8b,0x00);c.check(0x7473656e,27)
    c.call(2,729,[var(140),43]);c.check(1,28);c.imm(0);c.u8(0xc3)
    c.label('worker');c.call(1,229,[1]);c.imm(1);c.store(28);c.u8(0xc3)
    return image(c.done(),d,c.fix,['DOSCALLS','PMWIN'],pm=True,name='PMR5B')

def main():
    out=pathlib.Path(sys.argv[1] if len(sys.argv)>1 else 'tests/fixtures');out.mkdir(parents=True,exist_ok=True)
    (out/'queue-soft386.le').write_bytes(queue_image());(out/'pm-r5b-soft386.le').write_bytes(pm_image())
    print('Soft386 R5B fixtures generated')
if __name__=='__main__':main()
