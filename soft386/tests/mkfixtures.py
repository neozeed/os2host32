#!/usr/bin/env python3
import struct, pathlib, sys

CODE_BASE=0x00010000
DATA_BASE=0x00020000
PAGE=0x1000
DOSCALLS=1
VIOCALLS=2
KBDCALLS=3
SESMGR=4
NLSMOD=5

class Code:
    def __init__(self): self.b=bytearray(); self.fix=[]
    def u8(self,*x): self.b.extend(x)
    def u32(self,x): self.b.extend(struct.pack('<I',x & 0xffffffff))
    def push32(self,x): self.u8(0x68); self.u32(x)
    def push8(self,x): self.u8(0x6a,x&0xff)
    def push_mem32(self,addr): self.u8(0xff,0x35); self.u32(addr)
    def push_internal(self,obj,target=0):
        self.u8(0x68); off=len(self.b); self.u32(0); self.fix.append(('internal',off,obj,target))
    def call_import(self,ordinal,module=DOSCALLS):
        self.u8(0xe8); off=len(self.b); self.u32(0); self.fix.append(('external',off,module,ordinal))
    def addesp(self,n): self.u8(0x83,0xc4,n&0xff)
    def mov_eax_abs(self,addr): self.u8(0xa1); self.u32(addr)
    def mov_abs_imm32(self,addr,val): self.u8(0xc7,0x05); self.u32(addr); self.u32(val)
    def test_eax(self): self.u8(0x85,0xc0)
    def cmp_eax_imm32(self,val): self.u8(0x3d); self.u32(val)
    def jnz32(self): self.u8(0x0f,0x85); off=len(self.b); self.u32(0); return off
    def patch_rel32(self,off,target): struct.pack_into('<i',self.b,off,target-(off+4))

def write_call(c,msg_addr,msg_len,actual_addr):
    c.push32(actual_addr); c.push32(msg_len); c.push32(msg_addr); c.push8(1); c.call_import(282); c.addesp(16)

def exit_call(c,rc=0):
    c.push8(rc); c.push8(1); c.call_import(234); c.addesp(8); c.u8(0xf4)

def hello_program():
    data=bytearray(PAGE)
    msg=b"soft386: untouched 32-bit OS/2 LE/LX says hello!\n"
    data[0:len(msg)]=msg
    actual=0x100
    c=Code(); write_call(c,DATA_BASE,len(msg),DATA_BASE+actual); exit_call(c,0)
    return bytes(c.b),bytes(data),c.fix



def internal_fixup_program():
    data=bytearray(PAGE)
    msg=b"soft386: internal OFF32 fixup PASS\n"
    data[0:len(msg)]=msg
    actual=DATA_BASE+0x100
    c=Code()
    c.push32(actual); c.push32(len(msg)); c.push_internal(2,0); c.push8(1); c.call_import(282); c.addesp(16)
    exit_call(c,0)
    return bytes(c.b),bytes(data),c.fix

def hi_surface_program():
    """Exercise the exact DOSCALLS import surface of the historical hi.exe."""
    data=bytearray(PAGE)
    msg=b"soft386: historical hi.exe DOSCALLS surface PASS\n"
    data[0:len(msg)]=msg
    qsv=DATA_BASE+0x100
    allocp=DATA_BASE+0x108
    htype=DATA_BASE+0x10c
    hattr=DATA_BASE+0x110
    newpos=DATA_BASE+0x114
    actual=DATA_BASE+0x118
    c=Code()
    # DosQuerySysInfo(11,12,qsv,8)
    c.push8(8); c.push32(qsv); c.push8(12); c.push8(11); c.call_import(348); c.addesp(16)
    # prerelease C/386 DosAllocMem(&p,0x1000,flags,reserved)
    c.push8(0); c.push32(0x13); c.push32(0x1000); c.push32(allocp); c.call_import(299); c.addesp(16)
    # DosSetMem(p,0x1000,flags)
    c.push32(0x13); c.push32(0x1000); c.push_mem32(allocp); c.call_import(305); c.addesp(12)
    # DosQueryHType(stdout,&type,&attr)
    c.push32(hattr); c.push32(htype); c.push8(1); c.call_import(224); c.addesp(12)
    # DosSetFilePtr is imported by historical hi.exe; exercise the route even
    # though seeking a console/pipe may legitimately return an OS/2 error.
    c.push32(newpos); c.push8(1); c.push8(0); c.push8(1); c.call_import(256); c.addesp(16)
    write_call(c,DATA_BASE,len(msg),actual)
    # DosFreeMem(p)
    c.push_mem32(allocp); c.call_import(304); c.addesp(4)
    exit_call(c,0)
    return bytes(c.b),bytes(data),c.fix

