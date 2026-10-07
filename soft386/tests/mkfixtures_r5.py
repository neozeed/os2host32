#!/usr/bin/env python3
"""CRT-free LE/LX PM/DLL acceptance programs; no historical binaries required."""
import pathlib, struct, sys
from mkfixtures import Code, PAGE, DATA_BASE, CODE_BASE, make_image, exit_call, write_call

class Asm(Code):
    def __init__(self):
        super().__init__(); self.labels={}; self.branches=[]; self.failures=[]
    def label(self, name): self.labels[name]=len(self.b)
    def jump(self, name, condition=None):
        self.u8(*( [0xe9] if condition is None else [0x0f, condition]));off=len(self.b);self.u32(0);self.branches.append((off,name))
    def check(self, expected, code):
        self.cmp_eax_imm32(expected);self.jump('fail%d'%code,0x85)
        if code not in self.failures:self.failures.append(code)
    def load(self, offset, obj=2):
        self.u8(0xa1);off=len(self.b);self.u32(0);self.fix.append(('internal',off,obj,offset))
    def store(self, offset, obj=2):
        self.u8(0xa3);off=len(self.b);self.u32(0);self.fix.append(('internal',off,obj,offset))
    def imm(self, n):self.u8(0xb8);self.u32(n)
    def stack(self, offset):self.u8(0x8b,0x44,0x24,offset)
    def call(self, mod, ordinal, args):
        for x in reversed(args):
            if isinstance(x,tuple):
                if x[0]=='ptr':self.push_internal(2,x[1])
                elif x[0]=='code':self.u8(0x68);p=len(self.b);self.u32(0);self.branches.append(('code',p,x[1]))
                elif x[0]=='var':self.load(x[1]);self.u8(0x50)
                elif x[0]=='arg': # offset measured before this function's pushes
                    self.stack(x[1]+4*(len(args)-1-list(reversed(args)).index(x))); self.u8(0x50)
            else:self.push32(x)
        self.call_import(ordinal,mod);self.addesp(len(args)*4)
    def done(self):
        for e in self.failures:
            self.label('fail%d'%e);exit_call(self,e)
        for x in self.branches:
            if x[0]=='code':self.fix.append(('internal',x[1],1,self.labels[x[2]]))
            else:self.patch_rel32(x[0],self.labels[x[1]])
        return bytes(self.b)

def image(code,data,fix,modules,*,dll=False,exports=(),init=None,lx=False,resources=(),name='FIXTURE',pm=False):
    le=0x80; obj=0xc4; maps=obj+48; restab=maps+(16 if lx else 8)
    resources_bytes=b''.join(struct.pack('<HHIHI',ty,rid,size,2,off) for ty,rid,off,size in resources)
    resident=restab+len(resources_bytes)
    names=bytes([len(name)])+name.encode()+b'\0\0'
    for ordinal,ename,_,_ in exports:
        if ename:names+=bytes([len(ename)])+ename.encode()+struct.pack('<H',ordinal)
    names+=b'\0'; entry=resident+len(names);ent=bytearray();nextord=1
    for ordinal,ename,objectno,offset in sorted(exports):
        if ordinal>nextord:ent.extend(bytes([ordinal-nextord,0]))
        ent.extend(struct.pack('<BBHB I',1,3,objectno,1,offset));nextord=ordinal+1
    ent.append(0);fp=(entry+len(ent)+3)&~3;fr=fp+12
    proc=bytearray(b'\0');procoff={}
    for kind,off,mod,target in fix:
        if kind.startswith('name') and target not in procoff:
            procoff[target]=len(proc);proc+=bytes([len(target)])+target.encode()
    records=[bytearray(),bytearray()]
    for kind,off,mod,target in fix:
        page=1 if kind.endswith('_data') else 0
        if kind=='internal':rec=struct.pack('<BBhBI',7,0x10,off,mod,target)
        else:
            flags=0x12 if kind.startswith('name') else 0x11
            source=7 if 'abs' in kind or page else 8
            rec=struct.pack('<BBhBI',source,flags,off,mod,procoff.get(target,target))
        records[page]+=rec
    mods=b''.join(bytes([len(m)])+m.encode() for m in modules);im=fr+sum(map(len,records));ip=im+len(mods)
    datap=(le+ip+len(proc)+PAGE-1)&~(PAGE-1)
    out=bytearray(datap+2*PAGE);out[:2]=b'MZ';struct.pack_into('<I',out,0x3c,le);out[le:le+4]=b'LX\0\0' if lx else b'LE\0\0';struct.pack_into('<HH',out,le+8,2,1)
    header={0x10:(0x40008000 if dll else (0x300 if pm else 0)),0x14:2,0x18:1 if init is not None or not dll else 0,0x1c:init or 0,0x20:0 if dll else 2,0x24:0 if dll else 0x4000,0x28:PAGE,0x2c:0 if lx else PAGE,0x40:obj,0x44:2,0x48:maps,0x50:restab,0x54:len(resources),0x58:resident,0x5c:entry,0x68:fp,0x6c:fr,0x70:im,0x74:len(modules),0x78:ip,0x80:datap}
    for off,val in header.items():struct.pack_into('<I',out,le+off,val)
    struct.pack_into('<IIIIII',out,le+obj,PAGE,CODE_BASE,0x2005,1,1,0)
    struct.pack_into('<IIIIII',out,le+obj+24,0x4000,DATA_BASE,0x2003,2,1,0)
    if lx:
        struct.pack_into('<IHHIHH',out,le+maps,0,PAGE,0,PAGE,PAGE,0)
    else:out[le+maps:le+maps+8]=b'\0\0\1\0\0\0\2\0'
    out[le+restab:le+resident]=resources_bytes;out[le+resident:le+entry]=names;out[le+entry:le+entry+len(ent)]=ent
    struct.pack_into('<III',out,le+fp,0,len(records[0]),len(records[0])+len(records[1]))
    out[le+fr:le+im]=b''.join(records);out[le+im:le+ip]=mods;out[le+ip:le+ip+len(proc)]=proc
    assert len(code)<=PAGE and len(data)<=PAGE
    out[datap:datap+len(code)]=code;out[datap+PAGE:datap+PAGE+len(data)]=data
    return bytes(out)

