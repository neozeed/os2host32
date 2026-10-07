R5D update: see [PM fixes and Windows test steps](docs/R5D-PM-HANDOFF.md).

# Soft386 OS/2 R5C

## R5C TELNETPM and TCP/IP

The exact TELNETPM-1993 image now has a jar-specific mixed-mode profile.
SO32DLL and TCP32DLL use explicit copied-buffer bridges with asynchronous
native workers; blocked network calls yield the guest CPU. Native resolver
pointer graphs become guest pointers, with separate per-thread host/service/
address result storage. DosSearchPath consults the guest environment, and
DosBeep waits asynchronously. CRT exit callbacks and exception-chain records
remain in the jar. Supporting PM accelerator, cursor, font and switch calls
are also marshalled. Existing system DLL sources and WHP are unchanged.

Run `make check` for host regressions. `make win32` uses 32-bit MinGW.
The supplied Windows smoke kit needs neither Python nor a compiler.
**TELNETPM live GUI/network acceptance remains pending.** See
[R5C handoff](docs/R5C-TELNET-NET-HANDOFF.md) for tested scope and limitations.

R5B live feedback now confirms BIO's two windows, SimCity startup, Life, and
SarienPM loading. NEKO still shows only its control panel; R5C does not claim
to repair the missing cat.

## R5B runtime gaps from the live demo traces

R5B corrects the 13-argument WinCreateWindow ABI and adds WinSubclassWindow,
WinDlgBox, WinQueryPointerInfo, WinQueryWindowProcess, WinScrollWindow,
WinDrawBitmap, dialog short/text-limit helpers and WinSetSysModalWindow.
Saved native control procedures become callable guest veneers with private
native targets. QUECALLS.9/.14/.15/.16 use an explicit bridge; empty WAIT reads
park the guest thread, while payload values remain opaque 32-bit values.

The full host regression suite, new real-CPU callback/queue fixtures, sanitizer
boundary checks and i686 Windows build pass. Live R5B GUI acceptance is pending.
The system DLL sources and WHP remain unchanged. See
[R5B changes, limits and Windows steps](docs/R5B-PM-QUEUE-HANDOFF.md).

## R5A loader and PM service correction

HANOI has now been reported working live on Windows with R5. R5A fixes the
misleading BIO/Sarien-style "not LE/LX" error when an unrecognized system
personality is found as a native PE DLL through OS2LIBPATH. PMSHAPI and HELPMGR
have explicit marshal routes; PMWP.203 loads the requested user DLL inside the
jar. LX iterated pages are decoded with checked input and output bounds, and
resource-only DLL objects receive independent guest allocations.

The supplied BIO, HANOI, NEKO and NEKO.DLL pass R5A loader checks. This is not a
claim of complete GUI compatibility: unimplemented PM imports are now marked
in --check output. See [R5A correction and Windows steps](docs/R5A-LOADER-HANDOFF.md).
The original system DLL sources and WHP remain unchanged.

## R5 Presentation Manager and guest DLLs

R5 adds a bounded PMWIN/PMGPI/PMCTLS/MSG bridge and LE/LX user DLL loading.
System personality DLL sources are unchanged. Window procedures execute in the
Tiny386 jar, including nested callbacks and worker-to-PM-owner calls. The bridge
saves integer, segment, flags and x87 state, keeps native handles behind typed
guest tokens, copies buffers/resources, and lets other guest threads run while
`WinGetMsg` waits.

The headless end-to-end fixture loads `PMJAR.DLL`, runs its initialization,
registers its guest window procedure, yields inside `WM_CREATE`, performs a
nested send, paints, receives a worker send, destroys the window and terminates
the DLL. GCC/Clang host checks and the i686 Windows build were validated.
The user subsequently reported HANOI working live on Windows. The synthetic
smoke kit and each additional application still need their own live acceptance.

Start with [R5 handoff](docs/R5-PM-DLL-HANDOFF.md),
[Windows acceptance](docs/WINDOWS-TEST-R5.md), and the
[PM bridge catalogue](docs/R5-PM-API-SURFACE.md).

