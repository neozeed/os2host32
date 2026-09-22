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
and implementation route.  It currently catalogues 247 entries and covers every
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
