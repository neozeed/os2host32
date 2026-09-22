#!/usr/bin/env python
from __future__ import print_function
import os,re,struct,sys

def u16(b,o): return struct.unpack_from('<H',b,o)[0]
def u32(b,o): return struct.unpack_from('<I',b,o)[0]

def object_bytes(data,h,objnum):
    objtab=h+u32(data,h+0x40); pagemap=h+u32(data,h+0x48)
    pagesize=u32(data,h+0x28); datapage=u32(data,h+0x80); last=u32(data,h+0x2c); mpages=u32(data,h+0x14)
    o=objtab+(objnum-1)*24; size=u32(data,o); first=u32(data,o+12); count=u32(data,o+16)
    out=bytearray()
    for i in range(count):
        m=pagemap+(first-1+i)*4; phys=(data[m]<<16)|(data[m+1]<<8)|data[m+2]; flags=data[m+3]
        if flags==3: out.extend(b'\0'*pagesize); continue
        if flags!=0 or phys==0: raise ValueError('unsupported LE page')
        n=last if last and phys==mpages else pagesize; off=datapage+(phys-1)*pagesize
        out.extend(data[off:off+n]); out.extend(b'\0'*(pagesize-n))
    return bytes(out[:size])

def resources(data,h):
    tab=h+u32(data,h+0x50); cnt=u32(data,h+0x54); out={}
    for i in range(cnt):
        p=tab+i*14; typ=u16(data,p); rid=u16(data,p+2); size=u32(data,p+4); obj=u16(data,p+8); off=u32(data,p+10)
        ob=object_bytes(data,h,obj); out[(typ,rid)]=ob[off:off+size]
    return out

def modules(data,h):
    p=h+u32(data,h+0x70); cnt=u32(data,h+0x74); out=[]
    for _ in range(cnt):
        n=data[p]; p+=1; out.append(data[p:p+n].decode('latin1').upper()); p+=n
    return out

def imports(data,h):
    mods=modules(data,h); mpages=u32(data,h+0x14); fpt=h+u32(data,h+0x68); frt=h+u32(data,h+0x6c); ipt=h+u32(data,h+0x78)
    out={}; named={}; shapes=set()
    for page in range(1,mpages+1):
        p=frt+u32(data,fpt+(page-1)*4); end=frt+u32(data,fpt+page*4)
        while p<end:
            typ=data[p]; flags=data[p+1]; p+=2; st=typ&0x0f; kind=flags&3
            if flags&8: raise ValueError('chained fixup')
            if typ&0x20: count=data[p]; p+=1
            else: count=1; p+=2
            if flags&0x40: mid=u16(data,p); p+=2
            else: mid=data[p]; p+=1
            if kind==0:
                if st!=2: p+=4 if flags&0x10 else 2
            elif kind==1:
                if flags&0x80: ordinal=data[p]; p+=1
                elif flags&0x10: ordinal=u32(data,p); p+=4
                else: ordinal=u16(data,p); p+=2
                out[(mods[mid-1],ordinal)]=out.get((mods[mid-1],ordinal),0)+count
            elif kind==2:
                noff=u32(data,p) if flags&0x10 else u16(data,p); p+=4 if flags&0x10 else 2
                q=ipt+noff; n=data[q]; name=data[q+1:q+1+n].decode('latin1')
                named[(mods[mid-1],name)]=named.get((mods[mid-1],name),0)+count
            elif kind!=3: raise ValueError('fixup kind')
            if flags&4: p+=4 if flags&0x20 else 2
            if typ&0x20: p+=count*2
            shapes.add((st,kind))
    return out,named,shapes

def def_ordinals(path):
    got=set()
    for line in open(path):
        m=re.search(r'@(\d+)',line)
        if m: got.add(int(m.group(1)))
    return got

def main():
    root=sys.argv[1] if len(sys.argv)>1 else os.path.join('examples','m31f-jigsaw')
    exe=os.path.join(root,'JIGSAW.EXE'); bmp=os.path.join(root,'YOSEMITE.BMP')
    data=open(exe,'rb').read(); h=u32(data,0x3c)
    if data[h:h+2]!=b'LE': print('M31F JIGSAW FAILED: not LE'); return 1
    # Object flags: code/data must remain 32-bit.
    objtab=h+u32(data,h+0x40); objc=u32(data,h+0x44)
    flags=[u32(data,objtab+i*24+8) for i in range(objc)]
    if any((f & 0x2000)==0 for f in flags):
        print('M31F JIGSAW FAILED: mixed-width object', [hex(f) for f in flags]); return 1
    rr=resources(data,h)
    expected={(1,1):2502,(3,1):86,(4,256):159,(4,257):232,(5,1):70,(8,1):10,(11,1):8,(11,2):10}
    got=dict((k,len(v)) for k,v in rr.items())
    if got!=expected:
        print('M31F JIGSAW FAILED: resources',got); return 1
    imp,named,shapes=imports(data,h)
    if named:
        print('M31F JIGSAW FAILED: unexpected named imports',named); return 1
    if shapes != {(7,0),(8,1)}:
        print('M31F JIGSAW FAILED: non-flat fixups',shapes); return 1
    defs={'PMWIN':def_ordinals('pmwin.def'),'PMGPI':def_ordinals('pmgpi.def'),
          'DOSCALLS':def_ordinals('doscalls.def'),'PMSHAPI':def_ordinals('pmshapi.def')}
    missing={}
    for (mod,ordv),cnt in imp.items():
        if mod in defs and ordv not in defs[mod]: missing.setdefault(mod,[]).append(ordv)
    if missing:
        print('M31F JIGSAW FAILED: uncovered imports',dict((k,sorted(set(v))) for k,v in missing.items())); return 1
    b=open(bmp,'rb').read()
    if b[:2]!=b'BM' or len(b)<26:
        print('M31F JIGSAW FAILED: bad YOSEMITE.BMP'); return 1
    off=u32(b,10); dib=u32(b,14); w=u16(b,18) if dib==12 else u32(b,18); hh=u16(b,20) if dib==12 else u32(b,22)
    planes=u16(b,22) if dib==12 else u16(b,26); bpp=u16(b,24) if dib==12 else u16(b,28)
    if (dib,w,hh,planes,bpp)!=(12,640,480,1,4):
        print('M31F JIGSAW FAILED: bitmap format',(dib,w,hh,planes,bpp)); return 1
    print('M31F JIGSAW binary/import regression PASS')
    print('  flat 32-bit LE, 8 resources, OFF32+REL32 fixups only')
    print('  all %d imported ordinals are present in compatibility .def files' % len(imp))
    print('  YOSEMITE.BMP: OS/2 core header, 640x480, 4bpp, pixels @ %d' % off)
    return 0
if __name__=='__main__': sys.exit(main())
