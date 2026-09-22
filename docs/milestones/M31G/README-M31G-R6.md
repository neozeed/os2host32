# M31G R6 - PMWIN.780 WinLoadPointer for NEKO

R5 proved the native PMWP personality and ordinal 203, then the untouched
OS/2 2.0 GA NEKO.EXE advanced to its next unresolved import:

    PMWIN.780

Historical OS/2 import tables and PM documentation identify ordinal 780 as:

    HPOINTER WinLoadPointer(HWND hwndDesktop, HMODULE hmod, ULONG idres)

OS/2 uses the same RT_POINTER resource class for icons and mouse pointers.
NEKO contains one untouched type-1 resource, id 1.  The compatibility layer
already had generic RT_POINTER lookup plus OS/2 2.x bitmap-array conversion
for frame/dialog icons, so R6 exposes that machinery through the real API.

## Generic resource handling

R6 refactors the existing colour-icon converter into a common icon/pointer
converter.  It accepts OS/2 IC/CI icon signatures and PT/CP pointer signatures;
actual pointer resources retain their resource hotspot in ICONINFO before
CreateIconIndirect.  The original frame/dialog icon helper remains as a strict
icon-only wrapper, preserving all prior behavior.

WinLoadPointer:

- treats hmod 0 (and the existing synthetic main-module handle 1) as the guest
  executable through the existing resource registry;
- looks up RT_POINTER/idres;
- converts the OS/2 bitmap-array data into a native Win32 icon/cursor handle;
- returns NULLHANDLE for absent or unsupported resource data;
- contains no NEKO-specific resource IDs or fallback imagery.

## Build / test

    make PMWIN.dll os2host32.exe
    make m31g-neko-r6-check
    os2host32.exe --run examples\m31g-neko\NEKO.EXE

or:

    examples\m31g-neko\m31g-neko-r6-test.cmd

R6 success is that PMWIN.780 resolves and the loader advances to the next
actual boundary.  Do not pre-implement that boundary until the Windows run
identifies it.
