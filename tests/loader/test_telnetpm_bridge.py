#!/usr/bin/env python3
"""Exercise production x86 bridge bytes with Unicorn; no Win32/UI/network run.

Usage: python3 test_telnetpm_bridge.py LOADER EMITTER TELNETPM.EXE
Python dependency: unicorn. The historical input is not distributed.
"""
import hashlib
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import *

LOADER, EMITTER, SPECIMEN = map(lambda p: str(Path(p).resolve()), sys.argv[1:4])
MASK = 0xffffffff
TRACE, FS, REJECT, TARGET = 0x3000000, 0x3000010, 0x3000020, 0x4000000
STACK, STOP, PAYLOAD = 0x50f0000, 0x6000000, 0x7000000
REGS = [UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
        UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP]


def run(*args, status=0):
    p = subprocess.run(args, text=True, capture_output=True)
    assert p.returncode == status, (args, p.returncode, p.stdout, p.stderr)
    return p.stdout + p.stderr


def read32(u, addr):
    return struct.unpack('<I', u.mem_read(addr, 4))[0]


def write32(u, addr, value):
    u.mem_write(addr, struct.pack('<I', value & MASK))


class Machine:
    def __init__(self, objects, stubs):
        self.u = u = Uc(UC_ARCH_X86, UC_MODE_32)
        for base, data in objects + [stubs]:
            u.mem_map(base, (len(data) + 4095) & ~4095)
            u.mem_write(base, data)
        for base, size in [(TRACE, 4096), (TARGET, 65536), (0x5000000, 0x100000),
                           (STOP, 4096), (PAYLOAD, 4096)]:
            u.mem_map(base, size)
        self.before = [0x11111101 + i * 0x11111110 for i in range(7)]
        for reg, value in zip(REGS, self.before):
            u.reg_write(reg, value)
        u.reg_write(UC_X86_REG_ESP, STACK)
        u.reg_write(UC_X86_REG_EFLAGS, 0x647)  # DF=1: injected host calls must clear it
        u.reg_write(UC_X86_REG_FPCW, 0x27f)
        u.reg_write(UC_X86_REG_FP0, (0x923456789abcdef0, 0x4001))
        self.fpu = (u.reg_read(UC_X86_REG_FPCW), u.reg_read(UC_X86_REG_FP0))
        self.head = 0xf00d1234
        self.called = []
        self.action = None
        self.send = None
        self.sent = []
        u.hook_add(UC_HOOK_CODE, self.hook)

    def host_return(self):
        u = self.u
        sp = u.reg_read(UC_X86_REG_ESP)
        pc = read32(u, sp)
        # Deliberately clobber caller-saved registers/flags/x87 in host observer.
        for reg in (UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX):
            u.reg_write(reg, 0xbad12345)
        u.reg_write(UC_X86_REG_EFLAGS, 0x202)
        u.reg_write(UC_X86_REG_FPCW, 0x37f)
        u.reg_write(UC_X86_REG_FP0, (0xdeadbeef, 0x4005))
        u.reg_write(UC_X86_REG_ESP, sp + 4)
        u.reg_write(UC_X86_REG_EIP, pc)

    def hook(self, u, addr, size, ctx):
        sp = u.reg_read(UC_X86_REG_ESP)
        if addr == STOP:
            u.emu_stop()
        elif addr == TRACE:
            assert not u.reg_read(UC_X86_REG_EFLAGS) & 0x400
            self.called.append((read32(u, sp + 4), read32(u, sp + 8)))
            self.host_return()
        elif addr == FS:
            assert not u.reg_read(UC_X86_REG_EFLAGS) & 0x400
            kind, saved = read32(u, sp + 4), read32(u, sp + 8)
            self.called.append((kind, saved))
            if kind == 0:
                write32(u, saved + 36, self.head)
            elif kind == 1:
                self.head = saved + 40
            elif kind == 2:
                self.head = read32(u, saved + 40)
            elif kind == 3:
                self.head = read32(u, saved + 28)
            else:
                raise AssertionError(kind)
            self.host_return()
        elif addr == REJECT:
            self.action = 'reject'
            self.rejected = read32(u, sp + 4)
            u.emu_stop()
        elif TARGET <= addr < TARGET + 65536:
            self.action = addr
            u.emu_stop()
        elif addr == self.send:
            self.sent.append(bytes(u.mem_read(u.reg_read(UC_X86_REG_EAX),
                                               u.reg_read(UC_X86_REG_EDX))))
            assert u.reg_read(UC_X86_REG_ECX) == 1
            u.reg_write(UC_X86_REG_EAX, 0)
            u.reg_write(UC_X86_REG_EIP, read32(u, sp))
            u.reg_write(UC_X86_REG_ESP, sp + 4)

    def execute(self, addr):
        self.u.emu_start(addr, STOP, count=100000)

    def preserved(self):
        assert [self.u.reg_read(r) for r in REGS] == self.before
        assert self.u.reg_read(UC_X86_REG_EFLAGS) & 0xcd5 == 0x647 & 0xcd5
        assert (self.u.reg_read(UC_X86_REG_FPCW), self.u.reg_read(UC_X86_REG_FP0)) == self.fpu


