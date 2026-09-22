# M31A R6 — WMCHAR deferred visible-window activation

R5 established that the native peer is created successfully and the first guest
WM_CREATE callback returns normally.  The process then disappeared while still
inside ShowWindow(), before WinCreateStdWindow could return.

R6 keeps the native window hidden until the first WinGetMsg call.  This lets the
OS/2 application return from WinCreateStdWindow and complete RegisterSwitchEntry
and menu initialization before the Win32 show/activation lifecycle begins.

While the deferred ShowWindow call is in progress, synchronous Win32 WM_PAINT and
WM_ERASEBKGND traffic is handled natively instead of re-entering the guest.  R6
then invalidates the window so the normal PM message loop receives and dispatches
the real OS/2 WM_PAINT immediately afterward.

Run with:

    set OS2_PM_TRACE=1
    os2host32.exe --run WMCHAR.EXE

Expected new progress before the first show:

    PMWIN: show deferred
    PMWIN: WinQueryWindowProcess
    PMWIN: WinQueryWindowText
    PMSHAPI: WinAddSwitchEntry ...
    PMWIN: WinWindowFromID menu ...
    PMWIN: WinGetMsg enter
    PMWIN: deferred ShowWindow

All native messages delivered synchronously during ShowWindow are logged as
`native show msg`.
