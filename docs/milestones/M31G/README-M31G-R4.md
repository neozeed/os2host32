# M31G R4 - NEKO reaches PMWIN WinLoadMenu

R3 proved `WinQueryWindowPos` and advanced the untouched OS/2 2.0 GA NEKO.EXE
to:

    os2host32: PMWIN ordinal 778 is not exported

Historical ordinal tables identify PMWIN.778 as:

    HWND WinLoadMenu(HWND hwndParent, HMODULE hmod, ULONG idMenu)

R4 exports that generic API by reusing PMWIN's existing resource-backed menu
infrastructure.  Registered `RT_MENU` resources are located with
`pm_find_resource`, decoded by the same recursive OS/2 menu-template parser
already used by `WinCreateStdWindow`, and returned through the existing native
`HMENU` compatibility representation.

There is deliberately no synthetic menu fallback and no NEKO-specific logic.
If the module/resource does not exist or the template is invalid,
`WinLoadMenu` returns `NULLHANDLE`.

R4 also deliberately does not call Win32 `SetMenu`: an OS/2 menu loaded by
`WinLoadMenu` can be used as an action bar or popup, and attachment semantics
should follow the caller's subsequent PM API usage rather than be guessed at
load time.

## Build / test

    make PMWIN.dll os2host32.exe
    make m31g-neko-r4-check
    os2host32.exe --run examples\m31g-neko\NEKO.EXE

or run:

    examples\m31g-neko\m31g-neko-r4-test.cmd

The R4 success criterion is simply to move beyond
`PMWIN ordinal 778 is not exported` and capture the next real compatibility
boundary.