def main():
    specimen = Path(SPECIMEN).read_bytes()
    checks = 0
    with tempfile.TemporaryDirectory(prefix='telnetpm-phase2-') as temp:
        td = Path(temp)
        def canonical(env, status=0):
            path = td / 'environment'
            path.write_bytes(env)
            result = subprocess.run([EMITTER, str(path), '--etc-env'], capture_output=True)
            assert result.returncode == status, result.stderr
            return result.stdout

        for before, after in [
            (b'etc=C:\\OS2\\etc\0\0', b'ETC=C:\\OS2\\etc\0\0'),
            (b'EtC=Mixed Path\0\0', b'ETC=Mixed Path\0\0'),
            (b'ETC=\0\0', b'ETC=\0\0'),
            (b'Path=etc\0ETCX=abc\0E=x\0\0', b'Path=etc\0ETCX=abc\0E=x\0\0'),
            (b'=C:=C:\\OS2\0etc=C:\\cfg\0\0', b'=C:=C:\\OS2\0ETC=C:\\cfg\0\0'),
            (b'\0\0', b'\0\0'),
        ]:
            assert canonical(before) == after
            checks += 1
        for malformed in [b'ETC=unterminated', b'ETC=x\0']:
            canonical(malformed, status=3)
            checks += 1
        # SHA-256 known vectors, especially both padding branches.
        for length in [0, 1, 3, 55, 56, 63, 64, 65, 119, 120, 128, 4096]:
            v = bytes(i % 251 for i in range(length))
            (td / 'vector').write_bytes(v)
            assert run(EMITTER, str(td / 'vector'), '--hash').strip() == hashlib.sha256(v).hexdigest()
            checks += 1
        assert 'CHECK PASS' in run(LOADER, '--telnetpm-check', SPECIMEN)
        # Code, relocations, imports, resources, padding: changed files rejected.
        for off in [0x90, 0x600, 0x39e00, 0x3a000, 0x3b000, len(specimen)-1]:
            altered = bytearray(specimen)
            altered[off] ^= 1
            (td / 'changed.exe').write_bytes(altered)
            out = run(LOADER, '--telnetpm-check', str(td / 'changed.exe'), status=1)
            assert 'fingerprint mismatch' in out
            checks += 1
        for mode in ['--run', '--run-quiet']:
            for args in [[], ['-p', '2323', '127.0.0.1']]:
                assert 'TELNETPM native profile requires a 32-bit Win32 build' in run(
                    LOADER, mode, SPECIMEN, *args, status=1)
                checks += 1
            assert 'TELNETPM native profile requires a 32-bit Win32 build' in run(
                LOADER, mode, '--argv0', 'renamed.exe', SPECIMEN, '-p', '23', status=1)
            checks += 1
        assert 'supported via TELNETPM native bridge' in run(LOADER, '--scan', SPECIMEN)
        renamed = td / 'renamed.exe'
        renamed.write_bytes(specimen)
        assert 'TELNETPM native profile requires a 32-bit Win32 build' in run(
            LOADER, '--run', str(renamed), status=1)
        assert 'no recognized native bridge' in run(LOADER, '--run', str(td/'changed.exe'), status=3)
        assert 'requires a 32-bit Win32 build' in run(
            LOADER, '--telnetpm-probe', '--argv0', 'renamed.exe', SPECIMEN, '-p', '23', status=1)
        checks += 4
        assert 'not in the current executable subset' in run(LOADER, '--fixups', SPECIMEN, status=3)
        checks += 2
        snapshot = td / 'snapshot'
        run(EMITTER, SPECIMEN, str(snapshot))
        blob = snapshot.read_bytes()
        cursor = 0
        def word():
            nonlocal cursor
            v = struct.unpack_from('<I', blob, cursor)[0]
            cursor += 4
            return v
        def chunk():
            nonlocal cursor
            base, length = word(), word()
            data = blob[cursor:cursor+length]
            cursor += length
            return base, data
        objects = [chunk() for _ in range(word())]
        stubs = chunk()
        imports = []
        for _ in range(word()):
            entry, ordinal = word(), word()
            name = blob[cursor:cursor+64].split(b'\0')[0].decode()
            cursor += 64
            imports.append((entry, name, ordinal))
        assert cursor == len(blob)
        assert len(imports) == 137
        code = objects[0][1]
        # All import wrappers preserve ESP, args, nonvolatile/volatile GPRs,
        # flags, AL and x87 despite hostile observer clobbering.
        for idx, (entry, _, _) in enumerate(imports):
            m = Machine(objects, stubs)
            m.u.mem_write(STACK, struct.pack('<5I', STOP, 11, 22, 33, 44))
            m.execute(entry)
            assert m.action == TARGET + idx*16
            assert m.called == [(idx, STACK)]
            assert m.u.reg_read(UC_X86_REG_ESP) == STACK
            assert bytes(m.u.mem_read(STACK, 20)) == struct.pack('<5I', STOP, 11, 22, 33, 44)
            m.preserved()
            checks += 1
        # Every patched inline FS operation: continue at the exact next
        # instruction with the original flags/GPRs/x87 and correct stack delta.
        header = (Path(__file__).resolve().parents[2] / 'loader/telnetpm_bridge.h').read_text()
        sites = []
        for kind, name in enumerate(['push', 'esp', 'pop']):
            body = re.search(r'tp_fs_' + name + r'\[\] = \{(.*?)\};', header, re.S)[1]
            sites += [(int(s, 16), kind, 7) for s in re.findall(r'0x[0-9a-f]+', body)]
        sites += [(0x1d5d9, 3, 6)]
        assert len(sites) == 101
        for off, kind, length in sites:
            m = Machine(objects, stubs)
            start, resume = objects[0][0]+off, objects[0][0]+off+length
            write32(m.u, STACK, 0xcafebabe)
            m.u.emu_start(start, resume, count=1000)
            assert m.u.reg_read(UC_X86_REG_EIP) == resume
            assert len(m.called) == 1 and m.called[0][0] == kind
            assert m.u.reg_read(UC_X86_REG_ESP) == STACK + [-4,0,4,0][kind]
            if kind == 0:
                assert read32(m.u, STACK-4) == 0xf00d1234
            else:
                assert m.head == {1: STACK, 2: 0xcafebabe, 3: m.before[0]}[kind]
            m.preserved()
            checks += 1
        base = objects[0][0]
        # Reproduce the exact post-WinGetMsg(FALSE) path. The original guest
        # loops to initialization, so a second queue creation MUST fail.
        # Queue ownership itself is tested against production pm_queue.h.
        class ShutdownMachine(Machine):
            def __init__(self, queue_result):
                self.queue_result = queue_result
                self.apis = []
                super().__init__(objects, stubs)

            def hook(self, u, addr, size, ctx):
                if TARGET <= addr < TARGET + 65536:
                    index = (addr - TARGET)//16
                    api = imports[index][1:]
                    self.apis.append(api)
                    if api == ('PMWIN', 716):
                        result = self.queue_result
                    elif api in [('PMWIN', 763), ('PMWIN', 728), ('PMWIN', 888)]:
                        result = 1
                    else:
                        self.action = api
                        u.emu_stop()
                        return
                    sp = u.reg_read(UC_X86_REG_ESP)
                    u.reg_write(UC_X86_REG_EAX, result)
                    u.reg_write(UC_X86_REG_EIP, read32(u, sp))
                    u.reg_write(UC_X86_REG_ESP, sp+4)
                    return
                super().hook(u, addr, size, ctx)

        def shutdown_check(queue_result):
            m = ShutdownMachine(queue_result)
            u = m.u
            bp = STACK + 1024
            u.reg_write(UC_X86_REG_EBP, bp)
            u.reg_write(UC_X86_REG_ESP, bp-0x24c)
            u.reg_write(UC_X86_REG_EFLAGS, 0x202)
            write32(u, bp, 0)
            write32(u, bp+4, STOP)
            write32(u, bp-4, 100)
            data = objects[4][0]
            for off in [0xf880, 0xb490, 0xf7f4]:
                write32(u, data+off, 0)
            write32(u, data+0xc840, 42)
            m.execute(base+0x47cc)
            if queue_result:
                # Old behavior reaches application reinitialization again.
                assert m.action == ('PMWIN', 781), m.apis
            else:
                assert u.reg_read(UC_X86_REG_EIP) == STOP, (m.action, m.apis)
                assert u.reg_read(UC_X86_REG_EAX) == 0
                assert u.reg_read(UC_X86_REG_ESP) == bp+8
                assert m.apis == [('PMWIN', 763), ('PMWIN', 716), ('PMWIN', 728), ('PMWIN', 888)], m.apis
        for queue_result in [1, 0]:
            shutdown_check(queue_result)
            checks += 1
        # Repeat actual guest shutdown through direct native bindings. Keep
        # one unresolved import to verify that normal mode still traps it.
        traced = objects, stubs, imports
        run(EMITTER, SPECIMEN, str(snapshot), '--direct')
        blob, cursor = snapshot.read_bytes(), 0
        objects = [chunk() for _ in range(word())]
        stubs = chunk()
        imports = []
        for idx in range(word()):
            entry, ordinal = word(), word()
            name = blob[cursor:cursor+64].split(b'\0')[0].decode()
            cursor += 64
            imports.append((entry, name, ordinal))
            assert entry == TARGET+idx*16 if idx else stubs[0] <= entry < stubs[0]+len(stubs[1])
        assert cursor == len(blob) and len(imports) == 137
        for queue_result in [1, 0]:
            shutdown_check(queue_result)
            checks += 1
        m = Machine(objects, stubs)
        write32(m.u, STACK, STOP)
        # Stop at the observer as the real observer calls ExitProcess(4).
        def missing_observer(u, addr, size, ctx):
            if addr == TRACE:
                m.action = 'unresolved import trap'
                u.emu_stop()
        m.u.hook_add(UC_HOOK_CODE, missing_observer)
        m.execute(imports[0][0])
        assert m.called == [(0, STACK)] and m.action == 'unresolved import trap'
        checks += 1
        objects, stubs, imports = traced
        # Reproduce the real CRT environment builder and getenv, without
        # substituting getenv or DOSCALLS. Only malloc supplies a test buffer.
        # This separates a missing/case-mismatched entry from an INI problem.
        for env, expected in [
            (b'ETC=C:\\temp\\telnetpm-etc\0PATH=C:\\OS2\0\0', b'C:\\temp\\telnetpm-etc'),
            (b'Etc=C:\\temp\\telnetpm-etc\0PATH=C:\\OS2\0\0', None),
            (b'PATH=C:\\OS2\0\0', None),
            (b'ETC=\0\0', b''),
            (b'=C:=C:\\OS2\0PATH=C:\\OS2\0ETC=C:\\cfg\0\0', b'C:\\cfg'),
            (canonical(b'etc=C:\\OS2\\etc\0PATH=C:\\OS2\0\0'), b'C:\\OS2\\etc'),
        ]:
            m = Machine(objects, stubs)
            u = m.u
            data = objects[4][0]
            crt = data+0xa094
            write32(u, crt+0x30, MASK)  # unlocked original RTL semaphore
            write32(u, crt+0x1e4, PAYLOAD+0x100)
            write32(u, PAYLOAD+0x110, PAYLOAD+0x200)
            u.mem_write(PAYLOAD+0x200, env)

            def alloc(uc, address, size, _):
                if address == base+0x22340:
                    sp = uc.reg_read(UC_X86_REG_ESP)
                    uc.reg_write(UC_X86_REG_EAX, PAYLOAD+0x800)
                    uc.reg_write(UC_X86_REG_EIP, read32(uc, sp))
                    uc.reg_write(UC_X86_REG_ESP, sp+4)

            u.hook_add(UC_HOOK_CODE, alloc)
            u.reg_write(UC_X86_REG_EFLAGS, 0x202)
            u.reg_write(UC_X86_REG_EAX, crt)
            write32(u, STACK, STOP)
            m.execute(base+0x24900)
            assert u.reg_read(UC_X86_REG_EAX) == 0
            assert read32(u, crt+0x3c) == len(env.split(b'\0'))-2
            u.reg_write(UC_X86_REG_ESP, STACK)
            write32(u, STACK, STOP)
            u.reg_write(UC_X86_REG_EAX, data+0x33c)  # original "ETC" literal
            m.execute(base+0x1fcb4)
            result = u.reg_read(UC_X86_REG_EAX)
            if expected is None:
                assert result == 0
            else:
                assert result != 0
                assert bytes(u.mem_read(result, len(expected)+1)) == expected+b'\0'
            assert read32(u, crt+0x30) == MASK  # released after lookup
            checks += 1
        # Flat conversion helpers leave register-ABI EAX untouched.
        for off in [0x246f8, 0x24700]:
            m = Machine(objects, stubs)
            write32(m.u, STACK, STOP)
            m.execute(base+off)
            assert m.u.reg_read(UC_X86_REG_ESP) == STACK+4
            m.preserved()
            checks += 1
        # Execute original callback body, not a reimplementation. Only its
        # downstream send routine is intercepted; no sockets are involved.
        for first, second in [(b'AB', b'x\xffy'), (b'', b''), (b'abc', b'\xff\xff')]:
            m = Machine(objects, stubs)
            m.u.reg_write(UC_X86_REG_EFLAGS, 0x202)
            m.send = base+0x9cb0
            m.u.mem_write(PAYLOAD, first+b'\0')
            m.u.mem_write(PAYLOAD+128, second+b'\0')
            packed = struct.pack('<IHIH', PAYLOAD, len(first), PAYLOAD+128, len(second))
            frame = struct.pack('<4I', STOP, 1, base+0xb47c, 12) + packed
            m.u.mem_write(STACK, frame)
            m.execute(base+0x1cf14)
            assert m.sent == [b'\xff\xfa\x25'+first+second.replace(b'\xff',b'\xff\xff')+b'\xff\xf0']
            assert m.u.reg_read(UC_X86_REG_EAX) == 0
            assert m.u.reg_read(UC_X86_REG_ESP) == STACK+4
            for idx in [1,4,5,6]:
                assert m.u.reg_read(REGS[idx]) == m.before[idx]
            assert bytes(m.u.mem_read(STACK+4,len(frame)-4)) == frame[4:]
            checks += 1
        for mode, target, count in [(0,base+0xb47c,12),(1,base+0xdead,12),(1,base+0xb47c,14)]:
            m = Machine(objects, stubs)
            m.u.mem_write(STACK, struct.pack('<4I', STOP, mode, target, count))
            m.execute(base+0x1cf14)
            assert m.action == 'reject' and m.rejected == STACK
            checks += 1
        # Cross-object internal REL32 and both callback immediates are rebased.
        dest = (base+0x1d4ee+4+struct.unpack_from('<i',code,0x1d4ee)[0]) & MASK
        assert dest == objects[2][0]+0x12
        for off in [0xbc83,0xbcc3]:
            assert struct.unpack_from('<I',code,off)[0] == base+0xb47c
        assert objects[1][1] == b'\xcc'*35
        checks += 4
        # Original entry runs through its first inline FS push and reaches a
        # real import boundary without any emulated API return values.
        m = Machine(objects, stubs)
        m.u.mem_write(STACK, struct.pack('<5I', STOP, 1, 0, PAYLOAD, PAYLOAD+128))
        m.u.mem_write(PAYLOAD, b'\0\0')
        m.u.mem_write(PAYLOAD+128, b'telnetpm.exe\0\0\0')
        for reg in REGS:
            m.u.reg_write(reg, 0)
        m.u.reg_write(UC_X86_REG_EFLAGS, 0x202)
        m.execute(base+0x1d5b4)
        idx = (m.action-TARGET)//16
        assert imports[idx][1:] == ('DOSCALLS',331)
        assert read32(m.u,m.u.reg_read(UC_X86_REG_ESP)) == base+0x1da67
        assert m.called[0][0] == 0
        checks += 1
    print(f'TELNETPM phase2: {checks} checks PASS (production x86 bytes under Unicorn; no Windows runtime claim)')


if __name__ == '__main__':
    main()
