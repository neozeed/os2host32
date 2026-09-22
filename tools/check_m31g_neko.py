#!/usr/bin/env python
from __future__ import print_function
import hashlib, os, struct, sys

def u16(b,o): return struct.unpack_from('<H', b, o)[0]
def u32(b,o): return struct.unpack_from('<I', b, o)[0]

def lx_iterated(data, src, encoded_size, logical_size):
    end = src + encoded_size
    if src < 0 or end > len(data):
        raise ValueError('iterated data outside file')
    p = src
    out = bytearray()
    records = 0
    while len(out) < logical_size:
        if end - p < 4:
            raise ValueError('truncated iterated record')
        repeat = u16(data,p); pattern_len = u16(data,p+2); p += 4
        if repeat == 0 or pattern_len == 0:
            raise ValueError('zero repeat/pattern in iterated record')
        if p + pattern_len > end:
            raise ValueError('truncated iterated pattern')
        pattern = data[p:p+pattern_len]; p += pattern_len
        if repeat > (logical_size - len(out)) // pattern_len:
            raise ValueError('iterated page expands beyond logical size')
        for _ in range(repeat):
            out.extend(pattern)
        records += 1
    if len(out) != logical_size:
        raise ValueError('iterated page decoded to wrong size')
    if p != end:
        raise ValueError('trailing bytes in iterated page')
    return bytes(out), records

def modules(data,h):
    p=h+u32(data,h+0x70); cnt=u32(data,h+0x74); out=[]
    for _ in range(cnt):
        n=data[p]; p+=1; out.append(data[p:p+n].decode('latin1').upper()); p+=n
    return out

def imports(data,h):
    mods=modules(data,h); mpages=u32(data,h+0x14)
    fpt=h+u32(data,h+0x68); frt=h+u32(data,h+0x6c); ipt=h+u32(data,h+0x78)
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
            elif kind!=3:
                raise ValueError('unknown fixup kind')
            if flags&4: p+=4 if flags&0x20 else 2
            if typ&0x20: p+=count*2
            shapes.add((st,kind))
    return out,named,shapes

def def_ordinals(path):
    import re
    got=set()
    if not os.path.exists(path): return got
    for line in open(path):
        m=re.search(r'@(\d+)',line)
        if m: got.add(int(m.group(1)))
    return got

