#!/usr/bin/env python3
"""H4G: executable 16-bit NE files/stdio/child-inheritance/EA regression.

Generated fixtures use genuine NE far relocations to historical DOSCALLS
ordinals; no proprietary C/386 or RC binaries are bundled or emulated here.
"""
import datetime
import os
import pathlib
import struct
import subprocess
import sys
import tempfile

DATA_SEL = 0x108


def w(v):
    return struct.pack('<H', v & 0xffff)


class Code:
    def __init__(self):
        self.code = bytearray(b'\x89\xe3')  # mov bx,sp
        self.fixups = []
        self.fails = []

    def emit(self, value):
        self.code.extend(value)

    def push(self, n):
        self.emit(b'\x68' + w(n))

    def farptr(self, offset):
        self.push(DATA_SEL if offset is not None else 0)
        self.push(offset or 0)

    def memword(self, off):
        self.emit(b'\xff\x36' + w(off))  # push word ptr DS:[offset]

    def check_word(self, off, expected):
        self.emit(b'\xa1' + w(off))  # mov ax, word ptr DS:[offset]
        self.emit(b'\x3d' + w(expected))  # cmp ax, expected
        self.branch_fail()

    def call(self, ordinal, expected=0):
        at = len(self.code)
        self.emit(b'\x9a\xff\xff\xff\xff')
        self.fixups.append(struct.pack('<BBHHH', 3, 1, at+1, 1, ordinal))
        self.emit(b'\x39\xdc')  # cmp SP,BX -- exact Pascal cleanup
        self.branch_fail()
        self.emit(b'\x3d' + w(expected))  # cmp AX,expected APIRET
        self.branch_fail()

    def branch_fail(self):
        at = len(self.code)
        self.emit(b'\x0f\x85\x00\x00')  # JNE near (16-bit)
        self.fails.append(at+2)

    def exit(self, rc):
        self.push(1)  # action
        self.push(rc)  # process result
        at = len(self.code)
        self.emit(b'\x9a\xff\xff\xff\xff')
        self.fixups.append(struct.pack('<BBHHH',3,1,at+1,1,5))

    def finish(self):
        self.exit(0)
        fail = len(self.code)
        self.exit(42)
        for at in self.fails:
            displacement = fail-(at+2)
            assert -32768 <= displacement <= 32767
            struct.pack_into('<h',self.code,at,displacement)
        return self.code,self.fixups


def filewrite(c, handle, off, length, count=0x30):
    c.push(handle)
    c.farptr(off)
    c.push(length)
    c.farptr(count)
    c.call(138)


def open_file(c, off_path, off_handle, off_action, flags=0x01):
    # 16-bit Pascal: arguments are pushed in declaration order.
    c.farptr(off_path)
    c.farptr(off_handle)
    c.farptr(off_action)
    c.push(0);c.push(0)  # initial size ULONG
    c.push(0)  # initial attributes
    c.push(flags)
    c.push(0x42)  # read/write, deny none
    c.push(0);c.push(0)  # reserved ULONG
    c.call(70)


def dup_file(c, old, wanted_off, old_in_mem=False):
    if old_in_mem:c.memword(old)
    else:c.push(old)
    c.farptr(wanted_off)
    c.call(61)


def set_file(c, handle, level, info=0x100, cb=22, mem=False, result=0):
    if mem:c.memword(handle)
    else:c.push(handle)
    c.push(level)
    c.farptr(info)
    c.push(cb)
    c.call(83, result)


def close_file(c, handle, mem=False):
    if mem:c.memword(handle)
    else:c.push(handle)
    c.call(59)


def qhand_type(c, handle, expected_type):
    c.push(handle)
    c.farptr(0x34)  # PUSHORT pfsType
    c.farptr(0x36)  # PUSHORT pusDevAttr
    c.call(77)
    c.check_word(0x34,expected_type)


def delete_file(c, path, reserved=0, expected=0):
    # Real OS/2 1.x signature is DosDelete(PSZ, ULONG reserved).
    # This is EIGHT Pascal bytes, not four; the last pushed ULONG
    # occupies SS:SP+4, while the filename far pointer is SS:SP+8.
    c.farptr(path)
    c.push((reserved >> 16) & 0xffff)
    c.push(reserved & 0xffff)
    c.call(60, expected)


