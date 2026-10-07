# Soft386 R5C — TELNETPM jar profile, TCP/IP, search and beep

Base: os2host32-SOFT386-R5B.zip. Date: 2026-10-04 (UK).
Implementation changes are confined to soft386/. Existing system DLL sources,
common backends, the native loader and WHP are unchanged.

## Delivery and Windows use

- Full source: os2host32-SOFT386-R5C.zip
- Incremental text patch: SOFT386-R5C-from-R5B.patch (apply to R5B only)
- Prebuilt vessel and native DLL smoke kit: soft386-R5C-win32-smoke.zip

For an existing installation, replace C:\OS2\soft386_os2.exe with the kit's
soft386\soft386_os2.exe. Keep the installed native system DLLs and env.cmd.
SO32DLL.dll and TCP32DLL.dll must be on PATH; the kit includes builds of the
unchanged native sources if either DLL is missing. Do not overwrite a newer
native installation wholesale with the standalone kit's DLL directory.

In the same command prompt:

```bat
call C:\OS2\env.cmd
cd /d <directory-containing-TELNETPM.EXE>
C:\OS2\soft386_os2.exe --check TELNETPM.EXE
C:\OS2\soft386_os2.exe --max-cycles 0 --trace-native --run TELNETPM.EXE 2>telnetpm-r5c.log
```

Keep the application's existing configuration files and ETC setting. The jar
canonicalizes only the spelling of an inherited Etc/etc key to ETC for this
exact CRT; it preserves its value and does not invent a configuration path.
Native services are PE DLLs, found through PATH or explicit --*-dll options.
OS2LIBPATH remains the LE/LX guest-library search path.

New overrides: --so32dll-dll and --tcp32dll-dll, with SOFT386_SO32DLL_DLL and
SOFT386_TCP32DLL_DLL environment equivalents. Child jars inherit these paths.
The supplied cross-built binaries import Universal CRT API-set DLLs. They do
not retain R5B's msvcrt-only dependency set; use the established MinGW build
when targeting an older Windows/ReactOS runtime.

The banner identifies R5C. DOSCALLS must report ON in this installation.

Run RUN-R5C.cmd in the extracted smoke kit. It starts a one-connection peer
bound to 127.0.0.1:23230, then runs an LE client inside the production vessel.
The client resolves localhost, creates/connects a socket, blocks a reader
thread, sends eight binary bytes from its other thread, verifies the reply
and checks FS/x87/register preservation. The peer times out after 30 seconds.
No Python, compiler, external server or external network connection is needed.
The expected client line is "Soft386 R5C TCP CPU PASS". The server also prints
its eight-byte echo PASS. This kit has been cross-built, not run on Windows here.

## TELNETPM intake and mixed-mode adaptation

The supplied file is 273,544 bytes, SHA-256:
16f34d712cdefb8b3f4fa93b7956fc97429697419deccd80b62b8d838d4960e2

It exactly matches the native host's TELNETPM-1993 profile. R5B rejected its
small 16-bit data object before reaching its unlike-MSC/386 thunk machinery.
R5C admits this file by its complete fingerprint and verifies the inventory:
7,779 internal relocation sites, 567 external sites and seven nonflat sites.
The loader now also applies internal REL32 relocations.

The known dispatcher and local callback prologue/epilogue are adapted in jar
RAM. Every emitted target is a guest linear address. The callback's application
body is retained in production. Packed 4/2/4/2 argument widths are expanded to
flat slots, and its 16-bit return value is preserved. The two pinned selector
helpers carry flat guest tokens. General mixed-mode programs are not admitted.
External authentication far callbacks stop at a dedicated failure gate.

Unlike the native profile, no FS shadow patches are installed: Tiny386 executes
the original FS instructions against the selected thread's emulated TIB.
This does not modify TELNETPM.EXE on disk or run guest instructions natively.

## TCP/IP boundary

All exports in the supplied SO32DLL and TCP32DLL definition files have explicit
routes. SO32DLL's scalar socket IDs belong to the process-local native service;
the jar tracks ownership and rejects unknown/closed descriptors. SELECT uses
IBM's integer array ABI, never Winsock fd_set. IOCTL and socket-option payloads
remain subject to the native service's supported commands and structures.

Each request snapshots its arguments and inputs and validates outputs before
starting a native worker. Workers own host buffers and never touch RAM, CPU
state or the jar scheduler. The calling guest parks and retries its generated
veneer; completion copies results on that guest thread. Socket reads, connect,
select, DNS and other native network calls therefore do not hold the emulator's
host thread. This uses one temporary native worker per outstanding request,
not a performance-optimized persistent worker pool.

Resolver results are rebuilt in bounded guest arenas: names, aliases, address
lists and every nested pointer. Host, service and address-string results have
separate storage per guest thread, preserving their independent lifetimes.
SOCK_ERRNO/SET_ERRNO and TCP_H_ERRNO are saved per guest thread, rather than
inheriting the scheduler host thread's TLS state. As in the native service,
networking is IPv4; this is not a new IPv6 implementation.

