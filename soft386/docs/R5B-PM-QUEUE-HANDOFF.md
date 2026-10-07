# Soft386 R5B — PM runtime gaps and QUECALLS

Date: 2026-10-04. Base: the R5A package from this conversation.
All implementation changes are confined to soft386/. System DLL sources,
common backends and WHP are unchanged.

## What the user's Windows run established

R5A successfully loads BIO/WMCHAR as LE and NEKO as LX. The user's screenshots
show WMCHAR retaining text but only redrawing after a resize. NEKO opens its
first window; BIO reaches PM startup but exits without its options dialog.
The following missing hostcalls identify concrete gaps:

| Reported call | API | R5B change |
| --- | --- | --- |
| 08000336 | PMWIN.822 WinQueryPointerInfo | Copy 28-byte POINTERINFO; tokenize its four bitmap fields |
| 08000346 | PMWIN.838 WinQueryWindowProcess | Checked PID/TID outputs; owned windows report the jar's PID/TID |
| 08000351 | PMWIN.849 WinScrollWindow | Copy optional scroll/clip rectangles and update rectangle; translate HRGN |
| 0800039B | PMWIN.923 WinDlgBox | Native modal dialog with a guest callback, saved creation parameter and USHORT result |
| 080003A1 | PMWIN.929 WinSubclassWindow | Bind the guest callback and return guest code or a callable guest veneer |
| QUECALLS PE dependency | QUECALLS.9/.14/.15/.16 | Explicit native service classification and bounded queue bridge |

There was an additional signature bug: WinCreateWindow takes **13** arguments.
R5A described 12 and treated the height argument as HWND. R5B fixes the signature,
raises the marshal/call capacity to 13, and validates ctlData/presParams at the
correct positions. These two optional complex structures remain unsupported
when non-null. Control IDs and dimensions now reach the correct native slots.

Adjacent calls needed by these applications are also bridged: PMWIN.730
WinDrawBitmap, .814 WinQueryDlgItemShort, .858 WinSetDlgItemShort, .872
WinSetSysModalWindow and .903 WinSendDlgItemMsg. DrawBitmap's destination is an
8-byte POINTL or 16-byte RECTL according to DBM_STRETCH. Short query outputs copy
exactly two bytes. Native control sends use a reviewed scalar message list;
EM_SETTEXTLIMIT, list selection, slider and scrollbar values are included.
Arbitrary pointer-bearing MPARAMs remain rejected.

## Callback and handle ownership

Native PFNWPs never become Tiny386 instruction addresses. A saved procedure
returned by the native public-control subclass adapter is recorded privately
with its HWND token and owner TID. The application receives an OUT/RET veneer
in jar RAM. Calling it validates the record, window lifetime, owner TID and
reviewed message before invoking the saved native target. Static control icon
handles are translated on input and output. Stale windows cannot call their
saved native procedures. Existing guest callbacks returned by the native DLL
are converted back to their guest entries.

This preserves the frozen PMWIN backend's existing compatibility behavior:
registered/collapsed frame-client windows keep their current native handler
when subclassed. Public controls can install the callback. R5B does not rewrite
the native backend's subclass semantics or change its DLL sources.

Nested native dialog callbacks use the existing full CPU continuation adapter:
integer/segment/flags/x87 state is saved, the owner guest thread is selected,
then the interrupted continuation is restored. WM_INITDLG receives the original
guest creation value, never the host pointer. Nested dialogs have independent
saved parameters/window records. Returned modal dialog handles and known child
handles are retired after native destruction.

WinQueryWindowProcess returns PID 1 for owned windows, matching the current jar
PIB, and the recorded guest TID. It does not substitute the Win32 thread ID for
an emulated OS/2 TID. External window queries retain the native service result.

## Queue seam and PE classification

The new QUECALLS service uses hypercall module 0x0f; saved PM procedure veneers
use private module 0x10. Existing module IDs are unchanged. Static ordinal/name
imports and DosLoadModule/DosQueryProcAddr recognize the four queue exports.
A PE QUECALLS.DLL on OS2LIBPATH is no longer sent to the LE/LX guest loader.
An unknown native PE DLL still needs an explicit bridge; guessing its ABI from
the PE signature would not supply pointer or callback semantics.

Queue handles are private 0x6e... tokens, distinct from PM handles. All output
addresses are checked before a native read can consume an element. Native
outputs are copied through host locals. Queue ownership is process-local in the
existing native implementation, so returned owner/sender IDs are translated to
the jar PID. One native host process remains one jar; children keep independent
CPU/RAM/module/bridge state. --quecalls-dll and SOFT386_QUECALLS_DLL follow the
existing child path-propagation mechanism.

