# os2host32 — Milestone 18 direct-LX host

Milestone 18 is the direct-loader fork from `le2pe386` Milestone 17.

Unlike `le2pe386`, **os2host32 does not create a PE copy of the guest**. It
opens the original OS/2 LX executable and constructs that image directly in a
32-bit Win32 process.

The source remains deliberately ANSI-C89-oriented. The intended compiler floor
is Microsoft C/C++ 8.x / Visual C++ 1.x, matching the long-term goal of keeping
the compatibility work buildable with an early Win32 toolchain.

## Gate 1 — parse/map

Implemented and tested with the C/386 + OS/2 2.x `hi.exe` specimen:

- MZ -> LX discovery
- little-endian i386 / OS/2 validation
- LX object table parsing
- LX page-map parsing
- import-module-name parsing
- entry and stack discovery
- preferred-address allocation of LX objects
- normal and zero-filled LX pages
- Win32 page protection derived from LX object flags

Commands:

    os2host32 --info hi.exe
    os2host32 --map hi.exe

## Gate 2 — live fixups/imports

The direct host supports the narrow fixup subset already proven by
`le2pe386` M17:

- internal `OFF32` fixups
- external ordinal `REL32` fixups
- source-list fixup records
- 8/16/32-bit ordinal encodings
- direct loading of compatibility modules (`DOSCALLS` -> `DOSCALLS.dll`)
- ordinal resolution with `GetProcAddress`
- direct patching of the mapped LX image
- instruction-cache flush and final object protections

Safety-gate command:

    os2host32 --fixups hi.exe

For the current `hi.exe`, the scanner finds:

    70 internal fixup records (159 sites)
    12 external call sites
    8 unique DOSCALLS imports

The imports are:

    DOSCALLS.224
    DOSCALLS.234
    DOSCALLS.256
    DOSCALLS.282
    DOSCALLS.299
    DOSCALLS.304
    DOSCALLS.305
    DOSCALLS.348

All eight are already exported by the Milestone 17 `DOSCALLS.dll`.

## Gate 3 — first original-LX execution

The first real transfer reached the original Microsoft C/386 startup code and
faulted reproducibly at `0001026C`.  The register dump made the cause clear:

    EAX = argument pointer
    ESI = argument pointer - 1

The CRT was walking backwards from the OS/2 argument area to recover the
fully-qualified program-name string that OS/2 places immediately before it.
The first M18 attempt had allocated the argument text in an isolated Win32
page, so that backward walk crossed the allocation boundary.  This was a
startup-layout bug in the host, not a failure to execute LX code.

This revision builds an OS/2-style contiguous startup area instead:

    NAME=VALUE\0
    NAME=VALUE\0
    ...
    \0
    C:\\full\\path\\hi.exe\0      <- program-name (po) area
    hi.exe\0argument tail\0\0         <- argument (ao) area

`[ESP+0Ch]` points to the first environment string and `[ESP+10h]` points to
the first OS/2 argument string (`hi.exe`).  The program-name area immediately
precedes the argument area exactly so the C/386 CRT's backward scan can recover
it.

The original LX entry stack remains:

    ESP = 00022810

    [ESP+00]  00000000
    [ESP+04]  00000000
    [ESP+08]  00000000
    [ESP+0C]  environment/startup-area pointer
    [ESP+10]  OS/2 argument-area pointer

Run with:

    os2host32 --run hi.exe

Optional guest arguments can follow the filename:

    os2host32 --run hi.exe one two

Before transfer the host now prints all four useful addresses:

    OS/2 startup : ESP=........ ENV=........ PGM=........ ARG=........

The exception reporter also identifies the access-violation operation and
target address in addition to EIP/ESP and the general registers.

`DOSCALLS.234` is `DosExit`; the existing compatibility DLL maps it to
`ExitProcess`, so a normal C/386 program should terminate the host with the
guest exit code.


## Gate 3 status — SUCCESS

The corrected startup layout has now been exercised successfully on Windows.
An original, unmodified LX image built with the recovered Microsoft C/386-era
compiler and the OS/2 2.x LINK386 runs directly under `os2host32`:

    C:\2>os2host32.exe --run hi.exe
    ...
    Transfer         : original LX entry 00010020
    ---------------- guest begins ----------------
    hi

A second independently compiled LX argument test also succeeds:

    C:\2>os2host32.exe --run args.exe one two three
    ...
    hello from OS/2
    argc c is 4
    argv 0 is [args.exe]
    argv 1 is [one]
    argv 2 is [two]
    argv 3 is [three]

This confirms, for this C/386 runtime path, all of the following together:

- original LX object/page mapping
- live internal relocations
- external ordinal fixups into the compatibility DLL
- transfer to the original LX entry point
- original LX stack use
- Microsoft C/386 CRT startup
- OS/2-style environment/program/argument startup layout
- `argc` / `argv` reconstruction
- guest termination through the compatibility layer

There is still intentionally no claim of general LX compatibility: the loader
currently implements the subset needed by these test images.  This is now a
working execution baseline rather than only a loader experiment.

## Why `hi.exe` is the first target

It is deliberately tiny, has only two LX objects and imports only DOSCALLS.
We have its source, compiler lineage, OBJ lineage and final LX. That makes it a
much better execution bring-up target than `CMD.EXE` or `PMEXEC.EXE`.

Once this original binary runs, the next progression is intended to be:

1. argv/environment regression tests
2. file and directory tests
3. memory tests
4. threads/queues as needed
5. `DosExecPgm` and child-host creation
6. OS/2 2.0 GA LX `CMD.EXE`
7. larger PM/LX targets

## Build

The normal Makefile builds both host tools and the complete compatibility DLL set as part of `all`:

    make

The explicit groups are also available as `make tools`, `make compat`, or `make dlls`.

A plain `make` produces:

    le2pe386.exe
    os2host32.exe
    DOSCALLS.dll
    QUECALLS.dll
    PMWIN.dll
    PMGPI.dll

For a native host-only parser build on Unix-like systems:

    make os2host32-host

For the old Microsoft compiler experiment, `build-msvc.cmd` contains:

    cl /O2 /W3 os2host32.c /link /out:os2host32.exe
