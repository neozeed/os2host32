#!/usr/bin/env python3
"""Execute the *built i386 DLL code* with Unicorn and a narrow Win32 shim.

Transport uses real local TCP/UDP sockets; resolver replies and Win32 APIs are
controlled fixtures. This is an ABI/adapter test, NOT a Windows execution test.
The included socket-smoke.exe exercises actual Winsock separately on Windows.
Needs Python 3 + unicorn; no pefile, Wine, DNS or external server needed.
"""
import errno
import select
import socket
import struct
import sys
import time
from pathlib import Path

from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP,
    UC_X86_REG_EFLAGS, UC_X86_REG_EBX, UC_X86_REG_EBP, UC_X86_REG_ESI, UC_X86_REG_EDI)


def u16(b, p=0): return struct.unpack_from('<H', b, p)[0]
def u32(b, p=0): return struct.unpack_from('<I', b, p)[0]
def words(*v): return struct.pack('<' + 'I' * len(v), *(n & 0xffffffff for n in v))
def signed(v): return v if v < 0x80000000 else v - 0x100000000


class PE:
    def __init__(self, path, uc):
        b = Path(path).read_bytes(); pe = u32(b, 0x3c)
        assert b[pe:pe+4] == b'PE\0\0' and u16(b, pe+4) == 0x14c
        opt = pe + 24; assert u16(b, opt) == 0x10b
        self.base = u32(b, opt+28); self.image = bytearray(u32(b, opt+56))
        self.image[:u32(b,opt+60)] = b[:u32(b,opt+60)]
        secs = []; sp = opt + u16(b,pe+20)
        for i in range(u16(b, pe+6)):
            q = sp + 40*i; rva, size, raw = struct.unpack_from('<III', b, q+12)
            self.image[rva:rva+size] = b[raw:raw+size]; secs.append(rva)
        uc.mem_map(self.base, (len(self.image)+4095)&~4095)
        uc.mem_write(self.base, bytes(self.image))
        def z(rva): return bytes(self.image[rva:]).split(b'\0',1)[0].decode()
        ex = u32(b, opt+96); ib = self.image
        first, nf, nn, funcs, names, ords = struct.unpack_from('<6I',ib,ex+16)
        self.exports = {first+i: self.base+u32(ib,funcs+4*i)
                        for i in range(nf) if u32(ib,funcs+4*i)}
        self.exports.update({z(u32(ib,names+4*i)): self.exports[first+u16(ib,ords+2*i)]
                             for i in range(nn)})
        self.imports = []
        ip = u32(b, opt+104)
        while u32(ib,ip+12):
            oft, _, _, dll, iat = struct.unpack_from('<5I',ib,ip)
            n = 0
            while u32(ib,(oft or iat)+4*n):
                v = u32(ib,(oft or iat)+4*n)
                name = (v & 0xffff) if v & 0x80000000 else z(v+2)
                self.imports.append((z(dll).lower(),name,self.base+iat+4*n)); n += 1
            ip += 20
        # Call the sources' DllMain, not MinGW CRT startup. No C++ constructors,
        # CRT TLS, or runtime relocations are used by these two implementations.
        sym, count = struct.unpack_from('<II',b,pe+12); strings = sym+count*18; i = 0
        self.main = None
        while i < count:
            q = sym+i*18; raw = b[q:q+8]
            name = (b[strings+u32(raw,4):].split(b'\0',1)[0]
                    if raw[:4] == b'\0'*4 else raw.split(b'\0',1)[0])
            if name == b'_DllMain@12':
                value, section = struct.unpack_from('<Ih',b,q+8)
                self.main = self.base+secs[section-1]+value
            i += 1+b[q+17]
        assert self.main is not None, 'Build unstripped DLLs for this test'


