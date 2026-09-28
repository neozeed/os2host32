# OS2HOST32 — Milestone 31 with shared native/WHP personality core

OS2HOST32 provides two execution backends for 32-bit OS/2 LE/LX programs:

- `loader/` executes transformed programs as native 32-bit Win32 processes.
- `whp/` executes untouched 32-bit guest code under Windows Hypervisor Platform
  from a 64-bit host process.

The two loaders no longer maintain completely independent copies of ordinary
OS/2 system-call behaviour.  Shared API semantics live under `common/` and are
compiled into both the native compatibility DLLs and the WHP executable.
Loader-sensitive operations—guest scheduling, process creation, thread exit,
semaphore waits, callbacks, and address-space management—remain explicit
backend operations or loader intrinsics.

## Active source layout

- `common/` — loader-neutral OS/2 API semantics, the canonical ordinal catalogue,
  and small Win32 services used by both execution paths.
- `loader/` — native Win32 OS2HOST32 loader/runtime.
- `whp/` — alternative Win64/WHP loader, with its own tests, milestones, and
  validation history.
- `transformer/` — LE-to-PE transformer (`le2pe386`).
- `shell/` — CMD32 shell/personality source.
- `dlls/` — native compatibility DLL veneers and subsystem-specific code.
- `tests/` — common-core tests, milestone regressions, and retained legacy probes.

See [`DIRECTORY_LAYOUT.md`](DIRECTORY_LAYOUT.md) for the full directory policy.

## Shared system-call layer

The canonical API inventory is:

    common/api/os2_api_catalog.inc

Each entry records the OS/2 module, ordinal, API name, known argument-byte count,
and implementation route.  It currently catalogues 282 entries and covers every
ordinal exported by the native compatibility DLL DEF files.

The first shared DOSCALLS set is:

- `DOSCALLS.224` — `DosQueryHType`
- `DOSCALLS.230` — `DosGetDateTime`
- `DOSCALLS.256` — `DosSetFilePtr`
- `DOSCALLS.282` — `DosWrite`
- `DOSCALLS.299` — `DosAllocMem`
- `DOSCALLS.304` — `DosFreeMem`
- `DOSCALLS.305` — `DosSetMem`
- `DOSCALLS.348` — `DosQuerySysInfo`

For native execution, the exported `DOSCALLS.DLL` functions are thin veneers over
these common implementations.  For WHP, the same implementations receive a
backend context that validates and translates 32-bit guest addresses before
accessing guest RAM.  This avoids treating an untrusted guest pointer as a Win64
host pointer.

`QUECALLS.DLL` is also split at the personality/backend boundary.  The native
four-export ABI (`DosReadQueue`, `DosWriteQueue`, `DosOpenQueue`, and
`DosCreateQueue`) is a thin veneer over `common/queue/os2_queue.c`.  The common
queue session owns names, handles, owner PIDs, FIFO/LIFO/priority ordering and
opaque 32-bit payload values; `common/win32/os2_queue_win32.c` supplies only
locking, current-process identity and blocking/wakeup mechanics.  The common
nonblocking `os2_queue_try_read` primitive is intended for schedulers such as
WHP or an external OS2SS backend.

`KBDCALLS.DLL` follows the same split: the six native exports are thin veneers over
`common/kbd/os2_kbd.c`, while Win32 console/redirected-input mechanics live in
`common/win32/os2_kbd_win32.c`.

`SESMGR.DLL` now follows the same personality/backend boundary.  The common
Session Manager owns STARTDATA policy, session-id allocation, the shared registry
format, related-session ownership, stale-record reaping, title/query rules and
stop semantics.  The Win32 backend supplies process identity/liveness, mapped
registry storage/locking, CreateProcess/job/console mechanics, termination and
host title updates.  A WHP or OS2SS implementation can therefore replace the
backend without inheriting the Win32 named-mapping/process-handle model.

NLS R2 makes National Language Support state explicitly process-owned as well.
`Os2DosSession` embeds the authoritative `Os2NlsState`; DOSCALLS code-page,
country, DBCS and case-map APIs terminate in common NLS semantics rather than
the Win32 dispatch seam.  `common/win32/os2_nls_win32.c` supplies only an
initial host country/OEM-codepage hint, which the common layer validates against
its implemented OS/2 tables.  WHP uses the same explicit bootstrap seam and a
future OS2SS process can own the same common state without Win32 locale globals.

`MOUCALLS.DLL` is introduced directly with the same split.  `common/mou/os2_mou.c`
owns HMOU lifetime, event masks/queueing, pointer/device state, scale/threshold
and logical pointer shape.  The Win32 backend observes the GUI pointer/buttons
without consuming the console `INPUT_RECORD` stream, so it cannot steal input
from KBDCALLS.  Future WHP/OS2SS backends can feed the same common mouse state
through `os2_mou_try_read`.

The transformer and WHP import diagnostics also use the canonical catalogue, so
ordinal-only imports are reported by API name and implementation route.

Detailed architecture notes are in
[`docs/current/SHARED-PERSONALITY-ARCHITECTURE.md`](docs/current/SHARED-PERSONALITY-ARCHITECTURE.md).

## Build

The native build requires 32-bit MinGW:

    make clean
    make

This builds the native loader, transformer, CMD32, and compatibility DLLs.  The
runtime products remain together in the repository root, matching the existing
loader/DLL discovery model.

The WHP loader requires an **x64 Visual Studio Developer Command Prompt**:

    make whp

or:

    cd whp
    make

The WHP Makefile intentionally produces only:

    whp_os2_v2_hi.exe

It links the shared personality sources directly into that executable; it does
not try to load the 32-bit native compatibility DLL binaries into a Win64 process.

## Toolchain-independent verification

The common layer can be checked on a non-Windows development host:

    make verify

That runs:

- the C89 common-core behavioural test;
- catalogue/DEF coverage and duplicate checks;
- structural wiring checks proving that every API marked `shared` is called by
  both native `DOSCALLS.DLL` and WHP.

A dry run of either production build is also useful when reviewing dependencies:

    make -n all
    make -n whp

Actual native and WHP binaries still need to be built and regression-tested on
Windows with their respective toolchains.

## Documentation and retained history

- `docs/current/` — current architecture and handoff notes.
- `docs/milestones/` — completed native-loader milestone material.
- `docs/reference/microsoft-programmers-library/` — preserved Microsoft
  Programmer's Library OS/2 references.
- `whp/docs/current/` — current WHP milestone notes.
- `whp/docs/milestones/` — superseded WHP packages, patches, and revision history.
- `analysis/`, `examples/`, `fixtures/`, `regression-evidence/`, and `tools/` —
  retained project support material.

The repository cleanup history remains intact, but this revision is no longer a
layout-only change: it deliberately introduces the first shared native/WHP OS/2
personality implementation.