def main():
    root=sys.argv[1] if len(sys.argv)>1 else os.path.join('examples','m31g-neko')
    exe=os.path.join(root,'NEKO.EXE')
    data=open(exe,'rb').read()
    sha=hashlib.sha256(data).hexdigest()
    expected_sha='3315d6ccbb8ec2f2a1ffb53c3444257bf798f7af4cad9527738cb54cb1d45b0b'
    if sha != expected_sha:
        print('M31G NEKO FAILED: NEKO.EXE differs from frozen specimen',sha); return 1
    if len(data) != 22283:
        print('M31G NEKO FAILED: unexpected file size',len(data)); return 1
    h=u32(data,0x3c)
    if data[h:h+2] != b'LX':
        print('M31G NEKO FAILED: not LX'); return 1
    pagesize=u32(data,h+0x28); shift=u32(data,h+0x2c); mpages=u32(data,h+0x14)
    objtab=h+u32(data,h+0x40); objc=u32(data,h+0x44); pagemap=h+u32(data,h+0x48)
    iterbase=u32(data,h+0x4c); database=u32(data,h+0x80)
    if (pagesize,shift,mpages,objc)!=(4096,2,6,3):
        print('M31G NEKO FAILED: unexpected LX geometry',(pagesize,shift,mpages,objc)); return 1
    if iterbase != database or iterbase != 0x1060:
        print('M31G NEKO FAILED: unexpected iterated/data base',hex(iterbase),hex(database)); return 1
    obj=[]
    for i in range(objc):
        o=objtab+i*24
        obj.append((u32(data,o),u32(data,o+4),u32(data,o+8),u32(data,o+12),u32(data,o+16)))
    if any((o[2] & 0x2000)==0 for o in obj):
        print('M31G NEKO FAILED: mixed-width LX object', [hex(o[2]) for o in obj]); return 1
    # This specimen contains exactly two standard LX iterated pages.
    iter_pages=[]
    for idx in range(mpages):
        m=pagemap+idx*8
        off=u32(data,m); enc=u16(data,m+4); flags=u16(data,m+6)
        if flags == 1:
            # Find logical size from owning object / final partial page.
            logical=pagesize
            for size,base,oflags,first,count in obj:
                if first-1 <= idx < first-1+count:
                    room=size-(idx-(first-1))*pagesize
                    logical=min(pagesize,room)
                    break
            dec,recs=lx_iterated(data, iterbase+(off<<shift), enc, logical)
            iter_pages.append((idx+1,enc,len(dec),recs))
        elif flags not in (0,3):
            print('M31G NEKO FAILED: unexpected LX page flag',idx+1,flags); return 1
    expected_iter=[(5,1856,4096,62),(6,2622,3244,146)]
    if iter_pages != expected_iter:
        print('M31G NEKO FAILED: iterated page inventory',iter_pages); return 1
    # Resource table: one RT_POINTER/bitmap-style resource entry, exactly as scanned by host.
    rtab=h+u32(data,h+0x50); rcnt=u32(data,h+0x54)
    if rcnt != 1:
        print('M31G NEKO FAILED: resource count',rcnt); return 1
    r=(u16(data,rtab),u16(data,rtab+2),u32(data,rtab+4),u16(data,rtab+8),u32(data,rtab+10))
    if r != (1,1,3242,3,0):
        print('M31G NEKO FAILED: resource tuple',r); return 1
    imp,named,shapes=imports(data,h)
    if named:
        print('M31G NEKO FAILED: unexpected named imports',named); return 1
    if shapes != {(7,0),(8,1)}:
        print('M31G NEKO FAILED: non-flat fixups',shapes); return 1
    if len(imp) != 63:
        print('M31G NEKO FAILED: expected 63 distinct ordinal imports, got',len(imp)); return 1
    expected_modules=['DOSCALLS','PMGPI','PMWIN','PMSHAPI','HELPMGR','PMWP']
    if modules(data,h) != expected_modules:
        print('M31G NEKO FAILED: import modules',modules(data,h)); return 1
    defs={'PMWIN':def_ordinals('pmwin.def'),'PMGPI':def_ordinals('pmgpi.def'),
          'DOSCALLS':def_ordinals('doscalls.def'),'PMSHAPI':def_ordinals('pmshapi.def'),
          'PMWP':def_ordinals('pmwp.def'),'HELPMGR':def_ordinals('helpmgr.def')}
    missing=[]
    for mod,ordv in sorted(imp):
        if mod not in defs or ordv not in defs[mod]: missing.append('%s.%d'%(mod,ordv))
    src=open('os2host32.c','r').read()
    required=['LX_PAGE_ITERATED','iter_pages','expand_lx_iterated_page','truncated LX iterated page record']
    absent=[x for x in required if x not in src]
    if absent:
        print('M31G NEKO FAILED: loader iterated-page support absent',absent); return 1
    print('M31G NEKO R1 binary/loader regression PASS')
    print('  untouched NEKO.EXE sha256:',sha)
    print('  flat 32-bit LX: 3 objects, OFF32+REL32 fixups only, 1 resource')
    print('  iterated page 5: 1856 encoded -> 4096 bytes (62 records)')
    print('  iterated page 6: 2622 encoded -> 3244 bytes (146 records)')
    print('  63 distinct ordinal imports across 6 modules; %d remain outside current .def coverage' % len(missing))
    if missing:
        print('  next import/API tranche:', ', '.join(missing))
    return 0

if __name__=='__main__': sys.exit(main())