class Runtime:
    STOP = 0x31000000
    SP = 0x301ff000
    def __init__(self, so, tcp):
        self.u = Uc(UC_ARCH_X86,UC_MODE_32)
        self.u.mem_map(0x20000000, 0x04000000); self.heap = 0x20001000
        self.u.mem_map(0x30000000, 0x200000)
        self.u.mem_map(self.STOP,0x1000); self.u.mem_map(0x32000000,0x10000)
        self.modules = {'so32dll.dll': PE(so,self.u), 'tcp32dll.dll': PE(tcp,self.u)}
        self.stubs = {}; self.tls = {}; self.thread = 1; self.next_tls = 0
        self.socks = {}; self.next_sock = 0x60001000; self.last_error = 0
        self.allocations = set(); self.startups = 0; self.start_error = 0
        self.host_error = 0; self.heap_fail = False; self.io_error = 0
        self.cooperative = False; self.yielded = None; self.close_error = 0
        for module in self.modules.values():
            for dll, name, iat in module.imports:
                if dll in self.modules: addr = self.modules[dll].exports[name]
                else:
                    addr = 0x32000000 + len(self.stubs)*16
                    self.stubs[addr] = (dll,name)
                self.u.mem_write(iat,words(addr))
        self.u.hook_add(UC_HOOK_CODE,self.hook)
        for module in self.modules.values():
            assert self.call(module.main,module.base,1,0,cleanup=12) == 1

    def alloc(self, data=b'', size=None):
        p = self.heap; self.heap += ((size or len(data) or 1)+15)&~15
        assert self.heap < 0x24000000
        if data: self.u.mem_write(p,data)
        return p

    def rd(self,p,n): return bytes(self.u.mem_read(p,n))
    def wr(self,p,b): self.u.mem_write(p,b)
    def word(self,p): return u32(self.rd(p,4))
    def text(self,p):
        if not p: return None
        out = bytearray()
        while self.rd(p,1) != b'\0': out += self.rd(p,1); p += 1
        return bytes(out)

    def call(self,addr,*args,cleanup=0):
        self.wr(self.SP,words(self.STOP,*args))
        self.u.reg_write(UC_X86_REG_ESP,self.SP); self.u.reg_write(UC_X86_REG_EFLAGS,0x202)
        preserved = (UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_ESI,UC_X86_REG_EDI)
        for i, reg in enumerate(preserved): self.u.reg_write(reg,0xabc00100+i)
        self.u.emu_start(addr,self.STOP,count=4000000)
        assert self.u.reg_read(UC_X86_REG_EIP) == self.STOP, 'instruction budget exhausted'
        assert self.u.reg_read(UC_X86_REG_ESP) == self.SP+4+cleanup, 'stack cleanup ABI'
        for i, reg in enumerate(preserved):
            assert self.u.reg_read(reg) == 0xabc00100+i, 'callee-saved register ABI'
        return self.u.reg_read(UC_X86_REG_EAX)

    def so(self,ordinal,*args): return signed(self.call(self.modules['so32dll.dll'].exports[ordinal],*args))
    def tcp(self,ordinal,*args): return self.call(self.modules['tcp32dll.dll'].exports[ordinal],*args)
    def native_socket(self,s):
        h = self.next_sock; self.next_sock += 0x100
        s.settimeout(2); self.socks[h] = s; return h

    def hook(self,uc,addr,size,userdata):
        if addr not in self.stubs: return
        dll, name = self.stubs[addr]; sp = uc.reg_read(UC_X86_REG_ESP)
        a = [self.word(sp+4+4*i) for i in range(8)]
        try: result, argc = self.api(name,a)
        except OSError as e:
            if e.errno == errno.EPERM:
                raise RuntimeError('This test needs permission to create localhost sockets') from e
            self.last_error = {errno.EAGAIN:10035,errno.EINPROGRESS:10036,
                errno.ECONNREFUSED:10061,errno.ENOTCONN:10057,errno.EPIPE:10058,
                errno.ECONNRESET:10054,errno.EADDRINUSE:10048}.get(e.errno,10022)
            argc = {'connect':3,'recv':4,'send':4,'recvfrom':6,'sendto':6,
                    'shutdown':2,'bind':3,'accept':3,'socket':3}[name]; result = -1
        uc.reg_write(UC_X86_REG_EAX,result & 0xffffffff)
        uc.reg_write(UC_X86_REG_EIP,self.word(sp))
        uc.reg_write(UC_X86_REG_ESP,sp+4+(0 if dll == 'msvcrt.dll' else argc*4))
        if self.cooperative and name in ('select','Sleep'):
            self.yielded = name; uc.emu_stop()

    def close_waiter(self,ordinal,args,descriptor):
        """Interleave two actual DLL call stacks at wait boundaries.

        The worker must reach its first native select before SOCLOSE starts.
        Unlike a fake shutdown wakeup, real nonblocking POSIX I/O supplies
        WSAEWOULDBLOCK; the compiled close/readiness/refcount code must drain.
        """
        contexts = []; results = {}; preserved = (UC_X86_REG_EBX,
            UC_X86_REG_EBP,UC_X86_REG_ESI,UC_X86_REG_EDI)
        for i,(entry,values) in enumerate(((ordinal,args),(17,(descriptor,)))):
            sp = self.SP-i*0x10000
            self.wr(sp,words(self.STOP,*values))
            self.u.reg_write(UC_X86_REG_ESP,sp)
            self.u.reg_write(UC_X86_REG_EIP,self.modules['so32dll.dll'].exports[entry])
            self.u.reg_write(UC_X86_REG_EFLAGS,0x202)
            for j,reg in enumerate(preserved): self.u.reg_write(reg,0xabc00100+j)
            contexts.append(self.u.context_save())
        self.cooperative = True
        try:
            for turn in range(30):
                i = turn%2
                if i in results: continue
                self.thread = i+1; self.u.context_restore(contexts[i]); self.yielded = None
                self.u.emu_start(self.u.reg_read(UC_X86_REG_EIP),self.STOP,count=4000000)
                if turn == 0: assert self.yielded == 'select', 'worker must really wait before close'
                if self.u.reg_read(UC_X86_REG_EIP) == self.STOP:
                    assert self.u.reg_read(UC_X86_REG_ESP) == self.SP-i*0x10000+4
                    for j,reg in enumerate(preserved): assert self.u.reg_read(reg) == 0xabc00100+j
                    results[i] = signed(self.u.reg_read(UC_X86_REG_EAX))
                else: assert self.yielded, 'instruction budget exhausted in concurrent call'
                contexts[i] = self.u.context_save()
                if len(results) == 2: break
            assert results == {0:-1,1:0}, ('close must cancel and drain the pending call',results)
        finally:
            self.cooperative = False; self.thread = 1
        assert self.so(20) == 10004, 'cancelled worker must receive SOCEINTR'

    def address(self,p):
        b = self.rd(p,16); assert u16(b) == 2
        return socket.inet_ntoa(b[4:8]),struct.unpack('!H',b[2:4])[0]
    def write_address(self,p,n,address):
        if p:
            self.wr(p,struct.pack('<H',2)+struct.pack('!H',address[1])+socket.inet_aton(address[0])+b'\0'*8)
            self.wr(n,words(16))
    def fdset(self,p): return [self.word(p+4+4*i) for i in range(self.word(p))] if p else []

    def api(self,n,a):
        if n in ('InitializeCriticalSection','DeleteCriticalSection','EnterCriticalSection','LeaveCriticalSection'):
            return 0,1
        if n == 'TlsAlloc': self.next_tls += 1; return self.next_tls,0
        if n == 'TlsFree': return 1,1
        if n == 'TlsGetValue': return self.tls.get((self.thread,a[0]),0),1
        if n == 'TlsSetValue': self.tls[self.thread,a[0]] = a[1]; return 1,2
        if n == 'GetProcessHeap': return 1,0
        if n == 'HeapAlloc':
            if self.heap_fail: return 0,3
            p = self.alloc(size=a[2]); self.allocations.add(p); return p,3
        if n == 'HeapFree':
            assert a[2] in self.allocations, 'free must use allocation owner, not guest-mutated result'
            self.allocations.remove(a[2]); return 1,3
        if n == 'memcpy': self.wr(a[0],self.rd(a[1],a[2])); return a[0],3
        if n == 'strlen': return len(self.text(a[0])),1
        if n == 'sprintf':
            assert self.text(a[1]) == b'%u.%u.%u.%u'
            text = ('.'.join(str(v) for v in a[2:6])).encode()
            self.wr(a[0],text+b'\0'); return len(text),6
        if n == 'WSAStartup':
            self.startups += 1
            if self.start_error: return self.start_error,2
            self.wr(a[1],struct.pack('<H',0x202)); return 0,2
        if n == 'WSACleanup': return 0,0
        if n == 'WSAGetLastError': return self.last_error,0
        if n == 'GetTickCount': return int(time.monotonic()*1000)&0xffffffff,0
        if n == 'Sleep': assert a[0] < 50; return 0,1
        if n == 'socket': return self.native_socket(socket.socket(a[0],a[1],a[2])),3
        if n == 'closesocket':
            if self.close_error: self.last_error = self.close_error; return -1,1
            self.socks.pop(a[0]).close(); return 0,1
        if n == 'bind': self.socks[a[0]].bind(self.address(a[1])); return 0,3
        if n == 'listen': self.socks[a[0]].listen(a[1]); return 0,2
        if n == 'connect': self.socks[a[0]].connect(self.address(a[1])); return 0,3
        if n == 'accept':
            s, address = self.socks[a[0]].accept(); self.write_address(a[1],a[2],address)
            return self.native_socket(s),3
        if n in ('getsockname','getpeername'):
            address = getattr(self.socks[a[0]],n)(); self.write_address(a[1],a[2],address); return 0,3
        if n == 'send': return self.socks[a[0]].send(self.rd(a[1],a[2]),a[3]),4
        if n == 'recv':
            if self.io_error: self.last_error = self.io_error; return -1,4
            b = self.socks[a[0]].recv(a[2],a[3]); self.wr(a[1],b); return len(b),4
        if n == 'sendto': return self.socks[a[0]].sendto(self.rd(a[1],a[2]),a[3],self.address(a[4])),6
        if n == 'recvfrom':
            b, address = self.socks[a[0]].recvfrom(a[2],a[3]); self.wr(a[1],b)
            self.write_address(a[4],a[5],address); return len(b),6
        if n == 'shutdown': self.socks[a[0]].shutdown(a[1]); return 0,2
        if n == 'ioctlsocket':
            if a[1] == 0x8004667e: self.socks[a[0]].setblocking(not self.word(a[2]))
            elif a[1] == 0x4004667f:
                b = self.socks[a[0]].recv(65536,socket.MSG_PEEK|socket.MSG_DONTWAIT)
                self.wr(a[2],words(len(b)))
            else: assert a[1] == 0x40047307; self.wr(a[2],words(1))
            return 0,3
        if n == '__WSAFDIsSet': return int(a[0] in self.fdset(a[1])),2
        if n == 'select':
            groups = [self.fdset(p) for p in a[1:4]]
            assert a[4], 'native waits must be bounded, even for infinite guest select'
            tv = self.word(a[4])+self.word(a[4]+4)/1000000
            assert tv <= 0.05, 'native waits must be interruptible within 50 ms'
            ready = select.select(*[[self.socks[h] for h in g] for g in groups],tv)
            total = 0
            for p,g,r in zip(a[1:4],groups,ready):
                out = [h for h in g if self.socks[h] in r]; total += len(out)
                if p: self.wr(p,words(len(out),*out))
            return total,5
        if n in ('setsockopt','getsockopt'):
            level = socket.SOL_SOCKET if a[1] == 0xffff else a[1]
            option = {4:socket.SO_REUSEADDR,8:socket.SO_KEEPALIVE,16:socket.SO_DONTROUTE,
                32:socket.SO_BROADCAST,128:socket.SO_LINGER,256:socket.SO_OOBINLINE,
                0x1001:socket.SO_SNDBUF,0x1002:socket.SO_RCVBUF,
                0x1007:socket.SO_ERROR,0x1008:socket.SO_TYPE}.get(a[2],a[2]) if a[1] == 0xffff else a[2]
            s = self.socks[a[0]]
            if n == 'setsockopt':
                value = struct.pack('ii',*struct.unpack('<HH',self.rd(a[3],4))) if a[2] == 128 else signed(self.word(a[3]))
                s.setsockopt(level,option,value)
            else:
                value = struct.pack('<HH',*struct.unpack('ii',s.getsockopt(level,option,8))) if a[2] == 128 else words(s.getsockopt(level,option))
                if option == socket.SO_ERROR and level == socket.SOL_SOCKET:
                    value = words({0:0,errno.ECONNREFUSED:10061}.get(u32(value),10022))
                self.wr(a[3],value); self.wr(a[4],words(len(value)))
            return 0,5
        if n == 'inet_addr':
            try: return u32(socket.inet_aton(self.text(a[0]).decode())),1
            except OSError: return 0xffffffff,1
        if n in ('gethostbyname','gethostbyaddr'):
            argc = 1 if n == 'gethostbyname' else 3
            if self.host_error: self.last_error = self.host_error; return 0,argc
            name = self.alloc(b'fixture.test\0'); alias = self.alloc(b'alias.test\0')
            aliases = self.alloc(words(alias,0)); ip = self.alloc(socket.inet_aton('127.0.0.1'))
            ips = self.alloc(words(ip,0))
            # Actual Win32 hostent ABI: shorts at 8/10, address list at 12.
            self.native_host = self.alloc(words(name,aliases)+struct.pack('<hh',2,4)+words(ips))
            return self.native_host,argc
        if n in ('getservbyname','getservbyport'):
            name = self.alloc(b'telnet\0'); proto = self.alloc(b'tcp\0'); aliases = self.alloc(words(0))
            # Poison the short's padding. A shallow memcpy would contaminate
            # the guest's 32-bit port (TELNETPM reads all 32 bits at offset 8).
            return self.alloc(words(name,aliases)+struct.pack('<HH',0x1700,0xbeef)+words(proto)),2
        if n == 'gethostname': self.wr(a[0],b'fixture-host\0'); return 0,2
        raise AssertionError(f'unimplemented test shim API: {n}')


