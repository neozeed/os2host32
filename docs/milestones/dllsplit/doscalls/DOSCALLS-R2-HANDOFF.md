# OS2HOST32 DOSCALLS R2 handoff

## Status

`DOSCALLS_R2_COMMON_SESSION_WIN32_BACKEND_STATIC_VERIFIED_RUNTIME_PENDING`

This branch starts from the live-working VIO R2 package supplied by the user:

- `os2host32-VIO-R2.zip`
- SHA-256 `2cc60643010d088d6edb48805f8428bc6236bef88ecd56c37c85acefb8888551`

The DOSCALLS export table is frozen unchanged:

- `dlls/doscalls/doscalls.def`
- SHA-256 `2a1d6d7aa4f9b1369f9d949ebd448ea76746f42bf23a460763c097fee6f2d13b`
- 86 exports: 81 normal DOS APIs, 2 register-ABI selector helpers, 3 named NLS helpers.

## What changed

The previous ~4,744-line `dlls/doscalls/doscalls.c` mixed OS/2 personality state, exported ABI, Win32 filesystem/process/synchronization APIs, WinMM audio, compatibility TIB/PIB state, suballocation and shared personality calls.

R2 splits it into:

- `dlls/doscalls/doscalls.c` — ~529-line thin exported ABI veneer;
- `common/include/os2_doscalls.h` — backend-neutral types/session/API entry points;
- `common/include/os2_doscalls_backend.h` — low-level backend operations and typed transitional call records;
- `common/doscalls/os2_doscalls.c` — common process/session semantics and state;
- `common/include/os2_doscalls_win32.h` — native backend/session boundary;
- `common/win32/os2_doscalls_win32.c` — Win32 implementation/backend.

The proven `common/doscalls/os2_doscalls_core.c` remains in place and continues to be shared with WHP.

## State moved out of Win32

The active logical HFILE/std-handle/find/child tables now live in `Os2DosSession`.  The Win32 backend resolves and allocates those namespaces using common helpers rather than private static arrays.

Event semaphore state is now common: generation-encoded HEV values, references, post counts and process-local names.  Win32 provides only native event create/reset/post/wait/close operations.

The DosSub* allocator, exception chain, signal-focus nesting, DosError state and max-HFILE policy are also common.

## Public ABI preservation

`doscalls.def` is byte-for-byte unchanged from the VIO-R2 baseline.  A generated signature comparison found all 84 ordinary C-function wrappers (normal DOS APIs plus named NLS helpers, excluding the two register-only selector helpers) present with zero signature mismatches.

`DosFlatToSel` and `DosSelToFlat` intentionally remain naked/no-op register-token helpers in the public veneer, preserving the existing C/386/EMX bridge model.

`Dos16Sleep @32` remains an alias at the semantic layer to the same common `DosSleep` implementation.  No far16 loader descriptor or ordinal changes were made.

## Shared native/WHP personality preservation

The existing eight shared DOSCALLS implementations still route through `os2_doscalls_core` in the Win32 backend, and WHP continues to use the same core directly.  `tools/check_shared_personality.py` was updated only to follow the new native layering when verifying this fact.

## Tests added

`tests/doscalls/doscalls-core-check.c` covers:

- logical standard handles and owned std-handle replacements;
- HFILE allocation/replacement/free and max-HFILE floor behavior;
- find-handle allocation/free;
- child process store/take bookkeeping;
- DosSleep/Dos16Sleep;
- common DosBeep validation/zero-frequency sleep behavior;
- event-sem create/open/refcount/post/reset/query/close;
- DosSub* initialization, 8-byte allocation rounding, free/coalesce/unset;
- DosError/ExitList;
- exception-chain link/unlink and backend head synchronization;
- signal-focus nesting;
- typed fallback dispatch;
- common-session teardown of owned native tokens.

`tests/doscalls/doscalls-veneer-check.c` links the actual exported `dlls/doscalls/doscalls.c` to common DOSCALLS with a fake backend and proves representative public routing, including DosSleep, Dos16Sleep, a dispatched API, named NLS helper routing, and DosSetRelMaxFH.

`tools/check_doscalls_r2.py` freezes the `.def`, checks all exports remain represented, ensures no Win32 APIs leaked into the public/common layers, verifies common state ownership and confirms build wiring.

## Verification performed here

`make verify` passes, including all pre-existing personality, NLS, catalog, VIO and Life surface checks plus the new DOSCALLS core/veneer/static checks.

The common DOSCALLS test and exported-veneer test also pass with Clang 17 under strict C89 flags.

The real Win32 DOSCALLS DLL was **not built or executed in this environment** because `i686-w64-mingw32-gcc` is unavailable.  Therefore this handoff does not claim a live runtime pass for the new split.

See `STATIC-VERIFICATION-DOSCALLS-R2.txt` for the captured verification output.

## Recommended Windows validation

From the package root on the normal MinGW build host:

```text
make clean
make DOSCALLS.dll
make all
make verify
```

Then rerun the known native regressions before adding new functionality.  At minimum:

1. CMD32/basic DOSCALLS startup;
2. the C/386/EMX far16 bridge regressions;
3. Life/VIO (to prove Dos16Sleep remains good);
4. hi/hi2 and current file I/O specimens;
5. an EMX specimen that exercises exception/signal/max-HFILE setup if available;
6. process launch/wait paths;
7. any current program using event semaphores or DosSub*.

A successful run should promote the milestone to `DOSCALLS_R2_COMMON_SESSION_WIN32_BACKEND_LIVE_PASS` without further architecture changes.

## Important limitation / next decomposition

R2 establishes the architectural boundary without performing a risky flag-day semantic rewrite of all DOSCALLS.  Twenty-five API entry points are already fully common semantics/state; `DosBeep` is hybrid; the rest currently use the compile-time typed dispatch seam for their API-specific Win32 mechanics.

The next refactor should remove that seam incrementally by domain, preferably in this order:

1. filesystem/HFILE operations;
2. mutex and remaining synchronization APIs;
3. process/thread/wait APIs;
4. module management;
5. current-directory/path/environment functions;
6. NLS facade calls where useful.

Each domain can be migrated without changing the public veneer or ordinal table.

---

## R2A erratum — first real MinGW compile

The first external `i686-w64-mingw32-gcc` build of the original R2 package
exposed a source-organization defect that the host-side tests did not compile:
backend-private TIB/PIB layout definitions had been removed, while obsolete
backend `DosSub*`/exception/signal code remained.  This tree contains the R2A
correction.  See `DOSCALLS-R2A-HANDOFF.md`.  Do not use the original R2 archive
as the current build candidate.
