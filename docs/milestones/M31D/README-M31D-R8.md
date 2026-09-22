# M31D R8 - OS/2 class child-clipping fidelity

BIO registers its main `Biorhythm` window class with `CS_CLIPCHILDREN | CS_SIZEREDRAW`.
Previous PMWIN builds discarded the OS/2 class style completely.  That lets the parent
chart repaint through the area occupied by the `Legend` child, erasing the child client
contents after KidWndProc has successfully drawn them.

R8 stores the OS/2 class style alongside each registered guest wndproc and maps
`CS_CLIPCHILDREN` (0x20000000) to native `WS_CLIPCHILDREN` when instances of that class
are created.  No BIO-specific window names or IDs are used.

The existing Win32 class redraw behavior is deliberately left unchanged.

Run:

    make clean
    make tools compat
    make m31d-bio-r8-check
    set OS2_PM_TRACE=1
    os2host32.exe --run examples\m31d-bio\BIO.EXE

With tracing enabled the main frame should report `class CS_CLIPCHILDREN` during
WinCreateStdWindow.