def thread_program():
    data=bytearray(PAGE)
    tmsg=b"soft386: worker thread ran\n"
    mmsg=b"soft386: main resumed after DosWaitThread\n"
    data[0:len(tmsg)]=tmsg
    moff=0x80; data[moff:moff+len(mmsg)]=mmsg
    tid=0x180; actual=0x184
    c=Code()
    # DosCreateThread(&tid, thread_entry, 0x12345678, 0, 0x4000)
    c.push32(0x4000); c.push8(0); c.push32(0x12345678)
    entry_imm_pos=len(c.b)+1; c.push32(0)  # patched after thread offset known
    c.push32(DATA_BASE+tid); c.call_import(311); c.addesp(20)
    # DosWaitThread(&tid, DCWW_WAIT)
    c.push8(0); c.push32(DATA_BASE+tid); c.call_import(349); c.addesp(8)
    write_call(c,DATA_BASE+moff,len(mmsg),DATA_BASE+actual)
    exit_call(c,0)
    thread_off=len(c.b)
    struct.pack_into('<I',c.b,entry_imm_pos,CODE_BASE+thread_off)
    write_call(c,DATA_BASE,len(tmsg),DATA_BASE+actual)
    c.u8(0xc3) # returns to runtime thread-exit veneer
    return bytes(c.b),bytes(data),c.fix

def native_file_program():
    """Windows-only proof for the marshalled native DOSCALLS HFILE namespace."""
    data=bytearray(PAGE)
    path=b"soft386-r1-native.tmp\0"
    payload=b"soft386: native DOSCALLS file bridge PASS\n"
    data[0:len(path)]=path
    poff=0x80; data[poff:poff+len(payload)]=payload
    readbuf=0x180; hf=0x300; action=0x304; actual=0x308; newpos=0x30c
    c=Code(); failures=[]
    # Prerelease 8-argument guest DosOpen ABI.  Bridge adapts to native DLL's 9 args.
    c.push8(0); c.push32(0x42); c.push32(0x12); c.push8(0); c.push8(0)
    c.push32(DATA_BASE+action); c.push32(DATA_BASE+hf); c.push32(DATA_BASE); c.call_import(273); c.addesp(32)
    c.test_eax(); failures.append(c.jnz32())
    # Write payload to the DLL-owned HFILE.
    c.push32(DATA_BASE+actual); c.push32(len(payload)); c.push32(DATA_BASE+poff); c.push_mem32(DATA_BASE+hf); c.call_import(282); c.addesp(16)
    c.test_eax(); failures.append(c.jnz32())
    c.mov_eax_abs(DATA_BASE+actual); c.cmp_eax_imm32(len(payload)); failures.append(c.jnz32())
    # Seek back and read through a copied-out native buffer.
    c.push32(DATA_BASE+newpos); c.push8(0); c.push8(0); c.push_mem32(DATA_BASE+hf); c.call_import(256); c.addesp(16)
    c.test_eax(); failures.append(c.jnz32())
    c.mov_eax_abs(DATA_BASE+newpos); c.cmp_eax_imm32(0); failures.append(c.jnz32())
    c.push32(DATA_BASE+actual); c.push32(len(payload)); c.push32(DATA_BASE+readbuf); c.push_mem32(DATA_BASE+hf); c.call_import(281); c.addesp(16)
    c.test_eax(); failures.append(c.jnz32())
    c.mov_eax_abs(DATA_BASE+actual); c.cmp_eax_imm32(len(payload)); failures.append(c.jnz32())
    # Close the file before deleting it.
    c.push_mem32(DATA_BASE+hf); c.call_import(257); c.addesp(4)
    c.test_eax(); failures.append(c.jnz32())
    # Send the bytes read back from native storage to native stdout.
    c.push32(DATA_BASE+actual); c.push32(len(payload)); c.push32(DATA_BASE+readbuf); c.push8(1); c.call_import(282); c.addesp(16)
    c.test_eax(); failures.append(c.jnz32())
    # Cleanup the temporary file.
    c.push8(0); c.push32(DATA_BASE); c.call_import(259); c.addesp(8)
    c.test_eax(); failures.append(c.jnz32())
    exit_call(c,0)
    fail_off=len(c.b)
    for off in failures: c.patch_rel32(off,fail_off)
    exit_call(c,1)
    return bytes(c.b),bytes(data),c.fix