def ptr(x):return ('ptr',x)
def var(x):return ('var',x)
def cpu_abs(c,op,off):c.u8(*op);p=len(c.b);c.u32(0);c.fix.append(('internal',p,2,off))

def make_dll():
    # Per-instance state: hwnd, initialized, paints, nested calls, HPS, rectangle.
    d=bytearray(PAGE);title=b'Soft386 PM: DLL window procedure in the jar\0';d[128:128+len(title)]=title
    struct.pack_into('<ii',d,256,12,32)
    # RT_STRING 1: codepage + 16 NUL-terminated length-prefixed strings.
    resource=struct.pack('<H',437)+bytes([13])+b'Jar resource\0'+bytes([1,0])*15
    d[512:512+len(resource)]=resource
    c=Asm();c.label('init');c.stack(8);c.test_eax();c.jump('term',0x85)
    c.imm(1);c.store(4);c.u8(0xc3)
    c.label('term');c.imm(2);c.store(4);c.imm(1);c.u8(0xc3)
    c.label('proc');c.u8(0xbb);c.u32(0xdeadbeef);c.u8(0x9c,0x58,0x25);c.u32(0x400);c.check(0,57);c.u8(0x64,0xa1);c.u32(0x14);c.check(1,58);c.stack(4);c.store(0);c.stack(8)
    for msg,name in [(1,'create'),(0x23,'paint'),(0x1001,'add'),(0x1002,'quit'),(2,'destroy')]:c.cmp_eax_imm32(msg);c.jump(name,0x84)
    c.imm(0);c.u8(0xc3)
    c.label('create');c.load(4);c.check(1,51)
    # Nested WinSendMsg executes another guest frame, returns 42, preserves x87.
    c.u8(0xd9,0xe8) # fld1
    c.call(2,920,[var(0),0x1001,17,25]);c.check(42,52)
    c.call(1,229,[1]) # suspend inside the native CreateStdWindow callback
    cpu_abs(c,[0xdb,0x1d],64) # fistp dword [dll+64]
    c.load(64);c.check(1,53);c.imm(0);c.u8(0xc3)
    c.label('add');c.stack(12);c.u8(0x03,0x44,0x24,16);c.u8(0xc3)
    c.label('paint');c.call(2,703,[var(0),0,ptr(80)]);c.store(16);c.test_eax();c.jump('fail54',0x84);c.failures.append(54)
    c.call(2,743,[var(16),ptr(80),-2]);c.check(1,55)
    c.call(3,517,[var(16),2]);c.call(3,359,[var(16),ptr(256),len(title)-1,ptr(128)]);c.check(1,59)
    c.call(2,738,[var(16)]);c.check(1,56);c.imm(1);c.store(8);c.imm(0);c.u8(0xc3)
    c.label('quit');c.call(2,919,[var(0),0x2a,0,0]);c.imm(0);c.u8(0xc3)
    c.label('destroy');c.imm(1);c.store(12);c.imm(0);c.u8(0xc3)
    c.label('data');c.push_internal(2,0);c.u8(0x58,0xc3)
    code=c.done();exports=[(1,'JarWndProc',1,c.labels['proc']),(2,'JarState',1,c.labels['data'])]
    return image(code,d,c.fix,['DOSCALLS','PMWIN','PMGPI'],dll=True,exports=exports,init=0,lx=True,resources=[(5,1,512,len(resource))],name='PMJAR')

