# Milestone 28D2 - exact standard-handle ownership

M28D proved the real LX asynchronous process/wait path, but regressed the
native CMD bootstrap's file redirection and left the pipeline bridge brittle.
The failure was in the temporary Win32/CRT bridge, not in the OS/2 ABI path.

## The bug

M28D tracked standard-handle ownership in DOSCALLS with only a boolean.  The
native bootstrap simultaneously mirrors HFILE 0/1/2 into Microsoft CRT fd
0/1/2.  Once `_dup2` changes the CRT side, a boolean is no longer sufficient
to identify which Win32 HANDLE DOSCALLS actually owns.  A later replacement
could close a CRT-owned handle, producing ERROR_INVALID_HANDLE (6), or retain
a hidden pipe writer and prevent EOF.

## M28D2 fix

DOSCALLS now tracks the exact HANDLE it owns for HFILE 0/1/2.  `DosDupHandle`
replaces and closes only that exact owned HANDLE.

The native CMD backend now treats the OS/2 HFILE view as authoritative:

    CmdO2StdSave
        -> DosDupHandle(std, allocate)

    redirect
        -> DosOpen / DosDupHandle(file-or-pipe, std)
        -> duplicate selected Win32 std HANDLE into CRT fd 0/1/2

    CmdO2StdRestore
        -> DosDupHandle(saved, std)
        -> DosClose(saved)
        -> mirror restored std HANDLE into CRT fd 0/1/2

The genuine LX `cmdos2_os2.c` path is unchanged.

## Test order

Build normally:

    make

Then re-run the redirection regression:

    cd examples
    ..\cmd32os2.exe
    m28c-redir-test.cmd

It should finish with:

    delete-one=OK
    delete-two=OK
    M28C4 redirection regression complete

Then build the LX filters if needed:

    cd ..
    build-pipe-fixtures.cmd
    cd examples
    ..\cmd32os2.exe
    m28d2-pipe-test.cmd

M28D2 first runs each LX fixture directly through a file before exercising the
pipeline.  This separates loader/fixture problems from pipe problems.

For extra diagnostics, start CMD with:

    set CMD32_TRACE_PIPE=1
    ..\cmd32os2.exe

Each pipeline then prints the rendered left/right worker commands, PIDs and
final exit codes.

The already-successful M28D direct LX smoke (`async exec/wait=OK`, pipe child,
redirection and `M28D_OS2_BACKEND_OK`) does not need an ABI change in M28D2.