def pipe_producer_program():
    """Small real LE producer for CMD32 process/pipeline acceptance on Windows."""
    data=bytearray(PAGE)
    msg=b"SOFT386_PIPE_PASS\n"
    data[0:len(msg)]=msg
    c=Code(); write_call(c,DATA_BASE,len(msg),DATA_BASE+0x300); exit_call(c,0)
    return bytes(c.b),bytes(data),c.fix

def pipe_consumer_program():
    """Read inherited HFILE 0 and copy exactly what was read to HFILE 1."""
    data=bytearray(PAGE)
    buf=DATA_BASE+0x100; actual=DATA_BASE+0x300; wrote=DATA_BASE+0x304
    c=Code(); failures=[]
    c.push32(actual);c.push8(64);c.push32(buf);c.push8(0);c.call_import(281);c.addesp(16)
    c.test_eax();failures.append(c.jnz32())
    c.push32(wrote);c.push_mem32(actual);c.push32(buf);c.push8(1);c.call_import(282);c.addesp(16)
    c.test_eax();failures.append(c.jnz32())
    exit_call(c,0)
    fail=len(c.b)
    for off in failures:c.patch_rel32(off,fail)
    exit_call(c,1)
    return bytes(c.b),bytes(data),c.fix

def memory_program():
    """Prove allocation/protection/query/free stay in the virtual OS/2 address space."""
    data=bytearray(PAGE)
    ok=b"soft386: jar memory semantics PASS\n"
    bad=b"soft386: jar memory semantics FAIL\n"
    data[0:len(ok)]=ok
    bad_off=0x60; data[bad_off:bad_off+len(bad)]=bad
    p=0x180; cb=0x184; fl=0x188; actual=0x18c
    c=Code(); failures=[]
    # DosAllocMem(&p,0x2000,PAG_READ|PAG_WRITE|PAG_COMMIT,0)
    c.push8(0); c.push32(0x13); c.push32(0x2000); c.push32(DATA_BASE+p); c.call_import(299); c.addesp(16)
    c.test_eax(); failures.append(c.jnz32())
    # Query the full allocation; requested cb deliberately exceeds it.
    c.mov_abs_imm32(DATA_BASE+cb,0x3000)
    c.push32(DATA_BASE+fl); c.push32(DATA_BASE+cb); c.push_mem32(DATA_BASE+p); c.call_import(306); c.addesp(12)
    c.test_eax(); failures.append(c.jnz32())
    c.mov_eax_abs(DATA_BASE+cb); c.cmp_eax_imm32(0x2000); failures.append(c.jnz32())
    c.mov_eax_abs(DATA_BASE+fl); c.cmp_eax_imm32(0x00010013); failures.append(c.jnz32())
    # Change OS/2-visible protection state to READ|COMMIT and query it again.
    c.push32(0x11); c.push32(0x2000); c.push_mem32(DATA_BASE+p); c.call_import(305); c.addesp(12)
    c.test_eax(); failures.append(c.jnz32())
    c.mov_abs_imm32(DATA_BASE+cb,0x2000)
    c.push32(DATA_BASE+fl); c.push32(DATA_BASE+cb); c.push_mem32(DATA_BASE+p); c.call_import(306); c.addesp(12)
    c.test_eax(); failures.append(c.jnz32())
    c.mov_eax_abs(DATA_BASE+fl); c.cmp_eax_imm32(0x00010011); failures.append(c.jnz32())
    # Free.  A subsequent query must fail because the allocation is no longer live.
    c.push_mem32(DATA_BASE+p); c.call_import(304); c.addesp(4)
    c.test_eax(); failures.append(c.jnz32())
    c.mov_abs_imm32(DATA_BASE+cb,0x2000)
    c.push32(DATA_BASE+fl); c.push32(DATA_BASE+cb); c.push_mem32(DATA_BASE+p); c.call_import(306); c.addesp(12)
    c.test_eax()
    # Query after free returning zero would be a failure; JZ uses rel32.
    c.u8(0x0f,0x84); post_free_bad=len(c.b); c.u32(0)
    write_call(c,DATA_BASE,len(ok),DATA_BASE+actual); exit_call(c,0)
    fail_off=len(c.b)
    for off in failures: c.patch_rel32(off,fail_off)
    c.patch_rel32(post_free_bad,fail_off)
    write_call(c,DATA_BASE+bad_off,len(bad),DATA_BASE+actual); exit_call(c,1)
    return bytes(c.b),bytes(data),c.fix

