# M31G R19 - NEKO sliders and safe desktop namespace

R18 proved the central NEKO goal: the original OS/2 2.00 GA Cat and Mouse
binary and its original NEKO.DLL run through the native Win32 V1 path and the
cat visibly animates on Windows 10 x64.

R19 addresses the two compatibility issues revealed by that successful run.

## Guest desktop isolation

`HWND_DESKTOP` is an OS/2 PM namespace, not authority over arbitrary native
Windows applications.  `WinBeginEnumWindows(HWND_DESKTOP)` now returns only
windows belonging to the current os2host32 process.  This prevents programs
such as NEKO, whose Hide feature intentionally hides other PM applications,
from hiding Explorer or unrelated host windows.  `WinShowWindow` also refuses
foreign-process HWNDs and treats hiding HWND_DESKTOP itself as invalid.

## WC_SLIDER

OS/2 public class 38 (`WC_SLIDER`) is translated to the Win32 trackbar common
control.  Dialog resources and direct `WinCreateWindow` calls both use the
same mapping.  R19 implements the slider operations proven by NEKO's binary:

- `SLM_QUERYSLIDERINFO` (`0x036C`)
- `SLM_SETSLIDERINFO` (`0x0371`)
- `SLM_SETTICKSIZE` (`0x0372`)
- `SMA_SLIDERARMPOSITION` with `SMA_INCREMENTVALUE`
- slider arm sizing and tick placement
- native tracking/change -> OS/2 `WM_CONTROL` notifications

NEKO's three setup controls (Play time, Speed, Step) should therefore render as
real sliders and update the guest when moved.

Recommended test:

    make PMWIN.dll os2host32.exe
    set OS2LIBPATH=.;C:\cl386-research\os2_2.0\x\OS2\APPS
    set OS2_PM_TRACE=1
    os2host32 --run C:\cl386-research\os2_2.0\x\OS2\APPS\NEKO.EXE

The first safety test should use `~Hide`: only os2host32/NEKO-owned windows
should disappear. Explorer and unrelated Windows applications must remain
visible and usable.

## Runtime result — FINAL

Windows 10 x64 runtime testing confirms R19 is successful:

- the historical cat is visible and animates correctly;
- all three setup-panel sliders render and work;
- `~Hide` no longer affects Explorer or unrelated host applications;
- the prior PM regression programs remain working.

R19 is therefore frozen as **M31G FINAL**. See `MILESTONE31G-FINAL.md`.
