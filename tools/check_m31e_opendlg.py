#!/usr/bin/env python
from __future__ import print_function
import os,sys,struct

def u16(b,o): return struct.unpack_from('<H',b,o)[0]
def s16(b,o): return struct.unpack_from('<h',b,o)[0]
def u32(b,o): return struct.unpack_from('<I',b,o)[0]

def object_bytes(data,h,objnum):
    objtab=h+u32(data,h+0x40); pagemap=h+u32(data,h+0x48)
    pagesize=u32(data,h+0x28); datapage=u32(data,h+0x80); last=u32(data,h+0x2c); mpages=u32(data,h+0x14)
    o=objtab+(objnum-1)*24; size=u32(data,o); first=u32(data,o+12); count=u32(data,o+16)
    out=bytearray()
    for i in range(count):
        m=pagemap+(first-1+i)*4
        phys=(data[m]<<16)|(data[m+1]<<8)|data[m+2]; flags=data[m+3]
        if flags==3: out.extend(b'\0'*pagesize); continue
        if flags!=0 or phys==0: raise ValueError('unsupported page')
        n=last if last and phys==mpages else pagesize
        off=datapage+(phys-1)*pagesize
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
    for i in range(cnt):
        n=data[p]; p+=1; out.append(data[p:p+n].decode('latin1')); p+=n
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
            if flags&0x40: first=u16(data,p); p+=2
            else: first=data[p]; p+=1
            target=None
            if kind==0:
                if st!=2: p+=4 if flags&0x10 else 2
            elif kind==1:
                if flags&0x80: target=data[p]; p+=1
                elif flags&0x10: target=u32(data,p); p+=4
                else: target=u16(data,p); p+=2
                out[(mods[first-1],target)]=out.get((mods[first-1],target),0)+count
            elif kind==2:
                if flags&0x10: noff=u32(data,p); p+=4
                else: noff=u16(data,p); p+=2
                q=ipt+noff; n=data[q]; name=data[q+1:q+1+n].decode('latin1')
                named[(mods[first-1],name)]=named.get((mods[first-1],name),0)+count
            elif kind!=3: raise ValueError('fixup kind')
            if flags&4: p+=4 if flags&0x20 else 2
            if typ&0x20: p+=count*2
            shapes.add((st,kind))
    return out,named,shapes

def exports(data,h):
    p=h+u32(data,h+0x5c); ordinal=1; out=set()
    while True:
        count=data[p]; p+=1
        if count==0: break
        typ=data[p]; p+=1; base=typ&0x7f
        if base==0: ordinal+=count; continue
        if base in (1,2,3):
            obj=u16(data,p); p+=2
            for i in range(count):
                flags=data[p]; p+=1
                if base==3: off=u32(data,p); p+=4
                else:
                    off=u16(data,p); p+=2
                    if base==2: p+=2
                if flags&1: out.add(ordinal)
                ordinal+=1
        elif base==4:
            p+=2+count*7; ordinal+=count
        else: raise ValueError('entry bundle')
    return out

def strings(blob):
    if len(blob)<3: return 0,{}
    cp=u16(blob,0); pos=2; out={}
    for idx in range(16):
        if pos>=len(blob): break
        n=blob[pos]; pos+=1
        if n==0 or pos+n>len(blob): break
        raw=blob[pos:pos+n]; pos+=n
        out[idx]=raw.rstrip(b'\0').decode('latin1')
    return cp,out

def main():
    root=sys.argv[1] if len(sys.argv)>1 else os.path.join('examples','m31e-opendlg')
    dll=os.path.join(root,'OPENDLG.DLL'); exe=os.path.join(root,'HELLO','HELLO.EXE')
    db=open(dll,'rb').read(); dh=u32(db,0x3c); eb=open(exe,'rb').read(); eh=u32(eb,0x3c)
    if db[dh:dh+2]!=b'LE' or eb[eh:eh+2]!=b'LE':
        print('M31E OPENDLG check FAILED: expected LE'); return 1
    dr=resources(db,dh); er=resources(eb,eh)
    expected_dr={(4,100):476,(4,101):478,(5,1):187,(11,1):9}
    if dict((k,len(v)) for k,v in dr.items()) != expected_dr:
        print('M31E OPENDLG check FAILED: DLL resources',dict((k,len(v)) for k,v in dr.items())); return 1
    cp,st=strings(dr[(5,1)])
    expected_strings={0:'%%',1:'%% is not a valid filename.',2:'%% not found - Create new file?',3:'Replace existing %%?',4:'%% has changed.  Save current changes?',5:'Error opening %%.',6:'Error creating %%.'}
    if cp!=850 or any(st.get(k)!=v for k,v in expected_strings.items()):
        print('M31E OPENDLG check FAILED: DLL strings',cp,st); return 1
    if dict((k,len(v)) for k,v in er.items()) != {(1,1):1010,(3,1):99,(4,1):286}:
        print('M31E OPENDLG check FAILED: HELLO resources'); return 1
    ex=exports(db,dh)
    if not {21,26,27,28,29}.issubset(ex):
        print('M31E OPENDLG check FAILED: exports',sorted(ex)); return 1
    di,dn,ds=imports(db,dh); hi,hn,hs=imports(eb,eh)
    if dn or hn:
        print('M31E OPENDLG check FAILED: unexpected named imports'); return 1
    if set(k for k in hi if k[0]=='OPENDLG') != {('OPENDLG',26),('OPENDLG',27)}:
        print('M31E OPENDLG check FAILED: HELLO->OPENDLG imports',hi); return 1
    if ds != {(7,0),(8,1)} or hs != {(7,0),(8,1)}:
        print('M31E OPENDLG check FAILED: non-flat fixup shape',ds,hs); return 1
    print('M31E OPENDLG binary/resource regression PASS')
    print('  OPENDLG.DLL: 32-bit LE, exports 21/26/27/28/29, flat OFF32+REL32 fixups')
    print('  DLL resources: Open/Save dialogs, CP850 string table ids 0..6, DLGINCLUDE')
    print('  HELLO.EXE imports OPENDLG.26 SetupDLF and OPENDLG.27 DlgFile')
    print('  HELLO resources: icon, menu, About dialog')
    return 0
if __name__=='__main__': sys.exit(main())
