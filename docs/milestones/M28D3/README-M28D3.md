# M28D3 - native pipeline worker standard-handle synchronization

M28D2 proved the genuine LX process layer (`DosExecPgm(EXEC_ASYNCRESULT)`,
`DosWaitChild`, `DosCreatePipe`, `DosDupHandle`) but exposed a native-bootstrap
problem: the Win32 `STARTUPINFO` standard HANDLEs and MinGW/MSVCRT fd 0/1/2
could diverge inside a pipeline worker.

The decisive symptom was an LX filter returning 2 from `putchar()` even though
its producer exited normally.  M28C4 had already shown the missing piece: after
`_dup2`, republish `_get_osfhandle(fd)` with `SetStdHandle`.

M28D3 therefore keeps two deliberately separate duplicates:

* DOSCALLS owns the authoritative OS/2 HFILE 0/1/2 duplicate.
* the temporary native CMD bootstrap owns a CRT fd 0/1/2 duplicate.

`native_mirror_std_to_crt()` now republishes the *post-dup2* CRT HANDLE as the
Win32 standard HANDLE.  This prevents `CreateProcess` from inheriting a stale
HANDLE value whose temporary descriptor has already been closed.

On startup CMD also synchronizes inherited `GetStdHandle()` values into CRT
fd 0/1/2.  That matters for native pipeline workers executing builtins such as
`echo`, and disappears when CMD itself becomes LX.

## Regression

Build the native components and C/386 pipe fixtures, then:

    cd examples
    ..\cmd32os2.exe
    m28c-redir-test.cmd
    m28d3-pipe-test.cmd

For extra process tracing before starting the shell:

    set CMD32_TRACE_PIPE=1

Expected pipeline payloads are `ONE/TWO/THREE` for the external filters and
`MIXEDCASE` for the builtin-to-LX case.
