#!/usr/bin/env python3
"""Real LE calls to QuerySysInfo/AllocMem/FreeMem, executed by Tiny386."""
import pathlib, sys
from mkfixtures import Code, DATA_BASE, PAGE, make_image, write_call, exit_call

c=Code(); failures=[]; data=bytearray(PAGE)
ok=b'soft386: guest LE jar memory budget PASS\n'
bad=b'soft386: guest LE jar memory budget FAIL\n'
data[:len(ok)]=ok; data[0x80:0x80+len(bad)]=bad
out=DATA_BASE+0x100; ptr=DATA_BASE+0x120; actual=DATA_BASE+0x124

def success():
    c.test_eax(); failures.append(c.jnz32())
def equal(addr,expected):
    c.mov_eax_abs(addr); c.cmp_eax_imm32(expected); failures.append(c.jnz32())
def query(free_bytes,largest):
    c.push8(20); c.push32(out); c.push8(21); c.push8(17); c.call_import(348); c.addesp(16); success()
    for i,n in enumerate([64<<20,(64<<20)-free_bytes,free_bytes,largest,largest]): equal(out+4*i,n)

query(32<<20,32<<20)
c.push8(0); c.push8(0x13); c.push32(4<<20); c.push32(ptr); c.call_import(299); c.addesp(16); success()
query(28<<20,28<<20)
c.push_mem32(ptr); c.call_import(304); c.addesp(4); success()
query(32<<20,28<<20)
write_call(c,DATA_BASE,len(ok),actual); exit_call(c,0)
fail=len(c.b)
for off in failures: c.patch_rel32(off,fail)
write_call(c,DATA_BASE+0x80,len(bad),actual); exit_call(c,1)
p=pathlib.Path(sys.argv[1]); p.parent.mkdir(parents=True,exist_ok=True)
p.write_bytes(make_image(bytes(c.b),bytes(data),c.fix,False))