def sync_program():
    data=bytearray(PAGE)
    wmsg=b"soft386: jar mutex/event worker PASS\n"
    mmsg=b"soft386: jar semaphore main resumed PASS\n"
    data[0:len(wmsg)]=wmsg
    moff=0x80; data[moff:moff+len(mmsg)]=mmsg
    hev=0x180; hmtx=0x184; tid=0x188; actual=0x18c
    c=Code()
    # DosCreateEventSem(NULL,&hev,0,FALSE)
    c.push8(0); c.push8(0); c.push32(DATA_BASE+hev); c.push8(0); c.call_import(324); c.addesp(16)
    # DosCreateMutexSem(NULL,&hmtx,0,FALSE)
    c.push8(0); c.push8(0); c.push32(DATA_BASE+hmtx); c.push8(0); c.call_import(331); c.addesp(16)
    # Main takes mutex.
    c.push8(0); c.push_mem32(DATA_BASE+hmtx); c.call_import(334); c.addesp(8)
    # Create worker.
    c.push32(0x4000); c.push8(0); c.push8(0)
    entry_imm_pos=len(c.b)+1; c.push32(0)
    c.push32(DATA_BASE+tid); c.call_import(311); c.addesp(20)
    # Sleep briefly: worker runs and blocks on the mutex.
    c.push8(1); c.call_import(229); c.addesp(4)
    # Release -> mutex ownership is handed to worker.
    c.push_mem32(DATA_BASE+hmtx); c.call_import(335); c.addesp(4)
    # Wait for worker event.
    c.push32(0xffffffff); c.push_mem32(DATA_BASE+hev); c.call_import(329); c.addesp(8)
    write_call(c,DATA_BASE+moff,len(mmsg),DATA_BASE+actual)
    # Close objects after no waiters/owners remain.
    c.push_mem32(DATA_BASE+hmtx); c.call_import(333); c.addesp(4)
    c.push_mem32(DATA_BASE+hev); c.call_import(326); c.addesp(4)
    exit_call(c,0)
    thread_off=len(c.b); struct.pack_into('<I',c.b,entry_imm_pos,CODE_BASE+thread_off)
    # Worker requests mutex indefinitely, writes, releases, posts event, returns.
    c.push32(0xffffffff); c.push_mem32(DATA_BASE+hmtx); c.call_import(334); c.addesp(8)
    write_call(c,DATA_BASE,len(wmsg),DATA_BASE+actual)
    c.push_mem32(DATA_BASE+hmtx); c.call_import(335); c.addesp(4)
    c.push_mem32(DATA_BASE+hev); c.call_import(328); c.addesp(4)
    c.u8(0xc3)
    return bytes(c.b),bytes(data),c.fix


def fpu_program():
    """Exercise basic/transcendental x87 operations inside Tiny386."""
    data=bytearray(PAGE)
    ok=b"soft386: 80387 x87 math PASS\n"
    data[0:len(ok)]=ok
    r0=DATA_BASE+0x180; r1=DATA_BASE+0x184; r2=DATA_BASE+0x188; actual=DATA_BASE+0x18c
    c=Code(); failures=[]
    # FNINIT; sqrt(1+1)^2 -> 2
    c.u8(0xdb,0xe3, 0xd9,0xe8, 0xd9,0xe8, 0xd8,0xc1, 0xd9,0xfa, 0xd8,0xc8)
    c.u8(0xdb,0x1d); c.u32(r0)  # FISTP dword [r0]
    c.mov_eax_abs(r0); c.cmp_eax_imm32(2); failures.append(c.jnz32())
    # sin(pi) -> 0
    c.u8(0xd9,0xeb, 0xd9,0xfe, 0xdb,0x1d); c.u32(r1)
    c.mov_eax_abs(r1); c.cmp_eax_imm32(0); failures.append(c.jnz32())
    # cos(0) -> 1
    c.u8(0xd9,0xee, 0xd9,0xff, 0xdb,0x1d); c.u32(r2)
    c.mov_eax_abs(r2); c.cmp_eax_imm32(1); failures.append(c.jnz32())
    write_call(c,DATA_BASE,len(ok),actual); exit_call(c,0)
    fail=len(c.b)
    for off in failures:c.patch_rel32(off,fail)
    exit_call(c,1)
    return bytes(c.b),bytes(data),c.fix

