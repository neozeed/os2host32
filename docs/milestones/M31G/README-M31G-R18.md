# M31G R18 - NEKO visible-cat coordinate fix

R17/R16 runtime evidence finally reaches the real NEKO behavior path:
NEKO.DLL loads, all 36 resources publish, the public WC_STATIC is subclassed,
WinStartTimer runs at 200 ms, and WM_TIMER enters NEKO's guest PFNWP. A captured
frame visibly shows the historical cat and the Cat and Mouse dialog.

The remaining visibility failure is a generic WinSetWindowPos semantic bug.
NEKO creates a 32x32 desktop child/popup at OS/2 (20,20). On its first movement,
WinQueryWindowPos reports y=-12. The exact -32 delta identifies the mistake:
for a move-only WinSetWindowPos call, cx/cy are ignored by OS/2, but the host
bridge used the supplied cy as the window height when converting from OS/2's
bottom-left desktop coordinates to Win32 top-left coordinates.

R18 changes WinSetWindowPos to obtain the current native rectangle first. The
current width/height remain in force unless SWP_SIZE is explicitly set. Thus a
move-only request for y=20 on a 32-pixel-high window maps to native
`screenHeight - 20 - 32` and WinQueryWindowPos maps it back to y=20.

This is not a NEKO workaround. Any PM application using SWP_MOVE without
SWP_SIZE now gets the documented ignored-cx/cy behavior and a reversible
coordinate transform.

Useful first test:

    set OS2LIBPATH=.;C:\cl386-research\os2_2.0\x\OS2\APPS
    set OS2_PM_TRACE=1
    os2host32 --run C:\cl386-research\os2_2.0\x\OS2\APPS\NEKO.EXE

The trace now includes WinSetWindowPos flags and native position/height so any
remaining motion error is directly observable.
