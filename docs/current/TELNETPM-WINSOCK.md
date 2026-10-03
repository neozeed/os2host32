# TELNETPM phase 2 R5: native IBM TCP/IP adapters and switch-list queries

## R5: fixes driven by the R4 Windows logs

The R4 smoke passes resolver and ordinary TCP operations, then fails at
`close_socket(peer)` while another thread is inside `recv`. Its 2000 calls to
`Sleep(1)` counted iterations rather than elapsed milliseconds. A Windows
timer tick can make that loop take far longer than two seconds; `shutdown`
also did not release the outstanding synchronous receive in this run.

R5 keeps native sockets nonblocking and implements guest blocking behavior
with readiness waits capped at 50 ms. Close sets a cancellation flag, waits
for borrowers to release their references, then closes the native handle.
Pending receive/accept/send/connect/select calls can return SOCEINTR(10004).
The drain deadline uses elapsed `GetTickCount` time, with a 2000 ms bound.
No native socket is closed while a borrower can still be inside Winsock.
Positive linger may separately extend a blocking guest's final native close.

The R4 TELNETPM trace now gets through socket initialization and window
creation; its next stop is PMSHAPI.125 at object 1+0x4733. R5 adds
`WinQuerySwitchHandle`(125) and `WinQuerySwitchEntry`(124), and replaces the
advisory add/change/remove switch stubs (120/123/129) with stored records.
The title/flags returned by query reflect changes, removed handles fail,
and a reused Win32 HWND does not resurrect a destroyed window's HSWITCH.
The first lookup of an own-process frame creates the entry PM normally
supplies automatically. There are at most 128 entries, one per frame.

The 32-bit `SWCNTRL` has seven DWORD fields, a 64-byte title area, then a
DWORD program type: **96 bytes**, with the title at offset 28 and program
type at offset 92. The API title limit is 60 characters. This matches the
guest's stack layout and the IBM toolkit's flat 32-bit header, not the
older 16-bit header. These records are process-local compatibility data;
they do not control Windows Alt-Tab membership or change window captions.
Cross-process shell enumeration and a complete OS/2 shell are outside scope.

The same installed-runtime baseline should have **12 imports still deferred**
after this patch (previously 14). The next actual missing call still produces
probe exit code 4. This update does not claim a complete TELNETPM session.

## R4 networking foundation

The R3 Windows log reaches `so32dll.26` at object 1+0x3B7D. This is
`sock_init`, confirmed against the resident/nonresident export names in the
user-supplied OS/2 SO32DLL.DLL. R4 implements all twelve networking ordinals
imported by the **273,544-byte** TELNETPM.EXE and enables native network DLL
binding in `--telnetpm-probe`.

Target SHA-256:
`16f34d712cdefb8b3f4fa93b7956fc97429697419deccd80b62b8d838d4960e2`.
The exact-image bridge guard remains. The newer 276,480-byte TELNETPM is a
different executable and is not enabled by this change. WHP is unchanged.

## Required TELNETPM imports

| Import | IBM API | Implementation |
| --- | --- | --- |
| SO32DLL.26 | sock_init | Idempotent WSAStartup(2.2), outside DllMain |
| SO32DLL.16 | socket | IPv4 TCP/UDP; small guest descriptor table |
| SO32DLL.3 | connect | Convert old 16-byte sockaddr_in, then Winsock connect |
| SO32DLL.10 | recv | Preserve byte count, EOF, OOB/PEEK and errors |
| SO32DLL.13 | send | Preserve partial sends, OOB/DONTROUTE and errors |
| SO32DLL.17 | soclose | Close translated handle; preserve descriptor if close fails |
| SO32DLL.20 | sock_errno | Per-thread IBM socket error; success does not clear it |
| SO32DLL.25 | shutdown | Receive/send/both values 0/1/2 |
| TCP32DLL.4 | bswap | Swap the low 16 bits, used as htons/ntohs |
| TCP32DLL.5 | inet_addr | Winsock IPv4 parser and 0xffffffff failure sentinel |
| TCP32DLL.11 | gethostbyname | Deep-copy resolver output into IBM's 20-byte hostent |
| TCP32DLL.24 | getservbyname | Deep-copy output into IBM's 16-byte servent |

The ordinal identities also match the supplied TCP32DLL.DLL export tables.
Actual guest call sites show caller stack cleanup. All new exports use cdecl
veneers; Winsock's stdcall entry points are called internally.

