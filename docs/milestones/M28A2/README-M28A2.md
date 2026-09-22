# Milestone 28A2 - real OS/2 header/import-library backend

This is a correction/extension to M28A's direct OS/2 backend.

## What was wrong with the first scaffold

`cmdos2_os2.c` manually declared the `Dos*` entry points.  That was useful for a
host-side syntax check, but it is the wrong boundary for the real C/386/LX
build: OS/2 owns those ABI declarations.

M28A2 now defines the required `INCL_DOS*` groups and includes `<os2.h>`.
`RESULTCODES`, `FILESTATUS3`, `FILEFINDBUF3`, `HFILE`, `HDIR`, `APIRET`, the
`Dos*` prototypes, and their API calling conventions therefore come from the
SDK rather than local guesses.

It also corrects `DosOpen` to the real 8-argument 32-bit OS/2 interface.

## Why `/Gd` matters

The failing test build showed CRT references such as:

    _strcpy@8
    _memcpy@12
    _strlen@4

while the selected `LIBC.LIB` exports the C runtime using its cdecl names.
Compile the reconstructed CMD sources with `/Gd` so ordinary C functions use
cdecl.  The `Dos*` declarations in `os2.h` retain their OS/2 API convention.

## Why `_main` was unresolved

`cmdos2_os2.c` and `cmdfile.c` are modules; neither is supposed to contain
`main()`.  Linking those two objects by themselves therefore must fail with an
unresolved `_main` from CRT startup.

M28A2 adds `cmdos2_os2_smoke.c` solely to give the backend a small executable
entry point before the complete CMD has been made OS/2-clean.

## Required libraries

Link the C/386 runtime plus the 32-bit OS/2 import library:

    LIBC.LIB
    OS2386.LIB

`build-os2-backend.cmd` looks first in `C:\20ddk\lib`, then `C:\c386\lib`, or
uses the directory in `OS2LIB`.

## Test

    build-os2-backend.cmd
    os2host32.exe --run cmdos2_os2_smoke.exe

The smoke program exercises the real OS/2 API imports for current directory,
FindFirst/FindClose, open/write/close, move and delete.

Expected final line:

    M28A2_OS2_BACKEND_OK

The native M28A CMD build is otherwise unchanged; its existing regression
suite remains the baseline while more Win32-dependent CMD modules move behind
this boundary.
