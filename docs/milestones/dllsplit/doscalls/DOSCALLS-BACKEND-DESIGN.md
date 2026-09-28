# DOSCALLS R2 backend design

## Goal

Apply the same separation used by VIO R2 to native Win32 DOSCALLS without disturbing the already-proven WHP/shared personality core:

```
OS/2 program
    -> historical DOSCALLS export / ordinal
    -> thin ABI veneer (dlls/doscalls/doscalls.c)
    -> backend-neutral Os2DosSession / os2_dos_* semantic entry point
    -> low-level backend operation OR typed transitional dispatch
    -> Win32 backend mechanics
```

The public ABI layer has no `windows.h`, `HANDLE`, `DWORD`, or Win32 API calls.  `DosFlatToSel` and `DosSelToFlat` are the deliberate exception to ordinary function routing: they remain register-ABI EAX token helpers in the veneer because the existing C/386/EMX bridge depends on that exact behavior.

## Relationship to the existing shared DOSCALLS core

`common/doscalls/os2_doscalls_core.c` is **not replaced**.  It remains the address-space-neutral personality core shared by native DOSCALLS and WHP for the proven eight calls:

- DosQueryHType
- DosGetDateTime
- DosSetFilePtr
- DosWrite
- DosAllocMem
- DosFreeMem
- DosSetMem
- DosQuerySysInfo

The new `common/doscalls/os2_doscalls.c` is a native process/session layer above that existing core.  This avoids forcing WHP guest-address semantics into native process bookkeeping.

## Common session ownership

`Os2DosSession` owns state whose identity is OS/2 personality state rather than Win32 state:

- logical HFILE table and maximum-HFILE policy;
- DOSCALLS-owned replacements for logical stdin/stdout/stderr;
- find-handle namespace;
- asynchronous child-process bookkeeping;
- event-sem handle namespace, generation, reference count, name and post count;
- DosSub* suballocation pools/ranges;
- exception registration chain head;
- signal-exception focus nesting count;
- DosError flags.

Native objects are stored as opaque `O2NATIVE` tokens.  No Win32 type leaks into the common headers.

## Low-level backend operations already decomposed

The Win32 backend supplies small operations for:

- process standard-handle lookup;
- native handle close;
- find handle close;
- sleep;
- native memory-range validation;
- event create/reset/post/wait/close;
- common-state locking;
- exception-head synchronization with the compatibility TIB.

This is enough for the common layer to own the complete semantics/state of event semaphores, DosSub*, sleep, max HFILE bookkeeping, exception-chain bookkeeping and several small Control Program calls.

## Transitional typed dispatch

DOSCALLS is much larger than VIO.  R2 deliberately avoids a flag-day rewrite of every filesystem/process/module/NLS function.  APIs not yet decomposed into primitive backend operations still enter a compile-time typed common entry point and then use `Os2DosBackendOps.dispatch` with an API-specific argument structure.

This seam is intentionally transitional.  It preserves the ABI and working Win32 implementation while making future decomposition local: an individual `os2_dos_DosX` can acquire real common semantics and lower-level backend callbacks without changing the public DLL, ordinal table, loader, or callers.

The typed dispatch seam must not become a generic untyped varargs interface.  Every current call has a fixed `Os2DosArgs_DosX` structure and enum ID.

## Current semantic split

Twenty-five DOS API entry points are already wholly common-state/common-semantics routes. `DosBeep` additionally performs frequency/duration policy in common code and dispatches only actual audio generation.  The remaining API-specific host work is currently in the Win32 backend.

See `DOSCALLS-R2-API-MATRIX.md` for the exact per-export route.

## Win32 backend

`common/win32/os2_doscalls_win32.c` contains all Win32-specific mechanics, including files, paths, process creation, module loading, WinMM audio, mutexes and the still-to-be-decomposed legacy implementations.

The active HFILE/find/child helpers in this file now use `Os2DosSession`; the former Win32-owned `o2_handles`, `o2_std_handles`, `o2_find_handles`, and `o2_children` tables are gone.

## Locking

The Win32 backend provides one recursive `CRITICAL_SECTION` to the common session through `state_lock/state_unlock`.  Host-neutral tests use no-op lock callbacks.  This keeps locking policy out of common DOSCALLS while allowing process-owned state to be synchronized.

## Teardown

`os2_dos_session_destroy` closes common-owned event, find, HFILE/std-handle and child native tokens through backend callbacks.  On normal DLL unload the Win32 `DllMain` destroys the session before deleting the R2 state lock.  During process termination (`reserved != NULL`), it leaves cleanup to Windows as the pre-R2 implementation did for loader-lock safety.

## Deliberate non-goals for R2

- no change to DOSCALLS export ordinals or signatures;
- no loader/far16 descriptor changes;
- no WHP protocol changes;
- no new DOSCALLS APIs;
- no filesystem namespace redesign;
- no new OS/2 thread scheduler;
- no replacement of the existing NLS/shared personality core;
- no claim that all 81 normal APIs have already had every OS/2 rule extracted from Win32 code.

The next DOSCALLS refactor can remove typed-dispatch cases domain-by-domain (filesystem, process/thread, mutex/sync, module/NLS) without another ABI restructuring.