def make_pm(view=False):
    d=bytearray(PAGE)
    strings={128:b'PMJAR\0',160:b'JarWndProc\0',200:b'Soft386R5\0',224:b'Soft386 R5 - PM + DLL callbacks\0',320:b'Soft386 R5 PM/DLL acceptance PASS\n',512:b'JarState\0'}
    for off,s in strings.items():d[off:off+len(s)]=s
    # 0 hab,4 hmq,8 module,12 proc,16 client,20 frame,24 flags,28 tid,
    # 32 written,36 worker_pass,40 oldsp,44 stateptr,48 queried_proc,64 qmsg.
    struct.pack_into('<I',d,24,0x1b)
    c=Asm();c.u8(0xd9,0xe8,0xd9,0xe8,0xde,0xc1,0xd9,0xe8,0xde,0xc1) # main ST0=3
    c.call(1,318,[0,0,ptr(128),ptr(8)]);c.check(0,1)
    c.call(1,321,[var(8),0,ptr(160),ptr(12)]);c.check(0,2)
    c.call(1,321,[var(8),1,0,ptr(48)]);c.check(0,3)
    c.load(12);c.u8(0x3b,0x05);pos=len(c.b);c.u32(0);c.fix.append(('internal',pos,2,48));c.jump('fail4',0x85);c.failures.append(4)
    c.call(1,321,[var(8),0,ptr(512),ptr(48)]);c.check(0,5)
    c.load(48);c.u8(0xff,0xd0);c.store(44) # dynamic guest function returns DLL data ptr
    c.call(2,763,[0]);c.store(0);c.call(2,716,[var(0),0]);c.store(4);c.test_eax();c.jump('fail6',0x84);c.failures.append(6)
    c.call(2,926,[var(0),ptr(200),var(12),4,4]);c.check(1,7)
    c.call(1,311,[ptr(28),('code','worker'),0,0,0x4000]);c.check(0,8)
    c.u8(0xbb);c.u32(0x13579bdf);c.u8(0xfd) # DF=1 is restored after callback
    c.call(2,908,[1,0x80000000,ptr(24),ptr(200),ptr(224),0,var(8),0,ptr(16)]);c.store(20);c.test_eax();c.jump('fail9',0x84);c.failures.append(9)
    c.u8(0x89,0xd8);c.check(0x13579bdf,19);c.u8(0x9c,0x58,0x25);c.u32(0x400);c.check(0x400,20);c.u8(0xfc)
    # Load string through copied DLL resources; resource ID 0 uses bundle 1.
    c.call(2,781,[var(0),var(8),0,64,ptr(576)]);c.check(12,10)
    c.label('loop');c.call(2,915,[var(0),ptr(64),0,0,0]);c.test_eax();c.jump('end',0x84)
    c.call(2,912,[var(0),ptr(64)]);c.jump('loop')
    c.label('end');c.call(1,349,[ptr(28),0]);c.check(0,11);c.load(36);c.check(1,12)
    c.load(44);c.u8(0x8b,0x40,8);c.check(1,13) # real WM_PAINT happened
    c.call(2,728,[var(20)]);c.check(1,14)
    c.load(44);c.u8(0x8b,0x40,12);c.check(1,15) # WM_DESTROY happened in DLL
    c.call(2,726,[var(4)]);c.call(2,888,[var(0)])
    cpu_abs(c,[0xdb,0x1d],48);c.load(48);c.check(3,16) # interrupted application's x87 still 3
    write_call(c,DATA_BASE+320,len(strings[320]),DATA_BASE+32);exit_call(c,0)
    c.label('worker');c.call(1,229,[60]);c.label('awaitpaint');c.load(44);c.u8(0x8b,0x40,8);c.test_eax();c.jump('painted',0x85);c.call(1,229,[10]);c.jump('awaitpaint');c.label('painted');
    if view:c.call(1,229,[5000])
    c.call(2,920,[var(16),0x1001,19,23]);c.check(42,17);c.u8(0x64,0xa1);c.u32(0x14);c.check(2,21)
    c.imm(1);c.store(36);c.call(2,919,[var(16),0x1002,0,0]);c.check(1,18);c.u8(0xc3)
    code=c.done();return image(code,d,c.fix,['DOSCALLS','PMWIN'],pm=True,name='PMJARHELLO')

