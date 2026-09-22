# M31G R3 - NEKO reaches PMWIN WinQueryWindowPos

R2 proved the generic PMSHAPI.129 `WinRemoveSwitchEntry` bridge and advanced
NEKO to the next loader boundary:

    os2host32: PMWIN ordinal 837 is not exported

Historical OS/2 ordinal tables identify PMWIN.837 as:

    BOOL WinQueryWindowPos(HWND hwnd, PSWP pswp)

R3 adds that generic PMWIN API only.  The 32-bit OS/2 `SWP` layout is preserved:

    fl, cy, cx, y, x, hwndInsertBehind, hwnd, ulReserved1, ulReserved2

The native Win32 peer is queried with `GetWindowRect`.  For child windows the
screen rectangle is mapped into the parent client coordinate system; for
ordinary top-level windows it is mapped against the desktop.  Win32 top-left Y
coordinates are then converted to OS/2 bottom-left Y coordinates.  Current
minimized/maximized state is reflected in `swp.fl`, and the immediate preceding
native Z-order window is returned through `hwndInsertBehind`.

No NEKO-specific behavior is present.

## Build / test

    make PMWIN.dll os2host32.exe
    make m31g-neko-r3-check
    os2host32.exe --run examples\m31g-neko\NEKO.EXE

or run:

    examples\m31g-neko\m31g-neko-r3-test.cmd

The expected R3 result is to move beyond `PMWIN ordinal 837 is not exported`.
Based on the untouched executable's import order and current exports, the next
loader boundary is expected to be PMWIN.778, but R3 intentionally does not
pre-implement it.
