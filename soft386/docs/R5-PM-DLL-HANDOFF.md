# Soft386 R5 — Presentation Manager / guest DLL handoff

Date: 2026-10-04. Base: supplied `os2host32-f0c6ff2.zip` and the R4 checkpoint.
Status: implemented, host-tested with the real Tiny386 CPU and headless native
providers, and cross-built for i686 Windows. **Windows visual/live acceptance
has not been run here.** R4's earlier live results remain historical evidence.

## Architecture and ownership

One OS/2 process remains one host process, with its own Runtime, Tiny386 CPU,
64 MiB RAM, guest threads, DLL instances, resource copies and PM handle table.
Children continue to re-enter `soft386_os2.exe` through the existing process and
session backend. PM personality paths are now propagated in that environment.
No parent jar lends its RAM or callback registrations to a child.

The sources in `dlls/`, `common/`, the native loader, WHP and OS2SS are unchanged.
The changes are confined to `soft386/`. Existing PMWIN, PMGPI, PMCTLS and MSG
are used as native services. They never execute an LE/LX procedure directly.

| State | Owner / representation |
| --- | --- |
| EXE/DLL code, data, relocation targets, exported functions | Guest addresses in jar RAM |
| Callback argument/return frames | Owning guest thread's stack |
| Suspended CPU, flags, segments/FS and x87 state | Vessel callback continuation |
| PM HWND/HAB/HMQ/HPS/HDC/bitmap/region identities | Typed jar tokens; native backing kept in the bridge |
| User `MPARAM` pointers in messages to another guest procedure | Original guest values, in the same jar |
| Pointer-bearing native API arguments | Bounded host copies and explicit copy-back |
| Native queued messages | Private bridge records; public QMSG contains an integrity token |
| Resources consumed by native PM | Separate immutable host copies, keyed by guest HMODULE |

## Host-to-guest callbacks

Native PM can call a procedure synchronously during creation, painting,
`WinSendMsg`, showing a window or loading a modeless dialog. The bridge supplies
one of 64 compiled host thunks instead of the guest procedure's address. Each
binding records the Runtime, guest procedure address and owner TID.

The thunk validates the executable address and available guest stack, saves the
interrupted state, installs a four-argument flat32 call frame and runs Tiny386
until a checked return sentinel. Both caller-clean and callee-clean returns
are recognized. DF is clear on entry and restored on return. A callback result
is returned to the native DLL, after which the original guest API call resumes.

The native DLL's synchronous call frame remains on the host stack. The guest
CPU is only stepped after the preceding Tiny386 step has completed; this is
bounded re-entry of the vessel run loop, not concurrent execution of one CPU.
Nested callbacks use the same scheduler and saved x87 images. Limits: 16 frames
per guest thread and 32 active native callback frames across the vessel.

If the target is another runnable or PM-waiting guest thread, the native caller
is marked `THREAD_HOSTCALL`, the owner is selected, and both continuations are
restored afterward. The parked native caller is never scheduled while its
native call is incomplete. Cross-thread calls into an owner suspended on a
semaphore, sleep, another native call or explicit suspension are rejected in
this milestone; a proper inter-thread sent-message queue is still needed.

Invalid return frames, callback halts and cycle-limit failures unwind into a
process failure. A thread exiting inside a synchronous native callback also
fails the vessel rather than reviving the exited thread. `DosExit(EXIT_PROCESS)`
continues to unwind the process. This is an explicit initial lifecycle boundary.

## PM message loop

`WinGetMsg` uses the backend's nonblocking `WinPeekMsg`, plus jar-owned posted
messages. With no message it records `THREAD_WAIT_PM`, saves the import
continuation and yields. It retries after a 10 ms wait. A 4096-instruction
quantum also prevents a compute-bound worker from starving the message owner.

The existing native DLL defers showing standard windows to its own GetMsg.
The bridge tracks requested visibility and calls native `WinShowWindow` on the
first guest GetMsg/PeekMsg, after guest creation bookkeeping has completed.
It never enters native blocking GetMessage for this loop.

Public QMSG remains the repository's 28-byte ABI. The reserved WORD is a token;
native pointer-valued fields remain in the saved record. Dispatch validates the
complete returned QMSG before using its native counterpart. Copying an intact
QMSG to another guest address works. Forging/editing it, or dispatching a
consumed record, fails. For guest-posted messages, the original guest MPARAMs
are preserved and the procedure is invoked directly through the callback engine.
Do not treat the tokenized QMSG as a complete general OS/2 queue implementation.

One native PM queue owner is supported per jar. Other guest threads may compute,
draw through supported APIs, send to a runnable/PM-waiting owner, or post messages.
Child jars have independent owners. Native modal message/file dialogs can still
block the vessel outside callbacks; they do not yet have scheduler continuations.

## Guest DLL loader

- Static and `DosLoadModule` loading of 32-bit LE/LX user DLLs.
- Search: explicit path; main executable directory; current directory;
  semicolon-separated `OS2LIBPATH`. Missing extension defaults to `.DLL`.
- Ordinal and resident/nonresident named exports, including data exports.
- Transitive imports; objects/exports are published before cyclic dependencies
  are resolved. Dependency initialization is ordered, with visiting-cycle edges
  skipped. Cyclic initialization has not been a live acceptance target.