On process teardown, known sockets are closed. Workers still inside an OS call
retain their private allocations and DLL mappings until host process exit;
code is never unloaded underneath an in-flight request. Buffer capacity is
bounded at 16 MiB per copied argument and resolver result arenas at 64 KiB each.

## DOSCALLS and PM additions

- DOSCALLS.228 DosSearchPath resolves environment-name mode from the current
  guest PIB's environment block, then supplies a literal path to the native
  filesystem service. Output is validated and copied only after success.
- DOSCALLS.286 DosBeep calls the unchanged native service on a worker. Only
  the calling guest thread waits; other jar threads can progress.
- DOSCALLS.296 DosExitList stores validated guest callback entries and handles
  add/remove/continue. Normal process exit executes ascending priorities and
  LIFO within ties, resets the callback stack, stops peer guest execution and
  transfers owned mutexes to the exiting thread. EXLST_EXIT unwinds the active
  exit callback. New process/thread creation is rejected during this phase.
  The existing callback failure and cycle-limit handling still applies.
- DOSCALLS.354/.355 maintain bounded exception registration chains in the
  selected guest TIB. .378 maintains that thread's signal-focus count. These
  are registration/bookkeeping support, not a new CPU exception dispatcher or
  asynchronous OS/2 signal delivery implementation.
- PMWIN.776/.798/.850 marshal accelerator handles; .779 copies message text;
  .812 copies CURSORINFO and translates HWND; .890 copies 76-byte TRACKINFO.
  .707/.733/.793/.807 cover clipboard open/empty/close/format metadata; .854
  admits the native backend's bitmap route, not arbitrary guest pointers.
- PMGPI.586 copies the font count and bounded metric array, rejecting a zero
  stride or overflowing multiplication.
- PMSHAPI.123/.124/.125 translate switch-list structures/handles and the jar PID.

Exit-list ordering follows IBM's OS/2 2.0 Control Program Programming Reference:
https://www.bitsavers.org/pdf/ibm/pc/os2/OS2_2.x/IBM_OS2_2.0_Technical_Library_1992/S10G-6263-00_OS2_2.0_Control_Program_Programming_Reference_199203.pdf

## Validation completed here

- Full GCC host make check: R2C, R3, R4, R5, R5A, R5B and the new R5C checks pass.
- Real POSIX loopback test through the new marshalling layer: binary send/recv,
  concurrent progress while receive is pending, immutable input snapshots,
  resolver graph rebuilding/lifetimes, per-thread error state, integer-array
  SELECT, malformed output rejection, stale sockets and asynchronous beep.
- Tiny386 LE client with injected providers: a guest reader waits while a guest
  sender runs, then resumes with correct data, FS/TID, x87 and EBX.
- Tiny386 search/exit fixture: guest search and ordered exit callbacks with
  EXLST_EXIT continuation. Additional checks verify guest-only PATH resolution,
  missing variables, short output buffers and invalid destinations.
- Exact TELNETPM: loader check passes; its original CRT executes to WinInitialize
  under a headless provider, then runs its actual CRT exit callback. The provider
  deliberately returns initialization failure: this is NOT GUI acceptance.
- Dispatcher CPU test: exact loaded profile and emitted thunk, with a controlled
  callback body substituted only in test RAM, verifies packed argument order,
  return width, saved context and rejection of unknown external callbacks.
- PM boundaries cover new accelerator tokens, cursor/track buffers and font
  counts/strides, alongside previous callback/resource/handle regressions.
- AddressSanitizer/UndefinedBehaviorSanitizer network boundary test passes;
  leak scanning is disabled in this execution environment.
- 32-bit Windows production executable, loopback peer and unchanged native
  DLLs cross-build with Zig/Clang's x86-windows-gnu target. GNU make's default
  Windows compiler remains i686-w64-mingw32-gcc for user builds.

Evidence is in soft386/validation/r5c. User executables are not redistributed
in the source or smoke kit. To rerun the exact-image checks with your own file:

```sh
make -C soft386 check telnet-profile-check
soft386/telnet-profile-check /path/to/telnetpm.exe
soft386/r5c-runtime-check /path/to/telnetpm.exe
```

## Live acceptance and remaining work

R5B live feedback established BIO's two windows, SimCity startup, Life running,
and SarienPM loading. R5C still needs Windows confirmation of HELLO's open dialog,
Sarien's beep and TELNETPM's window, connection, send/receive and close behavior.
No live TELNETPM GUI or remote session is claimed by this build.

NEKO's missing cat is not fixed here. General 16-bit execution, external far
callbacks/authentication modules, full clipboard text transfer, and remaining
unbridged DOSCALLS such as .111/.300/.356/.418 are outside this tranche. Failed
calls retain explicit errors. --check marks missing PM marshallers but is not a
complete runtime API coverage checker. A TELNETPM menu path may expose another
missing call even though startup and the network boundary now work in tests.

Inherited PM limits remain: one queue owner per jar; native modal loops do not
pump unrelated jar threads while idle; synchronous cross-thread callbacks into
an owner in an unrelated wait can be rejected; guest DLL mappings are retained.
Network workers may finish during a modal loop, but guest completion still needs
the emulator thread to resume. Hardware exceptions do not yet walk the registered
OS/2 guest exception chain.
