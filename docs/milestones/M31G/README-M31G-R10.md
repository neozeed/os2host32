# M31G R10 - PMSHAPI.102 PrfOpenProfile + PMWIN.844 WinQueryWindowUShort

R9 is runtime-proven past PMWIN.804 for NEKO and PMWIN.773 for the separate
flat-32-bit `e.exe` specimen.  The next real boundaries are:

    NEKO: PMSHAPI.102
    e.exe: PMWIN.844

Historical OS/2 ordinal material identifies these as:

    HINI   PrfOpenProfile(HAB hab, PCSZ pszFileName)
    USHORT WinQueryWindowUShort(HWND hwnd, LONG index)

R10 implements both as generic compatibility behavior.

`PrfOpenProfile` allocates a small compatibility HINI token and retains the
requested private INI path.  The existing `PrfQueryProfileInt` and
`PrfWriteProfileString` paths now resolve such private HINI values to that
path; the previous default `os2user.ini` behavior remains the fallback for the
special/default profile handles.  Win32's profile APIs open the file per
operation, so no persistent native file handle is required.

`WinQueryWindowUShort` currently implements the public `QWS_ID` query used to
retrieve the identifier assigned to a PM window/control.  Native child/control
IDs map directly through `GetDlgCtrlID`.  Unsupported window-word offsets
return zero rather than inventing state.

Neither API contains NEKO or e.exe detection.

## Build / test

    make PMSHAPI.dll PMWIN.dll os2host32.exe
    make m31g-neko-r10-check
    os2host32.exe --run examples\m31g-neko\NEKO.EXE

For e.exe, run the same binary you used to prove the PMWIN.844 boundary.
R10 success is that PMSHAPI.102 and PMWIN.844 no longer block import
resolution and each program advances to its next real boundary.