The initial scope is one PM message-queue owner per jar, plus guest workers and
independent child jars. Full PM, arbitrary pointer-bearing control messages,
physical DLL unloading and general 16-bit OS/2 execution are not implemented.

## Preserved R4 checkpoint

## R4 optional 80387/x87

R4 vendors the Tiny386 optional x87 implementation supplied for this milestone,
builds the CPU with `I386_ENABLE_FPU`, links `tiny386/fpu.c`, and enables the
FPU before the protected-mode guest reset.  The existing R3 process-vessel
model is unchanged: every child jar gets its own Tiny386/FPU instance.

The saved guest-thread CPU image now includes an opaque byte-exact FPU snapshot,
so x87 state follows the virtual OS/2 TID across Soft386 scheduler switches.
Regression fixtures prove both arithmetic/transcendental execution and isolation
between two guest threads.  See `docs/R4-80387.md` and `docs/WINDOWS-TEST-R4.md`.

The imported Tiny386 FPU source explicitly describes itself as incomplete: x87
exceptions, a true tag word and full 80-bit precision are not modelled exactly.
R4 therefore treats it as a practical 80387 compatibility layer, not a claim of
bit-perfect coprocessor emulation.

## R3 process vessels

R3 adds `DosExecPgm`/`DosWaitChild` process orchestration by reusing the existing
native DOSCALLS process backend through the bounded marshalling seam.  Every OS/2
child re-enters `soft386_os2.exe`, so **one host process remains one Tiny386 OS/2
process vessel** with private CPU state and guest RAM.  CMD32 external programs and
anonymous pipelines can therefore span multiple jars without sharing guest memory.

The parent publishes itself as `OS2HOST32_LOADER`, propagates absolute personality
DLL paths to descendants, accepts the native loader's `--run-quiet`/`--argv0`
switches, and preserves OS/2 argument/environment/std-handle inheritance.  Normal
scheduler messages are now silent unless tracing is requested.  See
`docs/R3-PROCESS-VESSELS.md` and `docs/WINDOWS-TEST-R3.md`.



### R2C C/386 DOSCALLS.32 correction

R2C adds the historical C/386 migration descriptor for `DOSCALLS.32`
(`Dos16Sleep` / `DosSleep` with one 32-bit millisecond argument).  The migration
helper is lowered to the usual 32-bit Soft386 hostcall, but the call is handled
inside the jar by the virtual scheduler rather than delegated to `DOSCALLS.dll`.
This is the path used by INCL_16-style programs such as the OS/2 Life demo.


## R2B corrective note

R2B keeps the R2 guest ABI and system-module work but tightens the Windows host boundary after a live regression was observed while unrelated VIO/KBD/SESMGR DLLs were eagerly loaded for a DOSCALLS-only image.  System DLLs are now loaded only when the LE/LX import-module table actually names that personality.  DOSCALLS-only programs such as the historical `hi.exe` and `nlsinfo.exe` therefore do not load VIO/KBD/SESMGR as a side effect.

R2B also deliberately rejects `_WIN64`: the native os2host32 personality DLL set is a 32-bit x86 ABI and Soft386's current native bridge is a 32-bit milestone.  Use RosBE/i386 or `i686-w64-mingw32-gcc`.  `--trace-native` now prints the image system-module mask and `DosQueryHType` prints a post-call return marker, making a native-DLL fault distinguishable from a guest-side failure.


Soft386 is the software-CPU peer of the existing `whp/` loader. It embeds the
Tiny386 80386 core and runs untouched **32-bit OS/2 LE/LX** application code in
private guest RAM. No BIOS, DOS, real-mode bootstrap, VM86 layer, or hardware
hypervisor is involved.

R0 proved an untouched historical `hi.exe` on Tiny386. R1 proved the split
between a jar-owned OS/2 kernel/process world and a bounded native
`DOSCALLS.dll` bridge. R2 expands that personality boundary to the character
mode system modules normally imported by C/386 programs:

* `DOSCALLS`
* `VIOCALLS`
* `KBDCALLS`
* `SESMGR`
* direct `NLS` ordinals 5/6/7 (jar/common implementation)

