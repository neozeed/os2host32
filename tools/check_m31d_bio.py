#!/usr/bin/env python
from __future__ import print_function
import os,sys

def u16(b,o): return b[o] | (b[o+1]<<8)
def u32(b,o): return b[o] | (b[o+1]<<8) | (b[o+2]<<16) | (b[o+3]<<24)

def object_bytes(data,h,objnum):
    objtab=h+u32(data,h+0x40); pagemap=h+u32(data,h+0x48)
    pagesize=u32(data,h+0x28); datapage=u32(data,h+0x80); last=u32(data,h+0x2c)
    o=objtab+(objnum-1)*24; size=u32(data,o); first=u32(data,o+12); count=u32(data,o+16)
    out=bytearray()
    for i in range(count):
        m=pagemap+(first-1+i)*4
        phys=(data[m]<<16)|(data[m+1]<<8)|data[m+2]; flags=data[m+3]
        if flags==3: out.extend(b'\0'*pagesize); continue
        if flags!=0 or phys==0: raise ValueError('unsupported page')
        n=pagesize
        if last and phys==u32(data,h+0x14): n=last
        off=datapage+(phys-1)*pagesize
        out.extend(data[off:off+n]); out.extend(b'\0'*(pagesize-n))
    return bytes(out[:size])

def parse_dialog(blob):
    if len(blob)<44: raise ValueError('short dialog')
    offitems=u16(blob,6); cp=u16(blob,4)
    root=offitems; children=u16(blob,root+2); items=[]
    for i in range(children+1):
        o=offitems+i*30
        if o+30>len(blob): raise ValueError('dialog overrun')
        cls=u16(blob,o+6); cch=u16(blob,o+8); toff=u16(blob,o+10); ident=u16(blob,o+24); style=u32(blob,o+12)
        text=None
        if cch and toff+cch<=len(blob):
            raw=blob[toff:toff+cch]
            if cch==3 and raw[0]==0xff: text=('resource',u16(raw,1))
            else: text=raw.rstrip(b'\0').decode('latin1')
        items.append((cls,ident,style,text))
    return cp,items

def strings(blob):
    if len(blob)<3: return 0,{}
    cp=u16(blob,0); pos=2; out={}; idx=0
    while pos<len(blob) and idx<16:
        n=blob[pos]; pos+=1
        if not n or pos+n>len(blob): break
        raw=blob[pos:pos+n]; pos+=n
        out[idx]=raw.rstrip(b'\0').decode('latin1'); idx+=1
    return cp,out

def main():
    exe=sys.argv[1] if len(sys.argv)>1 else os.path.join('examples','m31d-bio','BIO.EXE')
    ico=sys.argv[2] if len(sys.argv)>2 else os.path.join('examples','m31d-bio','BIO.ICO')
    data=open(exe,'rb').read(); h=u32(data,0x3c)
    if data[h:h+2]!=b'LE': print('M31D BIO check FAILED: not LE'); return 1
    rtab=h+u32(data,h+0x50); rcnt=u32(data,h+0x54); res={}
    for i in range(rcnt):
        p=rtab+i*14; typ=u16(data,p); rid=u16(data,p+2); size=u32(data,p+4); obj=u16(data,p+8); off=u32(data,p+10)
        ob=object_bytes(data,h,obj); res[(typ,rid)]=ob[off:off+size]
    expected={(1,1):1010,(3,1):120,(4,2):274,(4,3):655,(5,1):49,(8,1):22}
    got=dict((k,len(v)) for k,v in res.items())
    if got!=expected: print('M31D BIO check FAILED: resources',got); return 1
    if res[(1,1)]!=open(ico,'rb').read(): print('M31D BIO check FAILED: icon'); return 1
    cp,st=strings(res[(5,1)])
    if cp!=850 or st.get(1)!='Biorhythm' or st.get(2)!='Legend': print('M31D BIO check FAILED: strings',cp,st); return 1
    cp2,d2=parse_dialog(res[(4,2)]); cp3,d3=parse_dialog(res[(4,3)])
    if cp2!=850 or cp3!=850 or len(d2)!=6 or len(d3)!=18: print('M31D BIO check FAILED: dialogs',len(d2),len(d3)); return 1
    a=res[(8,1)]
    if u16(a,0)!=3 or u16(a,2)!=850: print('M31D BIO check FAILED: accel header'); return 1
    acc=[(u16(a,4+i*6),u16(a,6+i*6),u16(a,8+i*6)) for i in range(3)]
    if acc!=[(0x11,ord('d'),0x100),(0x11,ord('l'),0x101),(0x11,ord('c'),0x102)]: print('M31D BIO check FAILED: accel',acc); return 1
    print('M31D BIO resource regression PASS')
    print('  strings: Biorhythm / Legend')
    print('  dialogs: About + 17-control Dates dialog')
    print('  accelerators: Ctrl+D / Ctrl+L / Ctrl+C')
    return 0
if __name__=='__main__': sys.exit(main())
