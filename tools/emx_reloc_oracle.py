#!/usr/bin/env python3
"""M30M development oracle for classic EMX a.out relocation recovery.

This tool is deliberately NOT used by os2host32 at runtime.  It reads the
retained unbound a.out only to score the bound-image inference rules against
the relocation table that emxbind discarded.
"""
import struct, sys

def u32(b,o): return struct.unpack_from('<I',b,o)[0]

def internal(v, text_size, data_bss_size):
    return (0x10000 <= v <= 0x10000 + text_size or
            0x20000 <= v <= 0x20000 + data_bss_size)

def one_modrm(op):
    lo=op&7
    return ((op<=0x3b and lo<=3) or op in (0x62,0x63,0x69,0x6b) or
            0x80<=op<=0x8f or op in (0xc0,0xc1,0xc6,0xc7) or
            0xd0<=op<=0xd3 or 0xd8<=op<=0xdf or op in (0xf6,0xf7,0xfe,0xff))

def two_modrm(op):
    if 0x80<=op<=0x8f: return False
    return op not in (0x05,0x06,0x07,0x08,0x09,0x0b,0x30,0x31,0x32,0x33,
                      0x34,0x35,0x77,0xa0,0xa1,0xa2,0xa8,0xa9,0xaa)

def has_disp32(m,sib=None):
    mod=m>>6; rm=m&7
    if mod==2: return True
    return mod==0 and (rm==5 or (rm==4 and sib is not None and (sib&7)==5))

def immop(op):
    return (op==0x68 or 0xa0<=op<=0xa3 or 0xb8<=op<=0xbf or
            op in (0x05,0x0d,0x15,0x1d,0x25,0x35,0x3d))

def c7imm(t,site):
    for s in range(max(0,site-8),site):
        if t[s]!=0xc7 or s+1>=len(t): continue
        m=t[s+1]; mod=m>>6; rm=m&7; reg=(m>>3)&7
        if reg: continue
        q=s+2; base=None
        if mod!=3 and rm==4:
            if q>=len(t): continue
            base=t[q]&7; q+=1
        if mod==0 and (rm==5 or (rm==4 and base==5)): q+=4
        elif mod==1: q+=1
        elif mod==2: q+=4
        if q==site: return True
    return False

def operand_site(t,p):
    if p>=1 and immop(t[p-1]): return True
    if p>=2:
        op,m=t[p-2],t[p-1]
        if one_modrm(op) and (m&7)!=4 and has_disp32(m): return True
    if p>=3:
        op,m,s=t[p-3],t[p-2],t[p-1]
        if one_modrm(op) and (m&7)==4 and has_disp32(m,s): return True
    if p>=3 and t[p-3]==0x0f:
        op,m=t[p-2],t[p-1]
        if two_modrm(op) and (m&7)!=4 and has_disp32(m): return True
    if p>=4 and t[p-4]==0x0f:
        op,m,s=t[p-3],t[p-2],t[p-1]
        if two_modrm(op) and (m&7)==4 and has_disp32(m,s): return True
    return c7imm(t,p)

def infer(text,data,text_size,data_bss_size):
    marks=set(); ambiguous=0
    cand=lambda v: internal(v,text_size,data_bss_size)
    for p in range(0,len(text)-3):
        v=u32(text,p)
        if not cand(v) or not operand_site(text,p): continue
        if v==0x10000 and p and (text[p-1]==0x68 or 0xb8<=text[p-1]<=0xbf or c7imm(text,p)):
            ambiguous+=1; continue
        marks.add(p)
    aligned=[]
    for p in range(0,len(text)-3,4):
        if p not in marks and cand(u32(text,p)): aligned.append(p)
    i=0
    while i<len(aligned):
        j=i+1
        while j<len(aligned) and aligned[j]==aligned[j-1]+4: j+=1
        if j-i>=2: marks.update(aligned[i:j])
        i=j
    dmarks={p for p in range(0,len(data)-3,4) if cand(u32(data,p))}
    return marks,dmarks,ambiguous

def main(path):
    b=open(path,'rb').read()
    info=u32(b,0); magic=info&0xffff; mid=(info>>16)&0xff
    if mid!=100 or magic not in (0x107,0x10b): raise SystemExit('not i386 EMX a.out')
    text_size,data_size,bss_size=u32(b,4),u32(b,8),u32(b,12)
    trsz,drsz=u32(b,24),u32(b,28)
    textoff=1024 if magic==0x10b else 32
    dataoff=textoff+text_size; troff=dataoff+data_size; droff=troff+trsz
    text=b[textoff:textoff+text_size]; data=b[dataoff:dataoff+data_size]
    truth_t=set(); truth_d=set()
    for off,sz,out in ((troff,trsz,truth_t),(droff,drsz,truth_d)):
        for q in range(off,off+sz,8):
            a,ri=u32(b,q),u32(b,q+4); sym=ri&0xffffff
            pcrel=(ri>>24)&1; length=(ri>>25)&3; ext=(ri>>27)&1
            if length==2 and not ext and not pcrel and sym in (4,6,8): out.add(a)
    inf_t,inf_d,amb=infer(text,data,text_size,data_size+bss_size)
    tp=len((inf_t&truth_t))+len(inf_d&truth_d)
    fp=len(inf_t-truth_t)+len(inf_d-truth_d)
    fn=len(truth_t-inf_t)+len(truth_d-inf_d)
    print('a.out: text=%08X data=%08X bss=%08X' % (text_size,data_size,bss_size))
    print('truth: text=%d data=%d total=%d' % (len(truth_t),len(truth_d),len(truth_t)+len(truth_d)))
    print('infer: text=%d data=%d ambiguous=%d' % (len(inf_t),len(inf_d),amb))
    print('score: TP=%d FP=%d FN=%d' % (tp,fp,fn))
    if inf_t-truth_t:
        print('false-positive TEXT sites:', ' '.join('%08X'%x for x in sorted(inf_t-truth_t)))
    if truth_t-inf_t:
        print('missed TEXT sites:', ' '.join('%08X'%x for x in sorted(truth_t-inf_t)))
    return 0 if fn==0 else 1

if __name__=='__main__':
    if len(sys.argv)!=2: raise SystemExit('usage: emx_reloc_oracle.py unbound-aout')
    raise SystemExit(main(sys.argv[1]))
