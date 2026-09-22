# M31G R11 - HELPMGR.52 / WinDestroyHelpInstance

R11 introduces a native `HELPMGR.dll` compatibility personality after the
OS/2 2.0 GA HELPMGR scan proved that the historical DLL itself is mixed
16/32-bit code and therefore outside the V1 direct-host execution model.

Only the boundary actually reached is added:

* `HELPMGR.52 = WinDestroyHelpInstance(HWND)`
* returns OS/2 `BOOL`
* accepts only compatibility-owned help-instance handles
* `HELPMGR.51` and `.54` remain intentionally absent

If a real IBM `HELPMGR.DLL` is sitting beside os2host32, rename it before
building/running R11 so it does not shadow the compatibility personality.

Build and test:

    make HELPMGR.dll os2host32.exe
    os2host32 --run NEKO.EXE

Static regression:

    make m31g-neko-r11-check