- External OFF32 and REL32 fixups, by ordinal or name; additive fields applied.
- Private module arena `03000000..03EFFFFF`, at most 32 guest DLLs, one instance
  of each module per jar shared by that jar's guest threads.
- Initializers run as guest code before application entry or before dynamic
  load returns. The existing loader's early INITINSTANCE `(0,hmod)` convention
  and later `(hmod,flag)` convention are preserved.
- Normal process termination calls flagged DLL terminators in reverse successful
  initialization order. Abnormal execution is not a complete OS/2 exit-list model.
- Module APIs: DOSCALLS.318/.319/.320/.321; resource APIs .352/.353/.572.
- `DosFreeModule` (.322) returns ERROR_NOT_SUPPORTED (50) for a guest DLL. Code
  remains mapped; physical unload and callback invalidation are deliberately
  deferred. This is not a fake successful unload.

16-bit exports/entry points, typed entries, forwarders, internal entry-table
fixups and unsupported page encodings still fail explicitly. Existing C/386
migration-helper lowering is retained; this is not general 16-bit OS/2 support.
Malformed DLL parsing still uses the vessel's fatal loader error path rather
than transactionally rolling back a failed dynamic load.

## Native marshalling

See `R5-PM-API-SURFACE.md` for the exact catalogue. The main implemented groups
are class/window creation, modeless dialogs, message dispatch, painting, text,
rectangles/points, timers, window properties, PS/DC/bitmap/region operations,
font metrics, counted drawing arrays and bitmap/palette buffers. PMCTLS.4 copies
the supported FILEDLG strings and output fields. MSG.4/.6 deep-copy substitution
strings and buffers; the native MSG backend's bound-message limitations remain.

Arbitrary control-message pointer layouts, subclass-procedure round trips,
modal WinDlgBox, clipboard ownership, accelerators, PMSHAPI/PMWP/HELPMGR and a
complete PM error-state model are not covered. Unknown APIs/messages fail;
there is no catch-all pointer cast. The broad native PM surface does not imply
that every existing native PM application will run in R5.

Native modules remain lazy. Console personalities imported only by a newly
loaded guest DLL can now be opened without resetting existing bridges.
Resources are copied before native registration and retained through cleanup.
The fixed tables currently permit 1024 PM handles, 128 message records and
256 resource copies; exhaustion is an error, not unbounded allocation.

## Proofs and builds

`make -C soft386 check` runs R2C, R3, R4 and R5 checks. R5 covers:

- LE and LX callers, transitive named imports and an OFF32 DLL data import.
- Dynamic load/query, missing-module outputs, explicit unload rejection and
  legacy INITINSTANCE argument order.
- A real LX DLL procedure entered by a native provider during WM_CREATE.
- Nested sends, sleep inside creation, a worker running during that sleep,
  worker-to-PM-owner sends while GetMsg waits, WM_PAINT and WM_DESTROY.
- Per-thread FS/TID, EBX, direction flag and x87 preservation; DLL resources and
  guest DLL termination.
- Host-pointer separation, invalid ranges, typed-handle confusion, nested MSG
  strings, bitmap scan/palette copy-back, QMSG tampering/replay and FIFO order.
- Malformed callback RET, HLT and a nonreturning callback hitting the cycle limit.

GCC and Clang host regression runs pass. The standalone PM boundary check also
passes AddressSanitizer and UndefinedBehaviorSanitizer. LeakSanitizer could not
inspect this execution environment's task list, so that run used
`ASAN_OPTIONS=detect_leaks=0`; leak checking is not claimed.

i686 MinGW builds the vessel and the unchanged native service DLLs. The vessel
is a console PE32, importing only KERNEL32 and msvcrt; no libwinpthread DLL is
required. Tiny386's existing `clock_gettime` RDTSC timing path needed a small
Win32 QueryPerformanceCounter portability branch. Instruction/FPU logic is
otherwise unchanged. Existing vendored-core warnings remain in build logs.

No Wine/display-backed Windows execution was available. The next decisive proof
is `RUN-R5.cmd` from the supplied Windows smoke kit, followed by the user's
existing PM examples. Keep the R4 jar executable until that acceptance passes.

## Files and next work

- `src/soft386_modules.h`: module graph, exports, fixups/resource intake helpers.
- `src/soft386_callbacks.h`: callback entry/return, lifecycle and module APIs.
- `src/soft386_pm_bridge.[ch]`: typed handles, marshal catalogue, native thunks,
  tokenized queues and resource copies.
- `tests/mkfixtures_r5.py`: reproducible CRT-free LE/LX fixtures.
- `tests/pm-runtime-check.c`: headless provider with the real vessel/CPU.
- `tests/pm-bridge-check.c`: boundary and memory tests.
- `tests/test_r5.py`: regression and failure-unwind driver.

After live PM acceptance, use concrete application traces to add message schemas
and missing APIs. General inter-thread sent messages and multiple PM owners need
native service-thread/continuation work because the existing PM DLL stores state
per native thread. Keep those mechanics in Soft386. Leave WHP frozen.