R2 also ports the live-proven WHP lowering for Microsoft C/386's known 32->16
VIO/KBD migration helpers. The helper fragments are recognized and replaced
with 32-bit hostcall veneers before execution. **Soft386 R2 still does not
execute 16-bit guest code.**

## Guest execution and module hostcalls

The guest starts directly in 80386 protected mode with flat 4 GiB 32-bit
code/data segments. LE/LX object pages are copied into a 64 MiB guest RAM
array, fixups are applied there, a C/386-compatible startup stack is built,
and Tiny386 begins at the LE/LX entry point.

Normal 32-bit imports use ordinary guest veneers:

```asm
mov eax, module_id | ordinal
out 0F0h, eax
ret
```

R2 hostcall classes are:

* `0x01000000 | ordinal` — DOSCALLS
* `0x02000001` — private runtime thread-return gate
* `0x03000000 | ordinal` — VIOCALLS flat 32-bit ABI
* `0x04000000 | ordinal` — KBDCALLS flat 32-bit ABI
* `0x05000000 | ordinal` — SESMGR flat 32-bit ABI
* `0x06000000 | ordinal` — NLS common/jar ABI
* `0x07000000 | descriptor` — lowered C/386 packed VIO/KBD helper

`DOSCALLS.425` (`DosFlatToSel`) is the C/386 migration token and is emitted as
a plain `RET`, preserving the flat address in EAX exactly as in the WHP path.

## C/386 migration helper lowering

CMD32 and NLSINFO were built with Microsoft C/386. Their VIO/KBD calls contain
small 16-bit migration thunks plus a 32-bit helper. R2 validates the complete
known pattern, including:

* the 14-byte far-call/far-return migration fragment;
* internal PTR32 return fixup;
* selector alias back to the 16-bit fragment;
* helper prologue and `LSS` transition sequence;
* stack-selector comparison fixup;
* exact per-API packed argument widths.

After validation the 32-bit helper prologue is redirected to a synthetic
32-bit `OUT 0xF0` veneer with `RET imm16`. The original 16-bit fragment is
never entered. Unknown or partially matching non-flat fixups are rejected
rather than guessed.

Current descriptors mirror the frozen native loader surface:

VIOCALLS: 7, 9, 13, 15, 19, 21, 24, 26, 27, 32, 48, 52.

KBDCALLS: 4, 9, 10, 11, 13, 22.

## Ownership rule

### Stays in the jar

Soft386 remains authoritative for objects that define the virtual OS/2
process/kernel:

* guest memory allocations and OS/2-visible protection state;
* virtual OS/2 threads and saved Tiny386 CPU contexts;
* PIB/TIB and per-thread FS descriptor state;
* sleep/wait scheduling;
* event and mutex semaphore objects;
* process exit;
* guest module/address namespace;
* per-process NLS country/codepage/case/DBCS state.

NLS DOSCALLS aliases implemented from common NLS are 289, 291, 395, 396 and
397. Direct `NLS.5/.6/.7` resolves to the same state.

### Marshalled native host services

On Win32 R2 can load the repository's existing native 32-bit DLLs by ordinal:

* `DOSCALLS.dll` — R1 host-facing file/path/HFILE/HDIR service tranche;
* `VIOCALLS.dll` — current VIO character-mode surface;
* `KBDCALLS.dll` — current keyboard surface;
* `SESMGR.dll` — title/session start/stop/private session enumeration surface.

Guest pointers are never passed to a native DLL. Strings, buffers and wire
structures are range-checked and copied into host memory; output is copied
back only after guest-range validation.

No existing DLL source or export definition is modified by Soft386 R2.

## Current VIO/KBD/SESMGR native surfaces

VIOCALLS:

* 7 `VioScrollUp`
* 9 `VioGetCurPos`
* 13 `VioWrtCharStr`
* 15 `VioSetCurPos`
* 19 `VioWrtTTY`
* 21 `VioGetMode`
* 24 `VioReadCellStr`
* 26 `VioWrtNAttr`
* 27 `VioGetCurType`
* 32 `VioSetCurType`
* 48 `VioWrtCharStrAtt`
* 52 `VioWrtNCell`

KBDCALLS:

