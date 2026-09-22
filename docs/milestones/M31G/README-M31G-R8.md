# M31G R8 - PMWIN.727 WinDestroyPointer for NEKO

R7 proved `WinCreateWindow` in the real Win32 loader path.  The untouched OS/2
2.0 GA NEKO.EXE then resolves existing PMWIN.910/848/912/915/726 and stops at:

    PMWIN.727

Historical OS/2 ordinal material identifies 727 as:

    BOOL WinDestroyPointer(HPOINTER hptrPointer)

OS/2 permits an application to destroy pointers/icons it created or loaded
privately, but system/shared pointer handles must not be destroyed.

## R8 implementation

R8 adds a small generic owned-HPOINTER registry to PMWIN:

- `WinLoadPointer` records each native handle created by the existing
  OS/2 RT_POINTER -> `CreateIconIndirect` conversion path;
- `WinDestroyPointer` accepts only compatibility-owned handles;
- owned handles are released with `DestroyIcon`, which is the documented
  destruction API for handles returned by `CreateIconIndirect`;
- the registry entry is removed only after a successful destroy;
- system/shared handles such as the current pointer or handles returned by
  `WinQuerySysPointer` are refused and return FALSE rather than being freed.

The registry is intentionally generic so future `WinCreatePointer` support can
use the same ownership path.

## Build / test

    make PMWIN.dll os2host32.exe
    make m31g-neko-r8-check
    os2host32.exe --run examples\m31g-neko\NEKO.EXE

or:

    examples\m31g-neko\m31g-neko-r8-test.cmd

R8 success is that PMWIN.727 resolves at all sites and execution reaches the
next genuine runtime boundary. Do not pre-implement the next missing import.