## Additional exports

SO32DLL: ACCEPT(1), BIND(2), GETPEERNAME(5), GETSOCKNAME(6), GETSOCKOPT(7),
IOCTL(8), LISTEN(9), RECVFROM(11), SELECT(12), SENDTO(14), SETSOCKOPT(15),
SET_ERRNO(35).

TCP32DLL: LSWAP(3), INET_NTOA(10), GETHOSTBYADDR(12), GETSERVBYPORT(23),
GETHOSTNAME(44), TCP_H_ERRNO(51).

There are **30 exports total**, including the twelve required by TELNETPM.
Both original uppercase names and original ordinals are exported. Gaps in the
export tables remain empty: unsupported calls are not assigned dummy handlers.
TCP32DLL imports SOCK_INIT and SET_ERRNO from SO32DLL so initialization and
socket error state are shared by the two modules.

## ABI details that matter

* TELNETPM stores the socket returned at object 1+0x9A24 in a signed 16-bit
  variable. Guest descriptors are 0..255; Windows handles never reach it.
  The table is locked, and borrowed handles are counted. Guest blocking mode
  is separate from native nonblocking mode. Close cancels readiness waits
  and drains borrowers before closing or recycling the native handle. If
  draining exceeds its deadline, close fails with SOCEINPROGRESS and retains
  the descriptor for retry. A failed native close also retains the descriptor.
* At object 1+0x9933, +0x993D and +0x9943, the guest reads hostent's family,
  address length and address-list pointer at offsets 8, 12 and 16. Winsock's
  shorts at offsets 8/10 and pointer at 12 cannot be returned directly.
* The guest reads a **32-bit** service port at object 1+0x99C5. The adapter
  zero-extends Winsock's 16-bit network-order port. It does not copy padding.
* The alternate-address path modifies hostent's address-list pointer at
  object 1+0x9AB0. Allocation ownership is therefore tracked separately from
  guest-visible pointers. Host and service results use separate, per-thread,
  deep-copy buffers, released on the next same-kind successful lookup or
  normal thread exit. A result is limited to 64 KiB; overflow fails.
* IBM's socket error numbers use the same 10000-based BSD values as Winsock
  for common errors (including 10035, 10054 and 10061). Windows-only provider
  errors map to SOCEOS2ERR(10100); allocation/aborted-operation errors map to
  SOCENOBUFS/SOCEINTR. TCP_H_ERRNO maps resolver failures to 1..4 separately.
* SELECT(12) is the old five-argument **integer-array API**, not BSD select.
  It translates three adjacent read/write/exception groups to Winsock fd_sets.
  Timeout is milliseconds; -1 blocks, 0 polls. A positive result marks
  nonready entries -1 without compacting the array. Timeout/error leaves it
  intact. Empty finite waits use Sleep because Winsock rejects empty sets.
* IOCTL(8) takes **four arguments**, including data length. Implemented:
  FIONBIO(0x8004667e), FIONREAD(0x4004667f), SIOCATMARK(0x40047307).
* Options supported: SO_REUSEADDR, SO_KEEPALIVE, SO_DONTROUTE, SO_BROADCAST,
  SO_LINGER, SO_OOBINLINE, SO_SNDBUF, SO_RCVBUF, SO_ERROR, SO_TYPE, TCP_NODELAY.
  IBM linger has two 32-bit integers; Winsock has two 16-bit fields.

## Scope and limitations

This is the old IBM TCP/IP 1.x--4.0 flat i386 API, using a 16-bit address family
at sockaddr offset 0. It does not implement the later length-byte sockaddr,
IPv6, AF_UNIX, raw sockets, BSDSELECT(32), scatter/gather, SO_CANCEL, socket
inheritance, or OS/2 interface/route-table ioctls. Unknown flags, options and
families return a concrete error. Original OS/2 networking DLLs are LX images;
install the supplied **PE Win32** DLLs for the native loader.

Host/service lookup uses the Windows resolver and service database. OS/2 ETC
hosts/services/resolv files are not parsed. Keep the existing ETC setting for
TELNETPM's own profile; R4 does not require a populated TELNETPM.INI.