The existing QUECALLS implementation stores pData as an opaque 32-bit value and
never dereferences it. R5B preserves that contract rather than adding the RAM
base or copying cbData bytes. Sarien's source sends beep frequencies/durations
in this field. ReadQueue always calls the native backend with NOWAIT. An empty
guest WAIT parks in THREAD_WAIT_QUEUE and retries at the generated import veneer
after yielding to other guest threads. Empty NOWAIT returns ERROR_QUE_EMPTY.

This milestone covers Read/Write/Open/Create only, matching the supplied native
DLL exports. Nonzero event semaphore notification is rejected with error 87;
its jar event ownership is not wired to the native backend. This is not a
cross-process shared-memory queue implementation.

## Validation and practical limits

- Full GCC make check: R2C/R3/R4/R5/R5A/R5B pass.
- Real Tiny386 PM fixture exercises all 13 create arguments, a saved native
  procedure called through a guest veneer, pointer-to-bitmap translation,
  scrolling/paint callback, nested modal dialogs, dialog controls, worker
  progress during a yielding callback, and restored FS/x87/DF/EBX state.
- Real Tiny386 queue fixture parks an empty reader, runs its producer, resumes
  with an unchanged high-bit payload value, and verifies TID/x87 preservation.
- Boundary tests reject truncated structures, wrong handle kinds, invalid code
  entries, stale/cross-thread native procedure calls, unsupported pointer
  messages, malformed queue outputs and unsupported semaphore notification.
- AddressSanitizer/UndefinedBehaviorSanitizer PM and queue boundary runs pass.
  Leak checking is disabled because of the execution environment's task-list
  restriction.
- Supplied BIO/HANOI/NEKO/NEKO.DLL pass loader checks. These are loader results,
  not confirmation of their full GUI behavior.
- i686 MinGW production build succeeds; the vessel still imports only KERNEL32
  and msvcrt. QUECALLS.dll in the kit is built from unchanged backend source.

Evidence is under validation/r5b/. There is no Wine/display-backed Windows run
here, so NEKO's cat, BIO's options and WMCHAR's repaint remain live acceptance
targets. The user's supplied WMCHAR screenshots and Sarien traces guided this
change; their exact executables were not supplied.

WinDlgBox uses the existing synchronous native modal loop. Other jar threads
can run when a callback yields, as tested; an idle native dialog loop does not
yet pump the jar scheduler/private posted queue. One PM queue owner per jar,
retained guest DLL mappings, limited typed message coverage and the backend's
existing PM behavior remain. BIO clipboard actions and HANOI's DosEnterCritSec
warning are outside this patch. No claim of complete PM compatibility is made.

## Windows use

1. Extract soft386-R5B-win32-smoke.zip into a fresh directory. RUN-R5B.cmd runs
   the new production QUECALLS fixture, then the existing R5A/R5 smoke checks.
   No Python or compiler is required. VIEW-R5.cmd retains the PM display demo.
2. For the existing installation, copy soft386\soft386_os2.exe from the kit to
   C:\OS2\soft386_os2.exe. Keep the existing native DLLs and env.cmd. The supplied
   QUECALLS.dll is for a standalone kit; the already-installed matching DLL can
   be used directly. Native DLLs use PATH or explicit --*-dll overrides.
3. Keep the LE/LX NEKO.DLL beside NEKO.EXE or on OS2LIBPATH, with that exact name.
4. Rerun from each application's directory:

```bat
\OS2\soft386_os2.exe --max-cycles 0 --trace-native --run NEKO.EXE 2>neko-r5b.trace
\OS2\soft386_os2.exe --max-cycles 0 --trace-native --run BIO.EXE 2>bio-r5b.trace
\OS2\soft386_os2.exe --max-cycles 0 --trace-native --run WMCHAR.EXE 2>wmchar-r5b.trace
\OS2\soft386_os2.exe --max-cycles 0 --trace-native --run sarienle.exe 2>sarien-r5b.trace
```

The banner should say R5B. Exercise BIO's options, close/reopen them, watch
NEKO's cat and type several characters into WMCHAR without resizing. The new
Sarien route removes the QUECALLS loader failure; further imports/runtime gaps
may still appear. Add --trace-hc --trace-sched if a callback or wait stalls.

For source users, apply SOFT386-R5B-from-R5A.patch to R5A, then build with a
32-bit MinGW/RosBE toolchain. The full source ZIP is also supplied. Do not apply
the incremental patch to R5 or the original f0c6ff2 tree.