def fpu_thread_program():
    """Prove x87 state is isolated by the saved-context guest scheduler."""
    data=bytearray(PAGE)
    ok=b"soft386: per-thread 80387 state PASS\n"
    data[0:len(ok)]=ok
    tid=DATA_BASE+0x180; mainv=DATA_BASE+0x184; workv=DATA_BASE+0x188; actual=DATA_BASE+0x18c
    c=Code(); failures=[]
    # Main starts with ST0=1.
    c.u8(0xdb,0xe3, 0xd9,0xe8)
    # Create worker; its saved x87 snapshot therefore also starts at ST0=1.
    c.push32(0x4000); c.push8(0); c.push8(0)
    entry_pos=len(c.b)+1; c.push32(0)
    c.push32(tid); c.call_import(311); c.addesp(20)
    # Main turns its own ST0 into 3, then yields.
    c.u8(0xd9,0xe8, 0xd8,0xc1, 0xd8,0xc1)
    c.push8(1); c.call_import(229); c.addesp(4)
    # After worker has run with its own FPU snapshot, main must still see 3.
    c.u8(0xdb,0x1d); c.u32(mainv)
    c.mov_eax_abs(mainv); c.cmp_eax_imm32(3); failures.append(c.jnz32())
    # Wait for worker and verify it observed/kept its independent value 2.
    c.push8(0); c.push32(tid); c.call_import(349); c.addesp(8)
    c.mov_eax_abs(workv); c.cmp_eax_imm32(2); failures.append(c.jnz32())
    write_call(c,DATA_BASE,len(ok),actual); exit_call(c,0)
    worker=len(c.b); struct.pack_into('<I',c.b,entry_pos,CODE_BASE+worker)
    # Worker starts at ST0=1, doubles to 2, yields so main restores its own x87 state.
    c.u8(0xd8,0xc0)
    c.push8(1); c.call_import(229); c.addesp(4)
    c.u8(0xdb,0x1d); c.u32(workv)
    c.u8(0xc3)
    fail=len(c.b)
    for off in failures:c.patch_rel32(off,fail)
    exit_call(c,1)
    return bytes(c.b),bytes(data),c.fix

def system_modules_program():
    """Flat 32-bit imports prove VIO/KBD/SESMGR module routing even with bridges absent."""
    data=bytearray(PAGE)
    ok=b"soft386: system module resolver PASS\n"
    data[0:len(ok)]=ok
    c=Code(); failures=[]
    # Without native DLLs on the regression host, these recognized calls must
    # return ERROR_INVALID_FUNCTION rather than becoming unknown hostcalls.
    c.push8(0); c.push8(0); c.call_import(8,SESMGR); c.addesp(8)
    c.cmp_eax_imm32(1); failures.append(c.jnz32())
    c.push8(0); c.call_import(13,KBDCALLS); c.addesp(4)
    c.cmp_eax_imm32(1); failures.append(c.jnz32())
    # VioWrtTTY(NULL,0,0): still resolves through VIOCALLS and returns bridge-missing 1.
    c.push8(0); c.push8(0); c.push8(0); c.call_import(19,VIOCALLS); c.addesp(12)
    c.cmp_eax_imm32(1); failures.append(c.jnz32())
    write_call(c,DATA_BASE,len(ok),DATA_BASE+0x300); exit_call(c,0)
    fail=len(c.b)
    for off in failures:c.patch_rel32(off,fail)
    exit_call(c,1)
    return bytes(c.b),bytes(data),c.fix

