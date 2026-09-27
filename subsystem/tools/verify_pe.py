#!/usr/bin/env python3
import argparse, struct
from pathlib import Path
p=argparse.ArgumentParser(); p.add_argument('pe'); a=p.parse_args()
d=Path(a.pe).read_bytes()
if d[:2]!=b'MZ': raise SystemExit('not MZ')
e=struct.unpack_from('<I',d,0x3c)[0]
if d[e:e+4]!=b'PE\0\0': raise SystemExit('not PE')
machine,nsects,_,_,_,opsize,chars=struct.unpack_from('<HHIIIHH',d,e+4)
opt=e+24
magic=struct.unpack_from('<H',d,opt)[0]
sub=struct.unpack_from('<H',d,opt+68)[0]
entry=struct.unpack_from('<I',d,opt+16)[0]
print(f'file={a.pe}')
print(f'machine=0x{machine:04x}')
print(f'optional_magic=0x{magic:04x}')
print(f'subsystem={sub}')
print(f'entry_rva=0x{entry:08x}')
print(f'sections={nsects}')
