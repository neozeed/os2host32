# M31A R7 - WMCHAR native IME isolation

R6 proved that WMCHAR reaches its first WinGetMsg and that the deferred native
ShowWindow path dies at Win32 message 0x0281 (WM_IME_SETCONTEXT), before
ShowWindow returns.

R7 keeps Windows IME plumbing out of the OS/2 compatibility window.  The PM
keyboard bridge already translates ordinary Win32 key/char messages to OS/2
WM_CHAR, so there is no guest-visible reason to create/use a host IME context.
The Win32 WM_IME_* family is therefore consumed by PMWIN.

R7 also traces entry/return of DefWindowProcA for all messages delivered while
the deferred ShowWindow is active.  This turns any subsequent native lifecycle
failure into an exact message boundary.