* 4 `KbdCharIn`
* 9 `KbdStringIn`
* 10 `KbdGetStatus`
* 11 `KbdSetStatus`
* 13 `KbdFlushBuffer`
* 22 `KbdPeek`

SESMGR:

* 5 `DosSmSetTitle`
* 8 `DosStopSession`
* 17 `DosStartSession`
* 1000 `O2HostQuerySessions`

`DosStartSession` deep-marshals the packed 32-bit STARTDATA wire structure and
its referenced strings/environment/object buffer before calling the native
DLL.

## Important R2 limitations

* Main executable only; no guest DLL loader/export resolver yet.
* Named imports remain unsupported.
* Only the recognized C/386 VIO/KBD migration helper family is lowered. General
  16-bit code/fixups are still rejected.
* Native `KbdCharIn(IO_WAIT)` and `KbdStringIn(IO_WAIT)` currently block the
  Soft386 host thread. This is adequate for a first interactive CMD/NLSINFO
  proof, but a later phase should poll the common nonblocking KBD seam and
  deschedule only the waiting virtual OS/2 thread.
* Native `DosStartSession` currently uses the existing Win32 SESMGR backend; it
  does not yet launch a child *inside another Soft386 jar*.
* Cross-process named semaphore/shared-memory/system-wide PID namespaces do not yet have a global Soft386 broker.  R3 covers parent/child process trees and inherited anonymous pipes.

## Building

Portable regression (validated with GCC and Clang):

```sh
cd soft386
make
make check
```

Build Soft386 plus the four native Windows service DLLs:

```bat
make soft386-system-win32
```

The older convenience name remains equivalent:

```bat
make soft386-native-win32
```

On RosBE where the compiler command is `gcc`:

```bat
make MINGW=gcc soft386-system-win32
```

Or build only Soft386 directly:

```bat
cd soft386
make WINCC=gcc win32
```

## Running on Windows

First verify a C/386 executable can be parsed and its helper fragments lowered:

```bat
soft386_os2.exe --check C:\path\cmd32os2_os2.exe
soft386_os2.exe --check C:\path\nlsinfo.exe
```

For explicit DLL paths from the `soft386` directory:

```bat
soft386_os2.exe ^
  --doscalls-dll ..\DOSCALLS.dll ^
  --viocalls-dll ..\VIOCALLS.dll ^
  --kbdcalls-dll ..\KBDCALLS.dll ^
  --sesmgr-dll ..\SESMGR.dll ^
  --trace-native --trace-hc ^
  C:\path\nlsinfo.exe
```

CMD32 builtins can be tested similarly, for example:

```bat
soft386_os2.exe ^
  --doscalls-dll ..\DOSCALLS.dll ^
  --viocalls-dll ..\VIOCALLS.dll ^
  --kbdcalls-dll ..\KBDCALLS.dll ^
  --sesmgr-dll ..\SESMGR.dll ^
  --trace-native --trace-hc ^
  C:\path\cmd32os2_os2.exe -c "echo SOFT386_CMD32_PASS"
```

With the four DLLs discoverable by normal Win32 DLL search rules the explicit
paths may be omitted.

## Regression fixtures

R2 retains every R0/R1 fixture and adds:

* `system-modules-soft386.le` — resolves DOS/VIO/KBD/SES module classes even
  when native providers are absent;
* `nls-alias-soft386.le` — exercises DOSCALLS 289/291/395/396/397 against
  jar-owned common NLS state;
* `nls-module-soft386.le` — proves the direct NLS module path;
* `system-bridge-check` — fake-provider pointer-marshalling test for flat and
  packed C/386 VIO/KBD plus SESMGR.

`make check` runs the complete inherited R1 runtime suite and the R2 additions.

## Live milestone status

R1 is live-proven on Windows:

* native DOSCALLS file bridge PASS;
* untouched historical `hi.exe` mixed jar/native routing PASS;
* output `hi!`, guest exit status 4.

R2's parser/dispatcher/common-NLS/marshalling implementation is independently
GCC/Clang regression-proven here. Actual Windows `VIOCALLS.dll` / `KBDCALLS.dll`
/`SESMGR.dll` loading and real CMD32/NLSINFO C/386 helper lowering remain the
next live acceptance test.
