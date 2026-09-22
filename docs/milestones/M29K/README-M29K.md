# M29K - child lifecycle, ERRORLEVEL, and DETACH

M29J2 is now a solid shell baseline: the reconstructed Microsoft C/386 command
processor can run interactively, execute built-ins, redirect, enumerate files,
and start nested LE/LX children through DOSCALLS.283 and OS2HOST32.

M29K tightens the process semantics around that working shell.

## What changes

### Child result -> shell ERRORLEVEL

Synchronous external execution already returns `RESULTCODES.codeResult` from
`DosExecPgm`.  M29K exposes the shell's `last_rc` through the command parser as
`%ERRORLEVEL%`, so a child result is immediately visible to later commands.

The older batch spelling is also retained:

    IF ERRORLEVEL 7 command
    IF NOT ERRORLEVEL 8 command

`IF ERRORLEVEL n` means "previous result is greater than or equal to n".

### Explicit wait regression

`cmdos2_os2_wait_test.exe` starts `m29k-wait-child.exe` with
`EXEC_ASYNCRESULT`, immediately issues `DosWaitChild(... NOWAIT ...)`, expects
`ERROR_CHILD_NOT_COMPLETE`, then performs a blocking wait and verifies the
child result code is 23.

Expected tail:

    async pid=...
    nowait: child still running
    waited pid=... result=23
    M29K_CHILD_WAIT_OK

This freezes the child-handle bookkeeping independently of pipeline behavior.

### DETACH

The reconstructed `DETACH` built-in is now active.  It resolves the program in
the same way as ordinary external execution but calls:

    DosExecPgm(..., EXEC_BACKGROUND, ...)

`EXEC_BACKGROUND` is OS/2 DosExecPgm flag 4: asynchronous and detached from the
parent session.  At the Win32 personality boundary the nested OS2HOST32 process
is created with `DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP`; its process
handle is deliberately not retained for `DosWaitChild`, matching the
"result discarded / orphaned background process" model.

M29K intentionally does not pretend that `START` is the same operation.  The
recovered OS/2 2.0 CMD import map contains `SESMGR.17 DOSSTARTSESSION`, twice.
A faithful START implementation therefore belongs in a SESMGR/DosStartSession
milestone rather than being approximated with `EXEC_ASYNC`.

## Build

`doscalls.c` changed, so rebuild the Win32 personality DLLs/host first:

    make
    build-os2-shell.cmd

The shell build also creates:

    rc-child.exe
    m29k-wait-child.exe
    m29k-detach-child.exe
    cmdos2_os2_wait_test.exe

## Tests

First freeze wait/result behavior:

    os2host32 --run cmdos2_os2_wait_test.exe

Then start the shell:

    os2host32 --run cmd32os2_os2.exe

Inside it, test result propagation manually:

    rc-child 37
    echo %ERRORLEVEL%

Expected output is `37`.

For the scripted test:

    examples\m29k-errorlevel-test.cmd

Expected markers include:

    ERRORLEVEL_AFTER_37=37
    IF_ERRORLEVEL_GE_37_OK
    IF_ERRORLEVEL_LT_38_OK
    ERRORLEVEL_AFTER_0=0

For DETACH:

    examples\m29k-detach-test.cmd

That script launches a child which waits about 1.2 seconds and writes
`m29k-detach.ok`.  The parent then runs a separate synchronous sleeper before
reading the marker.  Expected final marker:

    M29K_DETACH_CHILD_OK

For launch diagnostics, M29J2's trace remains available:

    set OS2_TRACE_EXEC=1

An EXEC_BACKGROUND launch should show `flag=4` and detached Win32 creation
flags in the DOSCALLS trace.

## Unchanged

The M29H/M29I seven far16 VIO/KBD migration thunks, C/386 backend ABI, LE/LX
loader/fixups, 60 KB shell stack reservation, and M29J2 environment
normalization are unchanged.