def fixture(path, code, fixups, data):
    ne = 0x80
    out = bytearray(0x2200)
    out[:2] = b'MZ'
    struct.pack_into('<I',out,0x3c,ne)
    out[ne:ne+2] = b'NE'
    for off,val in {0x0e:2, 0x10:0,0x12:0x200,0x14:0,0x16:1,
                    0x18:0x800,0x1a:2,0x1c:2,0x1e:1,0x22:0x40,
                    0x28:0x50,0x2a:0x52,0x32:9}.items():
        struct.pack_into('<H',out,ne+off,val)
    out[ne+0x36]=1
    struct.pack_into('<HHHH',out,ne+0x40,2,len(code),0x0d00,0x900)
    struct.pack_into('<HHHH',out,ne+0x48,8,len(data),0x0c01,0x900)
    struct.pack_into('<H',out,ne+0x50,0)
    out[ne+0x52:ne+0x5b]=b'\x08DOSCALLS'
    # Code lives at 0x400; data starts at 0x1000 (not 0x800).
    assert len(code)<0x900 and len(code)+2+8*len(fixups)<0xc00
    assert len(data)<=0x900
    out[0x400:0x400+len(code)]=code
    pos=0x400+len(code)
    struct.pack_into('<H',out,pos,len(fixups))
    for i,fx in enumerate(fixups):
        out[pos+2+i*8:pos+10+i*8]=fx
    out[0x1000:0x1000+len(data)] = data
    path.write_bytes(out)


def write_string(d,off,s):
    data=os.fsencode(str(s))+b'\0'
    assert off+len(data)<=len(d)
    d[off:off+len(data)]=data