def make_static_dll_pair(out):
    # Named transitive REL32 plus an OFF32 import to a DLL data export.
    d=bytearray(PAGE);struct.pack_into('<I',d,4,0xcafebabe)
    c=Asm();c.u8(0xb8);p=len(c.b);c.u32(0);c.fix.append(('external_abs',p,1,2));c.u8(0x8b,0x00,0xc3)
    (out/'OUTER.DLL').write_bytes(image(c.b,d,c.fix,['INNER'],dll=True,exports=[(1,'ReadInner',1,0)],name='OUTER'))
    (out/'INNER.DLL').write_bytes(image(b'\xb8\x2a\0\0\0\xc3',d,[],[],dll=True,exports=[(1,'Answer',1,0),(2,'SharedData',2,4)],name='INNER'))
    c=Asm();c.u8(0xe8);p=len(c.b);c.u32(0);c.fix.append(('name',p,2,'ReadInner'));c.check(0xcafebabe,31)
    msg=b'Soft386 R5 transitive DLL imports PASS\n';d[:len(msg)]=msg;write_call(c,DATA_BASE,len(msg),DATA_BASE+128);exit_call(c)
    code=c.done()
    for lx in [False,True]:(out/('dll-static-soft386.lx' if lx else 'dll-static-soft386.le')).write_bytes(image(code,d,c.fix,['DOSCALLS','OUTER'],lx=lx,name='DLLSTATIC'))

def make_module_api(out):
    d=bytearray(PAGE)
    for off,text in [(128,b'OUTER\0'),(160,b'outer.dll\0'),(192,b'ReadInner\0'),(224,b'ABSENT_R5_NOT_A_DLL\0')]:d[off:off+len(text)]=text
    c=Asm();c.call(1,318,[ptr(512),64,ptr(128),ptr(0)]);c.check(0,41)
    c.call(1,319,[ptr(160),ptr(4)]);c.check(0,42)
    c.load(0);c.u8(0x3b,0x05);off=len(c.b);c.u32(0);c.fix.append(('internal',off,2,4));c.jump('fail43',0x85);c.failures.append(43)
    c.call(1,320,[var(0),256,ptr(768)]);c.check(0,44)
    c.call(1,321,[var(0),0,ptr(192),ptr(8)]);c.check(0,45);c.load(8);c.u8(0xff,0xd0);c.check(0xcafebabe,46)
    c.call(1,322,[var(0)]);c.check(50,47)
    c.call(1,318,[ptr(512),64,ptr(224),ptr(16)]);c.check(2,48);c.load(16);c.check(0,49)
    msg=b'Soft386 R5 module API PASS\n';d[320:320+len(msg)]=msg;write_call(c,DATA_BASE+320,len(msg),DATA_BASE+32);exit_call(c)
    code=c.done();(out/'dll-api-soft386.le').write_bytes(image(code,d,c.fix,['DOSCALLS'],name='DLLAPI'))
    c=Asm();c.stack(4);c.check(0,61);c.stack(8);c.store(16);c.imm(1);c.u8(0xc3);entry=len(c.b);c.load(16);c.u8(0xc3)
    code=c.done();dll=bytearray(image(code,bytes(PAGE),c.fix,['DOSCALLS'],dll=True,init=0,exports=[(1,'MyModule',1,entry)],name='LEGACY'))
    struct.pack_into('<I',dll,0x80+0x10,0x8004)
    (out/'LEGACY.DLL').write_bytes(dll)
    c=Asm();c.call(2,1,[]);c.check(0x1000,62)
    msg=b'Soft386 R5 legacy INITINSTANCE PASS\n';d[:len(msg)]=msg;write_call(c,DATA_BASE,len(msg),DATA_BASE+64);exit_call(c)
    code=c.done();(out/'dll-legacy-soft386.le').write_bytes(image(code,d,c.fix,['DOSCALLS','LEGACY'],name='DLLLEGACY'))

def main():
    out=pathlib.Path(sys.argv[1] if len(sys.argv)>1 else 'tests/fixtures');out.mkdir(parents=True,exist_ok=True)
    (out/'PMJAR.DLL').write_bytes(make_dll());(out/'pmjar-soft386.exe').write_bytes(make_pm());(out/'pmjar-view-soft386.exe').write_bytes(make_pm(True));make_static_dll_pair(out);make_module_api(out)
    print('Soft386 R5 LE/LX fixtures generated')
if __name__=='__main__':main()
