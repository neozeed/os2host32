# M29J2 - normalize DosExecPgm child environment at the Win32 boundary

M29J1 proved that the reconstructed Microsoft C/386 CMD shell itself runs once
its historical LINK386 stack reservation is made explicit.  Interactive `CD`
and `DIR` work, but starting a nested LX program from that direct C/386 shell
returned `DosExecPgm rc=87` before the nested OS2HOST32 process appeared.

The direct C/386 backend deliberately owns a private OS/2 environment block so
`SET`, `SETLOCAL`/`ENDLOCAL`, and child inheritance do not depend on the host
CRT.  DOSCALLS.283 eventually feeds that block to Win32 `CreateProcessA` when
it creates the nested OS2HOST32 loader.  Windows has stricter requirements for
a caller-supplied environment block than the OS/2-facing side needs: entries
must be valid NAME=VALUE strings (with the special =X:=path drive entries
allowed), the block must be double-NUL terminated, and it should be sorted
case-insensitively.

M29J2 therefore normalizes a private copy inside DOSCALLS immediately before
process creation.  It does **not** mutate CMD's environment and it does not
change the loader, shell parser, far16 bridge, or seven-thunk console path.

An opt-in execution trace is also added:

    set OS2_TRACE_EXEC=1

It reports the nested command line, normalized environment size, selected
standard HANDLE types, and the CreateProcessA error code if process creation
still fails.

## Build/test

Rebuild the Win32 personality first because `doscalls.c` changed:

    make
    build-os2-shell.cmd

Then run the shell:

    set OS2_TRACE_EXEC=1
    os2host32 --run cmd32os2_os2.exe

From the shell, try a known LX child.  For example:

    cd ..
    cd sar
    sarienlx

If child creation succeeds, clear the trace afterward with:

    set OS2_TRACE_EXEC=

For an additional process-layer regression, `build-os2-backend.cmd` still
builds the older `cmdos2_os2_smoke.exe`, whose environment/async/handle tests
exercise DosExecPgm directly.
