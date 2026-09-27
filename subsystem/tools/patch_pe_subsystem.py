#!/usr/bin/env python3
import argparse, struct
from pathlib import Path

p=argparse.ArgumentParser()
p.add_argument('pe')
p.add_argument('subsystem', type=lambda x:int(x,0))
a=p.parse_args()
path=Path(a.pe)
data=bytearray(path.read_bytes())
if data[:2] != b'MZ': raise SystemExit('not MZ')
e_lfanew=struct.unpack_from('<I', data, 0x3c)[0]
if data[e_lfanew:e_lfanew+4] != b'PE\0\0': raise SystemExit('not PE')
opt=e_lfanew+24
magic=struct.unpack_from('<H',data,opt)[0]
if magic not in (0x10b,0x20b): raise SystemExit(f'bad optional header magic {magic:#x}')
off=opt+68
old=struct.unpack_from('<H',data,off)[0]
struct.pack_into('<H',data,off,a.subsystem)
path.write_bytes(data)
print(f'{path}: subsystem {old} -> {a.subsystem}')