def nls_program():
    """Exercise the DOSCALLS aliases used by the historical nlsinfo build."""
    data=bytearray(PAGE)
    ok=b"soft386: NLS aliases PASS\n"
    data[0:len(ok)]=ok
    cpbuf=DATA_BASE+0x100; actual=DATA_BASE+0x120; ctry=DATA_BASE+0x140
    ci=DATA_BASE+0x150; dbcs=DATA_BASE+0x190; casebuf=DATA_BASE+0x1a0
    data[0x1a0:0x1a3]=b'abz'
    c=Code(); failures=[]
    c.push32(437); c.call_import(289); c.addesp(4); c.test_eax(); failures.append(c.jnz32())
    c.push32(actual); c.push32(cpbuf); c.push8(8); c.call_import(291); c.addesp(12); c.test_eax(); failures.append(c.jnz32())
    c.push32(actual); c.push32(ci); c.push32(ctry); c.push8(44); c.call_import(395); c.addesp(16); c.test_eax(); failures.append(c.jnz32())
    c.push32(dbcs); c.push32(ctry); c.push8(8); c.call_import(396); c.addesp(12); c.test_eax(); failures.append(c.jnz32())
    c.push32(casebuf); c.push32(ctry); c.push8(3); c.call_import(397); c.addesp(12); c.test_eax(); failures.append(c.jnz32())
    write_call(c,DATA_BASE,len(ok),DATA_BASE+0x300); exit_call(c,0)
    fail=len(c.b)
    for off in failures:c.patch_rel32(off,fail)
    exit_call(c,1)
    return bytes(c.b),bytes(data),c.fix

def direct_nls_module_program():
    data=bytearray(PAGE); data[0:3]=b'abz'; actual=DATA_BASE+0x100
    c=Code(); failures=[]
    c.push32(DATA_BASE); c.push8(0); c.push8(3); c.call_import(7,NLSMOD); c.addesp(12)
    c.test_eax(); failures.append(c.jnz32())
    c.push32(actual);c.push8(3);c.push32(DATA_BASE);c.push8(1);c.call_import(282);c.addesp(16)
    exit_call(c,0)
    fail=len(c.b)
    for off in failures:c.patch_rel32(off,fail)
    exit_call(c,1)
    return bytes(c.b),bytes(data),c.fix

def fixrec(fixups):
    b=bytearray()
    for f in fixups:
        if f[0]=='external':
            if len(f)==3: _,source,ordv=f; mod=DOSCALLS
            else: _,source,mod,ordv=f
            # source REL32, external ordinal, 8-bit module field, 16-bit ordinal
            b += bytes([0x08,0x01]) + struct.pack('<hB H',source,mod,ordv)
        elif f[0]=='internal':
            _,source,obj,target=f
            # source OFF32, internal object target, 8-bit object + 16-bit offset
            b += bytes([0x07,0x00]) + struct.pack('<hB H',source,obj,target)
        else:
            raise RuntimeError('unknown fixup '+repr(f))
    return bytes(b)

