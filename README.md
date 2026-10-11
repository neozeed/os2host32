# OS2HOST32

OS2HOST32 is an experimental OS/2 2.x compatibility environment for running
32-bit OS/2 LE/LX applications on modern systems.

The project currently has three execution paths:

* **Native Win32** — runs transformed OS/2 applications as 32-bit Windows
  processes using compatibility DLLs.
* **WHP** — runs untouched 32-bit OS/2 code under Windows Hypervisor Platform
  from a 64-bit host.
* **ReactOS OS2SS** — implements an external OS/2 environment subsystem for
  ReactOS and runs untouched OS/2 LE applications inside subsystem-5 process
  vessels.

The long-term goal is to share as much OS/2 personality behaviour as possible
between these backends rather than maintaining unrelated implementations.

## Current status

The project can run a growing selection of real 32-bit OS/2 programs.

Examples exercised during development include:

* Microsoft C/386 generated LE executables
* CMD-style console applications
* Life
* Sarien
* Hack
* phoon
* Infocom-style applications

The ReactOS OS2SS path can load relocated untouched LE executables, resolve
DOSCALLS imports through native gateway veneers, communicate with an external
OS2SS personality server, and provide console/file I/O sufficient for programs
such as `phoon.exe` and `hi2.exe`.

This is experimental compatibility software, not a complete OS/2
implementation.

## Repository layout

    common/          Shared OS/2 personality code and API semantics
    dlls/            Native Win32 compatibility DLL veneers
    loader/          Native Win32 LE/LX loader
    transformer/     LE-to-PE transformer
    shell/           CMD32 shell/personality
    whp/             Windows Hypervisor Platform execution backend
    subsystem/       ReactOS OS2SS environment subsystem
    tests/           Tests and regression checks
    docs/            Architecture notes and milestone history
    examples/        Example OS/2 programs

See `DIRECTORY_LAYOUT.md` for more detail.

## Building

### Native Win32

Requires a 32-bit MinGW toolchain:

    make

or:

    make clean
    make

### WHP

The WHP backend is a 64-bit Windows executable and currently uses the Visual
Studio toolchain:

    make whp

or:

    cd whp
    make

### ReactOS OS2SS

The ReactOS subsystem build uses GNU Make and:

    i686-w64-mingw32-gcc

The required frozen ReactOS headers and import-library subset are kept under:

    subsystem/reactos/

A full ReactOS source checkout is therefore not required for normal subsystem
development.

Build with:

    cd subsystem
    make

The build produces:

    build-mingw/OS2SS.EXE
    build-mingw/OS2LE4CLAUNCH.EXE
    build-mingw/OS2BOOT.EXE

`OS2BOOT.EXE` is emitted directly as PE subsystem type 5
(`IMAGE_SUBSYSTEM_OS2_CUI`).

The same build is intended to work from Windows, Linux and macOS hosts using
the i686 MinGW-w64 cross toolchain.

## Architecture

Shared OS/2-visible semantics live primarily under `common/`.

Backend-specific code handles operations that genuinely depend on the execution
environment, such as:

* address-space management
* thread/process scheduling
* console and keyboard I/O
* host file access
* synchronization
* guest-memory translation

This allows the native Win32, WHP and ReactOS implementations to converge on a
common OS/2 personality without pretending that their execution mechanisms are
identical.

The canonical OS/2 API catalogue is:

    common/api/os2_api_catalog.inc

## ReactOS OS2SS

`subsystem/` implements an experimental OS/2 environment subsystem for ReactOS.

It registers `IMAGE_SUBSYSTEM_OS2_CUI` with SMSS and maintains separate:

* SMSS subsystem callback communication
* application/personality API communication

`OS2BOOT.EXE` is a small PE subsystem-5 process vessel. It loads an OS/2 LE
image into its own address space, applies relocations and import fixups, creates
DOSCALLS veneers, constructs the expected C/386 startup state, and transfers
control to the untouched OS/2 program.

DOSCALLS requests cross back through a native gateway into OS2SS.

No modified ReactOS kernel, SMSS, CSRSS or KERNEL32 is required.

## Verification

Common code can be checked with:

    make verify

Individual backends also contain their own regression and architecture tests.

Milestone-specific build records, hashes, experiments and historical handoff
documents belong under `docs/` rather than in this README.

## Documentation

    docs/current/       Current architecture/design material
    docs/milestones/    Completed milestone records
    whp/docs/           WHP-specific documentation
    subsystem/docs/     ReactOS OS2SS documentation, if present

## Project status

OS2HOST32 is under active development.

Compatibility is incomplete and many OS/2 APIs remain partial or unimplemented,
but the project is far enough along to execute non-trivial real OS/2 software
through multiple independent backends.