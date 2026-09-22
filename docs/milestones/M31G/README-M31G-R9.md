# M31G R9 - PMWIN.804 WinQueryCapture + PMWIN.773 WinIsWindowEnabled

R8 is runtime-proven through `WinDestroyPointer`.  The next untouched OS/2 2.0
GA NEKO boundary is:

    PMWIN.804

Historical OS/2 ordinal material identifies this as:

    HWND WinQueryCapture(HWND hwndDesktop)

R9 implements it as the query half of the existing `WinSetCapture` mapping:
`GetCapture()` obtains the current native capture window and the normal
`guest_hwnd()` bridge converts that HWND back to the OS/2-visible handle.  A
missing capture returns `NULLHANDLE`.

A separate flat-32-bit `e.exe` regression also reaches:

    PMWIN.773

which is:

    BOOL WinIsWindowEnabled(HWND hwnd)

R9 maps that directly to the native HWND state (`IsWindowEnabled`) after the
normal OS/2-to-Win32 HWND conversion and validity check.

Both APIs are generic PM compatibility additions.  There are no NEKO/e.exe
name checks and no application-specific behavior.

## Build / test

    make PMWIN.dll os2host32.exe
    make m31g-neko-r9-check
    os2host32.exe --run examples\m31g-neko\NEKO.EXE

or:

    examples\m31g-neko\m31g-neko-r9-test.cmd

NEKO R9 success is that PMWIN.804 resolves and execution reaches the next real
runtime boundary.  The separate `e.exe` success criterion is that PMWIN.773 no
longer blocks import resolution.
