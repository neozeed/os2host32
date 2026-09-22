# WHP OS/2 V2 loader — Milestone 3 shared personality

This directory contains the Windows Hypervisor Platform execution backend for
OS2HOST32.  It runs untouched flat 32-bit OS/2 LE/LX guest code in a Win64 host
and crosses into the host personality through synthetic ordinal veneers.

WHP keeps its own loader milestones, guest tests, validation captures, and
experiments under this directory.  Ordinary system-call semantics which are
shared with the native loader now live in the repository-level `../common/`
tree rather than being copied into WHP.

## Build

Use an **x64 Visual Studio Developer Command Prompt** and run:

    make

The WHP Makefile intentionally has one production output only:

    whp_os2_v2_hi.exe

It compiles the active WHP source plus:

    ../common/doscalls/os2_doscalls_core.c
    ../common/api/os2_api_catalog.c
    ../common/win32/os2_win32_services.c

The native 32-bit compatibility DLL binaries are not loaded into the Win64 WHP
process.  Both products compile the same common source with different adapters.

From the repository root, the same build can be entered with:

    make whp

The normal root `make all` does not build WHP implicitly because the native
runtime uses 32-bit MinGW while WHP requires the Win64 Microsoft toolchain and
`WinHvPlatform.lib`.

## Shared and intrinsic calls

The first common DOSCALLS set is ordinals 224, 230, 256, 282, 299, 304, 305,
and 348.  WHP supplies guest-memory, guest-handle, and guest-allocation adapter
operations to those implementations.

Thread/process exit, guest scheduling, process creation, event/mutex waits,
callbacks, sleep, and blocking beep behaviour remain WHP intrinsics.  They must
save or resume virtual CPU state and therefore cannot be replaced by a native
DLL call.

WHP import output now uses the canonical ordinal catalogue and prints API names
plus routes such as `[shared]`, `[backend]`, or `[intrinsic]`.

## Directory layout

- `src/` — active WHP loader/runtime and WHP-private implementation headers.
- `docs/current/` — current Milestone 3 handoff and validation notes.
- `docs/milestones/` — superseded WHP packages and transition patches.
- `docs/milestones/m2-package/` — the former current Milestone 2 package,
  preserved byte-for-byte.
- `docs/milestones/revisions/` — R3 through R12 development history.
- `tests/guest/` — OS/2 guest programs used to exercise WHP facilities.
- `tests/host/` — host-side/mock regression checks retained from WHP work.
- `scripts/` — historical build/run command files and reproduction helpers.
- `fixtures/` — stable guest executables supplied with milestones.
- `validation/` — captured Windows results.
- `archive/` — superseded WHP-specific artifacts retained outside active work.

See [`DIRECTORY_LAYOUT.md`](DIRECTORY_LAYOUT.md) for subtree rules and
[`docs/current/README-FIRST.md`](docs/current/README-FIRST.md) for the current
handoff.
