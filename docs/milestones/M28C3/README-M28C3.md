# Milestone 28C3 - native handle ownership fix

M28C2 proved the genuine LX path, including DosCreatePipe and DosDupHandle, but
the native Win32 bootstrap could hang during the redirection regression and
leave files locked with rc=32.

The reason is that the Microsoft CRT is allowed to manage the OS HANDLE behind
file descriptors 0/1/2 while DOSCALLS was simultaneously replacing the Win32
standard HANDLE.  Treating both layers as owners created orphaned duplicates and
ambiguous close order.

M28C3 makes the ownership boundary explicit:

* `cmdos2_win32.c` owns native bootstrap fd 0/1/2 with `_dup`, `_dup2`, and
  `_open`, then mirrors the resulting HANDLE with `SetStdHandle`.
* `cmd32os2.c` still only calls the host-neutral `CmdO2Std*` interface.
* `cmdos2_os2.c` still implements the same interface with real OS/2
  `DosOpen`, `DosSetFilePtr`, `DosDupHandle`, and `DosClose`.

This is intentional: native CMD is only the bootstrap personality.  The LX CMD
will use the OS/2 handle implementation, which M28C2's smoke test already proved.

## Regression

    make
    cd examples
    ..\cmd32os2.exe
    m28c-redir-test.cmd

Expected tail:

    delete-one=OK
    delete-two=OK
    M28C3 redirection regression complete

The direct LX smoke test is unchanged:

    build-os2-backend.cmd
    os2host32.exe --run cmdos2_os2_smoke.exe

Expected tail includes:

    pipe child exact payload=OK (20 bytes)
    stdout redirection/append through DosDupHandle=OK
    M28C_OS2_BACKEND_OK
