# Soft386 OS/2 R1 handoff

## Objective

Keep OS/2 kernel/process state in the Tiny386 vessel while reusing the existing
native 32-bit `DOSCALLS.dll` for host-facing Win32 services through explicit
copy-in/copy-out marshalling.

## What R1 adds over live-proven R0/R0A

1. Dynamic Win32 DOSCALLS bridge loaded by ordinal.
2. Explicit guest-pointer marshalling; no guest address is passed as a host
   pointer.
3. Native HFILE/HDIR ownership continuity across open/read/write/seek/close and
   related APIs.
4. Jar-owned event semaphore implementation (324-330).
5. Jar-owned mutex semaphore implementation (331-336), including recursion,
   wait queues and abandoned-owner handoff.
6. Jar-owned `DosQueryMem` plus tracked OS/2-visible allocation/protection state.
7. Regression guests proving virtual memory and sync behavior.
8. Fake-provider marshalling test independent of Windows.
9. Windows-only native file bridge LE fixture.
10. Root `soft386-native-win32` convenience target to build both sides.

## Proven in this build environment

GCC/Linux:

* LE and LX loading
* internal OFF32 + external ordinal REL32 fixups
* R0 historical hi import-surface fixture
* saved-context thread switch / per-thread FS restoration
* event/mutex block-wake sequencing
* guest memory allocate/set/query/free semantics
* fake native DOSCALLS pointer marshalling

Clang/Linux is also part of the release verification.

The actual Win32 `LoadLibrary`/ordinal calls cannot execute on this Linux host;
that last step is deliberately left for the RosBE/Windows live test described
in `WINDOWS-TEST-R1.md`.

## Important ownership rule

Do not solve a missing jar-kernel API by simply adding its ordinal to
`soft386_doscalls_bridge_ordinal()`.  First decide who owns the OS/2-visible
object.  If an API exposes guest addresses, TIDs, sem handles, module handles,
selectors, exception registrations, or scheduler-visible waits, its semantics
belong in Soft386/common, even if native os2host32 implements the same ordinal
inside `DOSCALLS.dll`.
