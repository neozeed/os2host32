#!/usr/bin/env python
from __future__ import print_function
import os, sys

def u16(b,o): return b[o] | (b[o+1] << 8)
def u32(b,o): return b[o] | (b[o+1] << 8) | (b[o+2] << 16) | (b[o+3] << 24)

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
    cb,typ,cp,offitems,fs,focus,coff=[u16(blob,i) for i in range(0,14,2)]
    if cb != len(blob) or offitems+30>len(blob): raise ValueError('dialog header')
    root=offitems
    children=u16(blob,root+2)
    items=[]
    for i in range(children+1):
        o=offitems+i*30
        if o+30>len(blob): raise ValueError('dialog item overrun')
        cls=u16(blob,o+6); cch=u16(blob,o+8); toff=u16(blob,o+10); ident=u16(blob,o+24)
        text=None
        if cch and toff+cch<=len(blob):
            raw=blob[toff:toff+cch]
            if cch==3 and raw[0]==0xff: text=('resource',u16(raw,1))
            else: text=raw.decode('latin1')
        items.append((cls,ident,text))
    return cp,items

def main():
    exe=sys.argv[1] if len(sys.argv)>1 else os.path.join('examples','m31c-hanoi','HANOI.EXE')
    ico=sys.argv[2] if len(sys.argv)>2 else os.path.join('examples','m31c-hanoi','HANOI.ICO')
    data=open(exe,'rb').read(); h=u32(data,0x3c)
    if data[h:h+2] != b'LE': print('M31C HANOI check FAILED: not LE'); return 1
    rtab=h+u32(data,h+0x50); rcnt=u32(data,h+0x54); res={}
    for i in range(rcnt):
        p=rtab+i*14; typ=u16(data,p); rid=u16(data,p+2); size=u32(data,p+4); obj=u16(data,p+8); off=u32(data,p+10)
        ob=object_bytes(data,h,obj); res[(typ,rid)]=ob[off:off+size]
    expected={(1,1):2152,(3,1):124,(4,6):215,(4,9):280,(8,1):10}
    got=dict((k,len(v)) for k,v in res.items())
    if got != expected:
        print('M31C HANOI check FAILED: resources',got); return 1
    if res[(1,1)] != open(ico,'rb').read():
        print('M31C HANOI check FAILED: icon mismatch'); return 1
    cp6,it6=parse_dialog(res[(4,6)]); cp9,it9=parse_dialog(res[(4,9)])
    if cp6!=850 or cp9!=850 or len(it6)!=5 or len(it9)!=6:
        print('M31C HANOI check FAILED: dialog templates'); return 1
    if it6[2][0]!=6 or it6[2][1]!=7: # entry field
        print('M31C HANOI check FAILED: set dialog entryfield'); return 1
    if it9[1][2] != ('resource',1):
        print('M31C HANOI check FAILED: about icon control'); return 1
    a=res[(8,1)]
    if not (u16(a,0)==1 and u16(a,2)==850 and u16(a,4)==0x21 and u16(a,6)==ord('s') and u16(a,8)==4):
        print('M31C HANOI check FAILED: accelerator'); return 1
    print('M31C HANOI resource regression PASS')
    print('  resources: icon, menu, dialogs 6/9, accelerator table')
    print('  set dialog: static + entryfield + OK/Cancel')
    print('  about dialog: icon + three static labels + OK')
    print('  accelerator: Alt+s -> IDM_SET (4)')
    return 0
if __name__=='__main__': sys.exit(main())
