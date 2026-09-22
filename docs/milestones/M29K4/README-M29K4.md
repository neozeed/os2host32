# Milestone 29K4 - DETACH hardening and OS/2 child termination semantics

M29K3 made Ctrl+C safe at the shell prompt and while a foreground child is
running.  M29K4 closes the next two child/session gaps before moving on to the
real `START` / `DosStartSession` work in M29L.

No LE/LX loader logic and no C/386 far16 bridge descriptors change here.

## 1. Host Ctrl+C no longer leaks an NT status into RESULTCODES

A foreground Win32 process that is terminated by an unhandled console Ctrl+C
normally exits with the host status `0xC000013A` (`STATUS_CONTROL_C_EXIT`).
That is not an OS/2 application result code and should not appear directly in
an OS/2 `RESULTCODES` structure.

OS/2 2.x defines these termination reasons:

    0  TC_EXIT
    1  TC_HARDERROR
    2  TC_TRAP       (16-bit child)
    3  TC_KILLPROCESS
    4  TC_EXCEPTION  (32-bit child)

Ctrl+C / Ctrl+Break are signal exceptions in the OS/2 2.x exception model.
M29K4 therefore translates a host `STATUS_CONTROL_C_EXIT` from a 32-bit child
to:

    codeTerminate = TC_EXCEPTION (4)
    codeResult    = 0

The reconstructed shell now derives ERRORLEVEL from the normal result for
`TC_EXIT`, and from the termination reason if an abnormal termination did not
provide an application result.  Consequently an unhandled Ctrl+C foreground
termination should leave:

    ERRORLEVEL=4

instead of a large Windows NT status value.

The same translation is applied to synchronous `DosExecPgm` and to
`DosWaitChild` results.

With `OS2_TRACE_EXEC=1`, the personality prints both the native exit status and
the OS/2 result mapping, for example:

    DOSCALLS EXEC: child pid=... exit=0xC000013A term=4 result=0

## 2. EXEC_BACKGROUND / DETACH hardening

The M29K DETACH implementation already creates a background child with
`DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP` and deliberately does not retain
it in the `DosWaitChild` child table.

M29K4 makes the standard-handle behavior less accidental.  A detached Win32
console process has no console association, so inherited console character
handles are replaced by an inheritable `NUL` handle.  Real redirected file or
pipe handles are preserved.  This means:

    detach child

is quiet and independent of the parent's console, while:

    detach child > detached.out

still inherits the redirected file through the OS/2 HFILE layer.

The background child continues to inherit the current directory and the
normalized environment supplied to `DosExecPgm`.

## New regressions

`build-os2-shell.cmd` additionally builds:

    m29k4-detach-child.exe
    cmdos2_os2_detach_test.exe

The direct API test verifies that `EXEC_BACKGROUND` returns a PID, that the
returned PID is deliberately *not* collectable through `DosWaitChild`, and that
the detached child continues to run and leaves its marker:

    os2host32 --run cmdos2_os2_detach_test.exe

Expected tail:

    background pid=...
    background is not in DosWaitChild result set
    M29K4_DETACH_API_OK

For the shell path run:

    examples\m29k4-detach-hardening.cmd

Expected useful lines include:

    DETACH_RETURNED=0
    M29K4_DETACH_MARKER_OK
    TOKEN=DETACH_ENV_OK
    M29K4_DETACH_STDOUT_OK

The marker proves the background child continued after DETACH returned, the
token checks environment inheritance, and the last line checks redirected
stdout inheritance from a detached child.

There is also a manual parent-lifetime probe:

    examples\m29k4-detach-survive-exit.cmd

Run it inside CMD32OS2.  It launches the delayed child and immediately exits
the shell.  From the outer Windows command prompt, wait about a second and
then inspect:

    type m29k4-survive.ok

It should contain both the detach marker and:

    TOKEN=PARENT_EXIT_OK

## Foreground Ctrl+C check

Run either native or hosted CMD32OS2:

    m29k3-break-child

Press Ctrl+C, then:

    echo %ERRORLEVEL%

The shell must survive and M29K4 expects `4`, representing the OS/2 2.x
32-bit exception termination reason rather than Windows' `0xC000013A`.

If a child handles Ctrl+C itself and exits normally, its own normal result is
left untouched.

## Next

If these pass, freeze M29K4 and move to M29L: implement `START` through a real
SESMGR / `DosStartSession` personality boundary rather than approximating it
with asynchronous `DosExecPgm`.