These DLLs have process lifetime in os2host32. Repeated sock_init calls do not
accumulate WSAStartup references. DLL_PROCESS_DETACH does not call Winsock
under the loader lock; Windows releases sockets and the WSA reference on
process exit. Explicit unload/reload while sockets or worker threads exist is
unsupported. An outstanding operation that cannot drain within 2000 elapsed
milliseconds causes close to fail with SOCEINPROGRESS.

Remaining PM/DOS/profile/message/control imports can still stop the probe.
Implementing these socket imports does not establish that the whole TELNETPM
UI or a remote Telnet session works. Missing imports still stop with code 4.

## Build and validation

From the R4 source tree with the incremental R5 patch applied:

```sh
make SO32DLL.dll TCP32DLL.dll PMSHAPI.dll socket-smoke.exe telnetpm-api-smoke.exe
make catalog-check
make socket-dll-check
make telnetpm-api-host-check
make telnetpm-bridge-check TELNETPM=/path/to/old/telnetpm.exe
```

The socket DLL test requires Python 3, Unicorn and permission to create local
TCP/UDP sockets. It executes the production **compiled i386 DLL code**, binds
TCP32DLL's imports to the actual SO32DLL exports, and checks caller cleanup
and callee-saved registers on every invocation. Win32 APIs and resolver data
are controlled shims; transport uses real 127.0.0.1 TCP/UDP connections. It
bypasses MinGW CRT startup and calls the sources' DllMain directly. It does
not test the Windows loader, real Winsock providers, or Windows scheduling.
R5 also interleaves two compiled DLL call stacks at native wait boundaries:
one blocks and the other calls SOCLOSE. This covers TCP receive, accept,
UDP receive, and infinite select cancellation without faking a shutdown
wakeup. Finite multi-slice select and failed-close handle retention are tested.

The separate `socket-smoke.exe` loads both DLLs by their original ordinals
and exercises actual Winsock on Windows: resolver layouts, per-thread state,
TCP and UDP loopback, select, nonblocking receive, linger, orderly shutdown,
close during receive/accept/UDP receive/infinite select, and descriptor reuse.
It prints elapsed close times for the four cancellation cases. The separate
`telnetpm-api-smoke.exe` exercises the five switch APIs by ordinal alongside
the R3 PM/profile checks. No external server is required.
The host tests were run here; the Windows smoke EXE was cross-built here and
must be run on Windows. See the package's validation logs for exact results.

## Reference sources

The supplied IBM DLLs and old TELNETPM disassembly are the authoritative
ordinal and call-site evidence for this specimen. Online references checked:

* IBM TCP/IP programming reference, mirrored from the OS/2 toolkit:
  [socket](https://komh.github.io/os2books/os2tk45/tcppr/206_L2_socket.html),
  [sock_errno](https://komh.github.io/os2books/os2tk45/tcppr/205_L2_sock_errno.html),
  [os2_select](https://komh.github.io/os2books/os2tk45/tcppr/190_L2_os2_select.html).
* [Apache APR's native OS/2 error constants](https://apr.apache.org/docs/apr/trunk/apr__errno_8h_source.html).
* OS/2 libc's [netdb.h](https://github.com/bitwiseworks/libc/blob/master/src/emx/include/netdb.h)
  and [native TCP interface](https://github.com/bitwiseworks/libc/blob/master/src/emx/include/InnoTekLIBC/tcpip.h).
  Its libc nerrno aliases are not the raw IBM sock_errno numeric values.
* Microsoft [WSAStartup](https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-wsastartup),
  [hostent](https://learn.microsoft.com/en-us/windows/win32/api/winsock/ns-winsock-hostent),
  [servent](https://learn.microsoft.com/en-us/windows/win32/api/winsock/ns-winsock-servent),
  [select](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-select),
  [closesocket](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-closesocket).
* IBM OS/2 Toolkit 4.5 [32-bit pmshl.h](https://github.com/bitwiseworks/os2tk45/blob/master/h/pmshl.h)
  and mirrored original IBM PM reference:
  [WinQuerySwitchHandle](https://komh.github.io/os2books/os2tk45/pm2/2224_L2H_WinQuerySwitchHandle.html),
  [WinQuerySwitchEntry](https://komh.github.io/os2books/os2tk45/pm2/2214_L2H_WinQuerySwitchEntryS.html),
  [WinChangeSwitchEntry](https://komh.github.io/os2books/os2tk45/pm2/116_L2H_WinChangeSwitchEntry.html).
