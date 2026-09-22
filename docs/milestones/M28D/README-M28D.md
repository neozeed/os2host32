# Milestone 28D - OS/2 process + pipeline boundary

M28D removes the last native Win32 process/pipe implementation from CMD's
pipeline executor.  `cmd32os2.c` no longer calls `CreatePipe`, `CreateProcess`,
`WaitForSingleObject`, `GetExitCodeProcess`, or `GetModuleFileNameA` for `A | B`.

The pipeline path is now:

    CMD parse tree
        |
        +-- CmdO2CreatePipe
        |      `-- DosCreatePipe
        |
        +-- CmdO2StdRedirectHandle(1, writer)
        +-- DosExecPgm(EXEC_ASYNCRESULT) left shell worker
        +-- restore stdout / close parent writer
        |
        +-- CmdO2StdRedirectHandle(0, reader)
        +-- DosExecPgm(EXEC_ASYNCRESULT) right shell worker
        +-- restore stdin / close parent reader
        |
        `-- DosWaitChild(right), DosWaitChild(left)

Pipeline status remains the right-hand process result, matching the M25
behaviour.

## DosWaitChild

The supplied OS2386.LIB identifies the 32-bit import as DOSCALLS ordinal 280.
M28D exports that ordinal and implements the process-wait subset needed by CMD:

* `DCWA_PROCESS`
* `DCWW_WAIT`
* specific child PID

`DCWW_NOWAIT` is also accepted by the compatibility DLL.  Process-tree waits
remain outside this milestone.

## Asynchronous DosExecPgm

DOSCALLS.283 now accepts:

* `EXEC_SYNC` (0)
* `EXEC_ASYNC` (1)
* `EXEC_ASYNCRESULT` (2)

For EXEC_ASYNCRESULT the Win32 process HANDLE is retained until DosWaitChild
collects it.  As on OS/2, the child PID is returned in the first RESULTCODES
field from the asynchronous DosExecPgm call.

There is one deliberately temporary bootstrap accommodation: the current
`cmd32os2.exe` is still a native Win32 PE.  When DOSCALLS sees that exact shell
worker name it starts the PE directly.  Ordinary OS/2/LX children still run via
OS2HOST32.  Once CMD itself is C/386 LX this exception becomes unused and can
be removed.

## Standard-handle lifetime

M28D also makes the native bridge ownership explicit.  DOSCALLS owns the
Win32 standard HANDLE installed by DosDupHandle.  CRT fd 0/1/2 receives a
separate duplicate of that handle.  `CmdO2StdSave/Restore` saves both views,
which prevents the earlier M28C handle-alias/leak failures while allowing pipe
redirection to use the real OS/2 handle calls.

## Tests

First build the native host and DLLs:

    make

Because M28D tightens the native standard-handle bridge again, first re-run
the known M28C4 redirection regression before testing pipelines:

    cd examples
    ..\cmd32os2.exe
    m28c-redir-test.cmd
    exit
    cd ..

It should still end with `delete-one=OK`, `delete-two=OK`, and the M28C4
completion marker.

Then build the C/386 LX smoke programs:

    build-os2-backend.cmd

The direct LX smoke test now includes an asynchronous child returning 7:

    os2host32.exe --run cmdos2_os2_smoke.exe

Expected new line near the middle:

    async exec/wait=OK pid=<pid> result=7

and the final marker:

    M28D_OS2_BACKEND_OK

For the actual CMD pipeline regression, build the two LX filters once:

    build-pipe-fixtures.cmd

Then:

    cd examples
    ..\cmd32os2.exe
    m28d-pipe-test.cmd

The test runs:

    ..\emit-test.exe | ..\upper-test.exe > m28d-pipe.txt
    ..\emit-test.exe | ..\upper-test.exe | ..\upper-test.exe > m28d-triple.txt
    echo mixedCase | ..\upper-test.exe > m28d-builtin.txt

and should end with:

    M28D process/pipe regression complete

At this point direct Win32 process/pipe calls are gone from CMD proper.  The
remaining substantive native island in `cmd32os2.c` is console rendering
(`CLS` and stdio), which is the intended hand-off to VIOCALLS/KBDCALLS.

## Validation status

The portable parser, batch, environment, strict-C89 boundary, and pipeline
boundary checks pass in the build environment used to prepare this bundle.
That environment does not contain the i686 MinGW or recovered C/386 toolchain,
so the real Windows DLL/PE build and C/386 LX link remain tests for the Windows
machine.  In particular, `DosWaitChild` through the recovered `os2.h`/
`OS2386.LIB` ABI and the concurrent native shell-worker pipeline need that
real build to be considered proven.
