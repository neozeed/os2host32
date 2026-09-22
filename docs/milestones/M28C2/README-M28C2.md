# Milestone 28C2 - handle lifetime + DosCreatePipe prototype fix

M28C proved that redirection content reached files, but the native CMD bridge
left an intermediate Win32 HANDLE open after mirroring HFILE 0/1/2 into the C
runtime.  The symptom was successful `type` output followed by OS/2 rc 32 on
`DEL`.  M28C2 keeps the DOSCALLS standard HANDLE authoritative and mirrors it
into the native CRT without replacing the Win32 standard HANDLE a second time.
DOSCALLS tracks handles it installed itself and closes the superseded duplicate
on the next DosDupHandle-to-0/1/2 operation.

The direct OS/2 build failure had a separate cause.  `DosCreatePipe` belongs to
the queue/pipe portion of the OS/2 headers.  `cmdos2_os2.c` did not define
`INCL_DOSQUEUES`, so C/386 saw no prototype and generated an implicit cdecl
external `_DosCreatePipe`.  OS2386.LIB correctly contains `DosCreatePipe` as an
OS/2 API import for DOSCALLS ordinal 239.  Defining `INCL_DOSQUEUES` makes the
compiler use the SDK calling convention and import name.

## Native regression

    make
    cd examples
    ..\cmd32os2.exe
    m28c-redir-test.cmd

Expected tail:

    delete-one=OK
    delete-two=OK
    M28C2 redirection regression complete

## Direct LX regression

    cd ..
    build-os2-backend.cmd
    os2host32.exe --run cmdos2_os2_smoke.exe

Expected near the end:

    pipe child exact payload=OK (20 bytes)
    stdout redirection/append through DosDupHandle=OK
    M28C_OS2_BACKEND_OK