def run(so,tcp):
    r = Runtime(so,tcp); checks = 0
    def check(ok,label):
        nonlocal checks
        checks += 1; assert ok,label
    def err(code): check(r.so(20) == code,f'errno {code}')
    def data(b): return r.alloc(b)
    def addr(): return data(b'\x02\x00\x00\x00\x7f\x00\x00\x01'+b'\0'*8)
    try:
        check(len(r.modules['so32dll.dll'].exports) == 40,'exact 20 socket exports')
        check(len(r.modules['tcp32dll.dll'].exports) == 20,'exact 10 TCP exports')
        r.start_error = 10093; check(r.so(26) == -1,'startup failure'); err(10100)
        r.start_error = 0; check(r.so(26) == 0,'startup retry')
        check(r.so(26) == 0 and r.startups == 2,'idempotent initialization'); err(10100)
        check(r.tcp(4,0xabcd1234)&0xffff == 0x3412,'BSWAP only low 16 bits')
        check(r.tcp(3,0x12345678) == 0x78563412,'LSWAP all 32 bits')
        check(r.tcp(5,data(b'127.0.0.1\0')) == 0x0100007f,'inet_addr byte order')
        check(r.tcp(5,data(b'invalid\0')) == 0xffffffff,'invalid IPv4 sentinel')
        check(r.text(r.tcp(10,0x0100007f)) == b'127.0.0.1','inet_ntoa')
        hp = r.tcp(11,data(b'fixture.test\0'))
        check(r.word(hp+8) == 2 and r.word(hp+12) == 4,'OS/2 hostent ints')
        check(r.rd(r.word(r.word(hp+16)),4) == b'\x7f\0\0\x01','hostent pointer at +16')
        check(r.word(hp+16) != r.word(r.native_host+12),'deep copied address list')
        r.wr(r.word(r.native_host),b'changed\0')
        check(r.text(r.word(hp)) == b'fixture.test','host result owns strings')
        sp = r.tcp(24,data(b'telnet\0'),data(b'tcp\0'))
        check(r.word(sp+8) == 0x1700 and r.text(r.word(sp+12)) == b'tcp','servent port padding')
        check(r.text(r.word(hp)) == b'fixture.test','service does not invalidate host result')
        r.wr(hp+16,words(r.word(hp+16)+4)) # TELNETPM mutates this during retries.
        check(r.tcp(12,data(b'\x7f\0\0\x01'),4,2) != 0,'host result can be replaced after guest mutation')
        for native, guest in [(11001,1),(11002,2),(11003,3),(11004,4)]:
            r.host_error = native
            check(r.tcp(11,data(b'fixture.test\0')) == 0 and r.tcp(51) == guest,'resolver error mapping')
        r.host_error = 0; r.heap_fail = True
        check(r.tcp(11,data(b'fixture.test\0')) == 0,'resolver allocation failure'); err(10055)
        r.heap_fail = False
        r.so(35,10061); r.thread = 2
        check(r.so(20) == 0 and r.tcp(51) == 0,'fresh thread error state')
        check(r.tcp(11,data(b'fixture.test\0')) != 0,'second thread resolver')
        r.so(35,10038); r.thread = 1; err(10061)

        server = r.so(16,2,1,0); client = r.so(16,2,1,0); target = addr(); size = data(words(16))
        check(0 <= server < 256 and 0 <= client < 256,'small guest descriptors for large host handles')
        check(r.so(2,server,target,16) == 0,'bind loopback ephemeral port')
        check(r.so(6,server,target,size) == 0 and r.word(size) == 16,'getsockname old ABI')
        check(r.so(9,server,1) == 0,'listen'); check(r.so(3,client,target,16) == 0,'connect')
        peer = r.so(1,server,0,0); check(peer >= 0,'accept')
        check(r.so(5,client,addr(),size) == 0,'getpeername')
        out = data(b'x'*16); msg = data(b'\x00telnet\xff')
        check(r.so(13,client,msg,8,0) == 8,'binary send')
        available = data(words(0)); check(r.so(8,peer,0x4004667f,available,4) == 0 and r.word(available) == 8,'FIONREAD')
        group = data(words(peer,client,client))
        check(r.so(12,group,2,1,0,1000) == 2,'array select read and write')
        check(r.rd(group,12) == words(peer,-1,client),'select preserves positions')
        check(r.so(10,peer,out,8,2) == 8 and r.rd(out,8) == b'\x00telnet\xff','MSG_PEEK')
        check(r.so(10,peer,out,8,0) == 8,'recv after peek')
        group = data(words(peer)); check(r.so(12,group,1,0,0,0) == 0 and r.word(group) == peer,'timeout leaves array intact')
        check(r.so(12,0,0,0,0,0) == 0,'empty select zero timeout')
        bad = data(words(peer,9999)); check(r.so(12,bad,2,0,0,0) == -1,'bad select descriptor'); err(10038)
        check(r.rd(bad,8) == words(peer,9999),'select error leaves array intact')
        on = data(words(1)); check(r.so(8,peer,0x8004667e,on,4) == 0,'FIONBIO four arguments')
        check(r.so(10,peer,out,1,0) == -1,'nonblocking receive'); err(10035)
        check(r.so(6,peer,addr(),size) == 0,'success after error'); err(10035)
        for native,guest in [(10054,10054),(995,10004),(8,10055),(12345,10100)]:
            r.io_error = native; check(r.so(10,peer,out,1,0) == -1,'injected Winsock failure'); err(guest)
        r.io_error = 0
        linger = data(words(1,3)); check(r.so(15,peer,0xffff,128,linger,8) == 0,'32-bit linger to WinSock shorts')
        r.wr(size,words(8)); check(r.so(7,peer,0xffff,128,out,size) == 0 and r.rd(out,8) == words(1,3),'linger roundtrip')
        check(r.so(15,peer,6,1,on,4) == 0,'TCP_NODELAY')
        check(r.so(15,peer,0xffff,0x1005,on,4) == -1,'unsupported timeout option rejected'); err(10042)
        check(r.so(13,peer,msg,1,0x80) == -1,'unsupported message flags rejected'); err(10045)
        check(r.so(8,peer,0xdeadbeef,on,4) == -1,'unknown ioctl rejected'); err(10045)
        check(r.so(3,client,target,2) == -1,'short address rejected'); err(10022)
        check(r.so(25,client,1) == 0,'shutdown send')
        check(r.so(10,peer,out,1,0) == 0,'orderly EOF')
        r.close_error = 10035
        check(r.so(17,peer) == -1,'failed native close retains guest descriptor'); err(10035)
        r.wr(size,words(16))
        check(r.so(6,peer,addr(),size) == 0,'descriptor remains usable after failed close')
        r.close_error = 0
        check(r.so(17,client) == 0 and r.so(17,peer) == 0 and r.so(17,server) == 0,'close sockets')
        check(r.so(17,client) == -1,'double close rejected'); err(10038)
        check(r.so(16,23,1,0) == -1,'IPv6/other families rejected'); err(10047)
        check(r.so(16,2,3,0) == -1,'raw sockets rejected'); err(10044)

        udp = r.so(16,2,2,0); sender = r.so(16,2,2,0); target = addr(); r.wr(size,words(16))
        check(r.so(2,udp,target,16) == 0 and r.so(6,udp,target,size) == 0,'UDP bind/query')
        check(r.so(14,sender,msg,8,0,target,16) == 8,'UDP sendto')
        check(r.so(11,udp,out,8,0,addr(),size) == 8 and r.rd(out,8) == b'\x00telnet\xff','UDP recvfrom')
        check(r.so(17,udp) == 0 and r.so(17,sender) == 0,'UDP close')
        bound = r.so(16,2,1,0); refused = r.so(16,2,1,0); target = addr()
        r.so(2,bound,target,16); r.wr(size,words(16)); r.so(6,bound,target,size)
        check(r.so(3,refused,target,16) == -1,'real connection refusal'); err(10061)
        r.so(17,bound); r.so(17,refused)
        # Closing an idle accepted connection used to wait ~30 seconds and
        # fail with 10036 on Windows. Exercise real compiled concurrent paths.
        server = r.so(16,2,1,0); client = r.so(16,2,1,0); target = addr()
        r.so(2,server,target,16); r.wr(size,words(16)); r.so(6,server,target,size)
        r.so(9,server,1); r.so(3,client,target,16); peer = r.so(1,server,0,0)
        r.close_waiter(10,(peer,out,1,0),peer)
        check(r.so(17,client) == 0,'close cancels a blocked TCP receive')
        r.close_waiter(1,(server,0,0),server)
        check(r.so(17,server) == -1,'close cancels a blocked accept and frees descriptor')
        for ordinal in (11,12):
            udp = r.so(16,2,2,0); r.so(2,udp,addr(),16); group = data(words(udp))
            args = (udp,out,1,0,0,0) if ordinal == 11 else (group,1,0,0,-1)
            r.close_waiter(ordinal,args,udp)
            check(r.word(group) == udp,'cancelled recvfrom/select preserves caller data')
        # A finite wait must keep its original descriptor sets across slices.
        udp = r.so(16,2,2,0); r.so(2,udp,addr(),16); group = data(words(udp)); began = time.monotonic()
        check(r.so(12,group,1,0,0,120) == 0 and r.word(group) == udp,'multi-slice select timeout')
        check(0.10 <= time.monotonic()-began < 1,'select uses a total elapsed-time deadline')
        r.so(17,udp)
        opened = [r.so(16,2,2,0) for _ in range(256)]
        check(opened == list(range(256)),'descriptor allocation/reuse')
        check(r.so(16,2,2,0) == -1,'descriptor table full'); err(10024)
        check(len(r.socks) == 256,'failed adoption closes native socket')
        for s in opened: check(r.so(17,s) == 0,'release descriptor')
        for thread in (2,1):
            r.thread = thread; m = r.modules['tcp32dll.dll']; r.call(m.main,m.base,3,0,cleanup=12)
        check(not r.allocations,'resolver TLS result allocations freed')
        print(f'PASS: {checks} behavioral checks; every compiled export call also checks i386 stack/register preservation')
        print('Transport: real localhost TCP/UDP. Win32/resolver APIs: controlled shim. Native Windows run remains required.')
    finally:
        for s in r.socks.values(): s.close()


if __name__ == '__main__':
    run(*(sys.argv[1:] or ['SO32DLL.dll','TCP32DLL.dll']))