def make_image(code,data,fixups,lx=False,modules=None):
    if modules is None: modules=['DOSCALLS']
    le=0x80
    obj_rel=0xc4
    objmap_rel=obj_rel+48
    objmap_size=16 if lx else 8
    res_rel=objmap_rel+objmap_size
    entry_rel=res_rel+1
    fixpage_rel=(entry_rel+1+3)&~3
    fixrec_rel=fixpage_rel+12
    fr=fixrec(fixups)
    impmod_rel=fixrec_rel+len(fr)
    modtab=b''.join(bytes([len(m)])+m.encode('ascii') for m in modules)
    impproc_rel=impmod_rel+len(modtab)
    data_abs=0x400
    if le+impproc_rel+1 > data_abs: raise RuntimeError('metadata too large')
    out=bytearray(data_abs+2*PAGE)
    out[0:2]=b'MZ'; struct.pack_into('<I',out,0x3c,le)
    h=le; out[h:h+4]=b'LX\0\0' if lx else b'LE\0\0'
    struct.pack_into('<H',out,h+8,2)      # i386
    struct.pack_into('<H',out,h+0x0a,1)  # OS/2
    struct.pack_into('<I',out,h+0x10,0)
    struct.pack_into('<I',out,h+0x14,2)
    struct.pack_into('<I',out,h+0x18,1)
    struct.pack_into('<I',out,h+0x1c,0)
    struct.pack_into('<I',out,h+0x20,2)
    struct.pack_into('<I',out,h+0x24,0x4000)
    struct.pack_into('<I',out,h+0x28,PAGE)
    struct.pack_into('<I',out,h+0x2c,0 if lx else PAGE)
    struct.pack_into('<I',out,h+0x40,obj_rel)
    struct.pack_into('<I',out,h+0x44,2)
    struct.pack_into('<I',out,h+0x48,objmap_rel)
    struct.pack_into('<I',out,h+0x58,res_rel)
    struct.pack_into('<I',out,h+0x5c,entry_rel)
    struct.pack_into('<I',out,h+0x68,fixpage_rel)
    struct.pack_into('<I',out,h+0x6c,fixrec_rel)
    struct.pack_into('<I',out,h+0x70,impmod_rel)
    struct.pack_into('<I',out,h+0x74,len(modules))
    struct.pack_into('<I',out,h+0x78,impproc_rel)
    struct.pack_into('<I',out,h+0x80,data_abs)
    # object 1 code and object 2 data/stack
    struct.pack_into('<IIIIII',out,h+obj_rel,0x1000,CODE_BASE,0x2005,1,1,0)
    struct.pack_into('<IIIIII',out,h+obj_rel+24,0x4000,DATA_BASE,0x2003,2,1,0)
    m=h+objmap_rel
    if lx:
        struct.pack_into('<IHH',out,m,0,PAGE,0)
        struct.pack_into('<IHH',out,m+8,PAGE,PAGE,0)
    else:
        out[m:m+4]=bytes([0,0,1,0]); out[m+4:m+8]=bytes([0,0,2,0])
    out[h+res_rel]=0; out[h+entry_rel]=0
    struct.pack_into('<III',out,h+fixpage_rel,0,len(fr),len(fr))
    out[h+fixrec_rel:h+fixrec_rel+len(fr)]=fr
    out[h+impmod_rel:h+impmod_rel+len(modtab)]=modtab
    out[h+impproc_rel]=0
    out[data_abs:data_abs+len(code)]=code
    out[data_abs+PAGE:data_abs+2*PAGE]=data
    return bytes(out)

def main():
    d=pathlib.Path(sys.argv[1] if len(sys.argv)>1 else 'fixtures'); d.mkdir(parents=True,exist_ok=True)
    code,data,fx=hello_program()
    (d/'hello-soft386.le').write_bytes(make_image(code,data,fx,False))
    (d/'hello-soft386.lx').write_bytes(make_image(code,data,fx,True))
    code,data,fx=thread_program()
    (d/'thread-soft386.le').write_bytes(make_image(code,data,fx,False))
    code,data,fx=fpu_program()
    (d/'fpu-soft386.le').write_bytes(make_image(code,data,fx,False))
    code,data,fx=fpu_thread_program()
    (d/'fpu-thread-soft386.le').write_bytes(make_image(code,data,fx,False))
    code,data,fx=sync_program()
    (d/'sync-soft386.le').write_bytes(make_image(code,data,fx,False))
    code,data,fx=memory_program()
    (d/'memory-soft386.le').write_bytes(make_image(code,data,fx,False))
    code,data,fx=native_file_program()
    (d/'native-file-soft386.le').write_bytes(make_image(code,data,fx,False))
    code,data,fx=pipe_producer_program()
    (d/'pipe-producer-soft386.exe').write_bytes(make_image(code,data,fx,False))
    code,data,fx=pipe_consumer_program()
    (d/'pipe-consumer-soft386.exe').write_bytes(make_image(code,data,fx,False))
    code,data,fx=hi_surface_program()
    (d/'hi-surface-soft386.le').write_bytes(make_image(code,data,fx,False))
    code,data,fx=internal_fixup_program()
    (d/'internal-soft386.le').write_bytes(make_image(code,data,fx,False))
    (d/'internal-soft386.lx').write_bytes(make_image(code,data,fx,True))
    code,data,fx=system_modules_program()
    (d/'system-modules-soft386.le').write_bytes(make_image(code,data,fx,False,['DOSCALLS','VIOCALLS','KBDCALLS','SESMGR']))
    code,data,fx=nls_program()
    (d/'nls-alias-soft386.le').write_bytes(make_image(code,data,fx,False))
    code,data,fx=direct_nls_module_program()
    (d/'nls-module-soft386.le').write_bytes(make_image(code,data,fx,False,['DOSCALLS','VIOCALLS','KBDCALLS','SESMGR','NLS']))
    for p in sorted(d.glob('*soft386.*')): print(p,p.stat().st_size)
if __name__=='__main__':main()
