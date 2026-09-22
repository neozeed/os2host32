# M31G R15 - e.exe PMWIN.833 WinQueryVersion + NEKO R14 path diagnosis

R14 has two independent Windows results.

## e.exe

The editor now resolves DOSCALLS.218 DosSetFileInfo and proceeds through its
remaining existing imports until the next actual boundary:

    PMWIN.833

Historical OS/2 ordinal material identifies this as the one-argument PM API:

    ULONG WinQueryVersion(HAB hab)

R15 exports ordinal 833 and advertises the OS/2 2.0 Presentation Manager
contract as `0x00020000`.  The answer is intentionally independent of the
Win32 host version: guest applications should see the compatibility
personality they are running against, not Windows 10/11 version metadata.

## NEKO

The first R14 traced run did not reach the new native-control subclass/timer
code.  It stopped during NEKO's own startup because PMWP.203's now-real
DosLoadModule bridge tried to load the bare resource module name `NEKO` and
returned APIRET 126 (`ERROR_MOD_NOT_FOUND`):

    PMWP: ordinal 203 module="NEKO" rc=126 hmod=00000000 fail="NEKO"

NEKO's startup path treats that as an error and calls WinAlarm, explaining the
audible ding before exit.  This is not evidence that R14 subclassing failed.

Run NEKO with the directory containing NEKO.DLL on OS2LIBPATH, for example:

    set OS2LIBPATH=C:\cl386-research\os2_2.0\x\OS2\APPS;.
    set OS2_PM_TRACE=1
    os2host32 --run C:\cl386-research\os2_2.0\x\OS2\APPS\NEKO.EXE

Do not add an application-specific executable-directory resource fallback.
Normal guest module search semantics remain the compatibility mechanism.

## Regression policy

R15 is additive.  It preserves R14's PMWP.203 DosLoadModule bridge,
resource-only guest-DLL publication, native public-control subclassing,
SM_QUERYHANDLE/SM_SETHANDLE compatibility, timer translation, and
DOSCALLS.218 behavior.  The frozen M31F regression suite remains mandatory.