def main():
    loader=pathlib.Path(sys.argv[1]).absolute()
    with tempfile.TemporaryDirectory(prefix='soft386-h4e-') as tmp:
        root=pathlib.Path(tmp)
        capture=root/'redirected.txt'
        reopened=root/'reopened.txt'
        meta=root/'metadata.bin'
        delete=root/'should-delete.bin'
        child=root/'child-ne.exe'
        parent=root/'parent-ne.exe'
        capture.write_bytes(b'')
        meta.write_bytes(b'info')
        delete.write_bytes(b'delete')
        d=bytearray(0x900)
        d[0x60:0x68]=b'CHILD\r\n\x00'
        c=Code();filewrite(c,1,0x60,7)
        fixture(child,*c.finish(),d)
        c=Code()
        d=bytearray(0x900)
        # Metadata buffer: set DOS last-write timestamp and readonly bit.
        t=datetime.datetime(2020,2,3,12,34,56)
        date=((t.year-1980)<<9)|(t.month<<5)|t.day
        clock=(t.hour<<11)|(t.minute<<5)|(t.second//2)
        struct.pack_into('<HH',d,0x100+8,date,clock)
        struct.pack_into('<H',d,0x100+20,1)
        # 16-bit EAOP is 12 bytes: GEA fp, FEA fp, DWORD oError.
        # Test a *real* nonempty FEALIST (no FEA2 alignment/pointers).
        ea_name=b'.TYPE';ea_value=b'1234567890abcdef'
        ea_used=4+4+len(ea_name)+1+len(ea_value)
        ea_size=(ea_used+3)&~3
        assert ea_used==30 and ea_size==32
        struct.pack_into('<I',d,0x600+4,(DATA_SEL<<16)|0x680)
        struct.pack_into('<I',d,0x680,ea_size)
        struct.pack_into('<BBH',d,0x684,0,len(ea_name),len(ea_value))
        d[0x688:0x688+len(ea_name)+1+len(ea_value)]=ea_name+b'\x00'+ea_value
        # Invalid nonzero trailing byte still rejects the FEALIST.
        d[0x840:0x840+ea_size]=d[0x680:0x680+ea_size]
        d[0x840+ea_size-1]=0x5a
        struct.pack_into('<I',d,0x720+4,(DATA_SEL<<16)|0x840)
        struct.pack_into('<I',d,0x700+4,(DATA_SEL<<16)|0x740)
        struct.pack_into('<I',d,0x740,5)  # truncated FEA: inconsistent
        struct.pack_into('<H',d,0x48,0xffff)  # save the original stdout
        struct.pack_into('<H',d,0x4a,1)  # redirect guest stdout
        struct.pack_into('<H',d,0x4c,1)  # restore guest stdout
        write_string(d,0x180,meta)
        write_string(d,0x280,capture)
        write_string(d,0x380,delete)
        write_string(d,0x480,child)
        write_string(d,0x580,str(child)+'\0')  # argv0 + empty tail
        write_string(d,0x780,reopened)
        d[0x80:0x80+8]=b'PARENT\r\n'
        # Metadata: open, perform good/bad level/buffer/handle operations.
        open_file(c,0x180,0x40,0x42)
        set_file(c,0x40,2,info=0x600,cb=12,mem=True)
        set_file(c,0x40,2,info=0x720,cb=12,mem=True,result=255)
        set_file(c,0x40,2,info=0x700,cb=12,mem=True,result=255)
        set_file(c,0x40,2,info=0x600,cb=8,mem=True,result=122)
        set_file(c,0x40,1,mem=True)
        set_file(c,0x40,3,mem=True,result=124)
        set_file(c,0x40,1,cb=10,mem=True,result=122)
        set_file(c,0xffff,1,result=6)
        close_file(c,0x40,mem=True)
        # Redirect stdout. Parent and child both write into the same file.
        open_file(c,0x280,0x40,0x42)
        dup_file(c,1,0x48)  # save stdout => new guest HFILE
        dup_file(c,0x40,0x4a,old_in_mem=True)  # redirect fd 1
        filewrite(c,1,0x80,8)
        # DosExecPgm: seven Pascal arguments (last is program name).
        c.farptr(None)  # failure object buffer
        c.push(0)  # cbObj
        c.push(0)  # EXEC_SYNC
        c.farptr(0x580)  # args
        c.farptr(None)  # environment
        c.farptr(0x50)  # result codes
        c.farptr(0x480)  # program path
        c.call(144)
        # Rebind stdout before cleaning up the child's file handle.
        dup_file(c,0x48,0x4c,old_in_mem=True)
        # Real Microsoft RC closes stdout, opens its temporary stdout file,
        # then starts RCPP.  H4F allocated a new guest HFILE >=3 rather than
        # reusing the free standard slot 1.  Assert both handle identity and
        # inherited child output, not just DosDupHandle redirection.
        close_file(c,1)
        open_file(c,0x780,0x44,0x46,flags=0x12)
        c.check_word(0x44,1)
        qhand_type(c,1,0)  # reopened standard HFILE 1 is now a disk FILE
        filewrite(c,1,0x80,8)
        c.farptr(None);c.push(0);c.push(0)
        c.farptr(0x580);c.farptr(None)
        c.farptr(0x50);c.farptr(0x480)
        c.call(144)
        dup_file(c,0x48,0x4c,old_in_mem=True)
        close_file(c,0x48,mem=True)
        close_file(c,0x40,mem=True)
        delete_file(c,0x380,reserved=1,expected=87)
        delete_file(c,0x380)
        delete_file(c,0x380,expected=2)
        fixture(parent,*c.finish(),d)
        p=subprocess.run([str(loader),'--trace-hc','--run',str(parent)],
                         cwd=str(root),stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE,timeout=40)
        log=p.stderr.decode('latin1',errors='replace')
        if (p.returncode or p.stdout or
            capture.read_bytes()!=b'PARENT\r\nCHILD\r\n' or
            reopened.read_bytes()!=b'PARENT\r\nCHILD\r\n'):
            raise AssertionError((p.returncode,p.stdout,capture.read_bytes(),
                                  reopened.read_bytes(),log))
        observed_time=int(meta.stat().st_mtime)
        observed_readonly=(meta.stat().st_mode & 0o200)==0
        if sys.platform=='linux':
            assert os.getxattr(meta,'user.os2..TYPE')==ea_value,log
        os.chmod(meta,0o666)  # also make TemporaryDirectory cleanup safe on Windows
        assert not delete.exists(),log
        assert observed_time==int(t.timestamp()),(observed_time,log)
        assert observed_readonly,log
        for needle in ('DOS16.83 SetFileInfo','level=2','list=',
                       'rc=255','rc=124',
                       'rc=122','rc=6','DOS16 DupHandle old=1',
                       'wanted=1','DOS16 Delete','reserved=00000001',
                       'open file='+str(reopened)+' h=1',
                       'QHandType h=1',
                       'NE EXEC inherit std1'):
            # The Windows-only inherit log is replaced by POSIX fork inheritance.
            if needle=='NE EXEC inherit std1' and os.name!='nt':continue
            assert needle in log,(needle,log)
    print('NE-H4G RC FILES PASS: close/reopen stdout inheritance, EA trailing padding, DosDelete ABI and cleanup regressions')


if __name__=='__main__':main()
