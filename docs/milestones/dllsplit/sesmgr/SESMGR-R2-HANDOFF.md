# OS2HOST32 SESMGR R2 — common Session Manager semantics + Win32 backend

## Status

`SESMGR_R2_COMMON_STATE_WIN32_BACKEND_STATIC_VERIFIED_RUNTIME_PENDING`

This milestone starts from the user's live-proven `os2host32-KBDCALLS-R2` baseline.  KBDCALLS, QUECALLS, DOSCALLS and VIO behavior is otherwise unchanged.

## Objective

Split the existing native `SESMGR.dll` using the same architecture already established for VIO, DOSCALLS, QUECALLS and KBDCALLS so that WHP and ReactOS OS2SS can later provide their own session-execution backends.

This is a refactor, not an API expansion.

## Frozen ABI

`dlls/sesmgr/sesmgr.def` is byte-identical to the baseline:

- `DOSSMSETTITLE=DosSmSetTitle @5`
- `DosStopSession @8`
- `DosStartSession @17`
- `O2HostQuerySessions @1000` (private CMD32 extension)

SHA-256: `a61b200f3b1e6ff595d6219b31779129bf9c88dfae3b01ea5400ec78918ee913`

`loader/os2host32.c` is also byte-identical, SHA-256:

`793ee7de7c6d0bd79b7d8bbdfaab6774cb2ec6af39b88d0cefc3c714430f0c55`

No loader/far16 descriptors or import behavior changed.

## New architecture

### `dlls/sesmgr/sesmgr.c`

Thin native ABI veneer only.  It owns the packed historical STARTDATA wire layout and its accepted prefix lengths, normalizes that wire structure, and calls the common SESMGR entrypoints.

It contains no CreateProcess, process-handle, Job Object, mapping, mutex or liveness logic.

### `common/sesmgr/os2_sesmgr.c`

Owns OS/2 Session Manager semantics/state:

- STARTDATA validation and default policy;
- session-id allocation;
- common shared registry format;
- stale-record reaping policy;
- process/owner identity matching;
- related-session ownership table;
- title ownership/update rules;
- owner session query semantics;
- DosStopSession selection/lifecycle semantics;
- related-child remember/resume ordering.

### `common/win32/os2_sesmgr_win32.c`

Owns Win32 mechanics only:

- critical section locking;
- process identity via PID + creation time;
- process liveness queries;
- named shared mapping/mutex storage;
- `OS2HOST32_LOADER` lookup;
- command-line construction;
- Win32 environment-block normalization;
- STARTUPINFO mapping;
- `CreateProcessA`;
- new-console vs detached-PM behavior;
- Job Objects;
- resume/terminate/wait/handle cleanup;
- SetConsoleTitleA;
- existing session tracing.

## Shared registry compatibility

The Win32 backend preserves the existing mapping and mutex names:

- `Local\\OS2HOST32_SESMGR_R1`
- `Local\\OS2HOST32_SESMGR_R1_MUTEX`

The common registry structure deliberately preserves the old mapped byte layout (header plus 128 records).  This avoids changing the native cross-process contract merely because ownership moved into common code.

## Why this split matters

WHP should not inherit Win32 process handles/Job Objects as the definition of an OS/2 session.  ReactOS OS2SS should not need a named Win32 mapping to implement session ownership.

Both can implement `Os2SesmgrBackendOps` using their own execution engine while reusing the same STARTDATA, session-id, ownership, title/query and stop semantics.

## Verification

Host-portable verification added:

1. `sesmgr-core-check`
   - common session-id allocation;
   - related-session registration;
   - owned query;
   - parent and current-session title policy;
   - StopSession removal;
   - independent-session behavior;
   - STARTDATA policy rejection paths.

2. `sesmgr-veneer-check`
   - actual `dlls/sesmgr/sesmgr.c` linked against a fake backend;
   - 24-byte STARTDATA prefix defaults;
   - 60-byte STARTDATA normalization;
   - ObjectBuffer clearing;
   - start/stop/query routing through common SESMGR.

3. `sesmgr-win32-shim-check`
   - actual common core + actual Win32 backend against a deterministic Win32 stub;
   - related child -> process group + suspended + new console;
   - STARTUPINFO minimize/position/size mapping;
   - custom environment sorting;
   - mapped-registry path;
   - related StopSession;
   - PM independent -> detached/no new console/no suspend.

4. `check_sesmgr_r2.py`
   - frozen DEF and loader hashes;
   - no Win32 mechanics in common core/headers;
   - thin veneer enforcement;
   - common registry/policy ownership;
   - Win32 implementation placement;
   - packed STARTDATA guard;
   - build/test integration.

Complete `make verify` passes with GCC and Clang host compilers.  Applying the R2 patch to a fresh copy of the exact KBDCALLS-R2 baseline and rerunning `make verify` also passes.

## Runtime status

The environment used to build this package does not contain `i686-w64-mingw32-gcc`, so no real MinGW DLL or Windows runtime claim is made.

Recommended native test:

```text
make clean
make SESMGR.dll
make all
make verify
```

Then test CMD32 session behavior that exercises the existing implementation:

- `START` a related console child;
- `START` a PM program if available;
- `PS` sees the related session;
- child/title update still appears;
- `KILL` / DosStopSession stops a selected child;
- multiple STARTs allocate distinct session IDs;
- existing CMD/VIO/KBD/QUEUE demos remain responsive.

If those pass, promote the milestone to:

`SESMGR_R2_COMMON_STATE_WIN32_BACKEND_LIVE_PASS`

## Out of scope / unchanged

No new SESMGR exports were added.  Termination queues, PgmHandle install-database semantics, cooperative close, full-screen VIO fidelity, icon binding and richer inheritance behavior remain future work.
