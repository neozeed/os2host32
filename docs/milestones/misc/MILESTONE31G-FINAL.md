# M31G FINAL — OS/2 2.00 GA NEKO on native Win32

## Status

**Frozen/proven milestone.**

The untouched Microsoft/IBM OS/2 2.00 GA `NEKO.EXE` (“Cat and Mouse”) now runs
through the V1/native-Win32 os2host32 path on Windows 10 x64 using its original
`NEKO.DLL` resources.

Runtime proof from the Windows test system confirms:

- the original flat-32-bit LX `NEKO.EXE` loads and executes;
- all 63 NEKO imports are covered by the compatibility personalities;
- original `NEKO.DLL` loads as a resource-only flat-32-bit LX module;
- its three base-zero objects are relocated into distinct 64-KB slots;
- all 36 resources are published under a synthetic OS/2 HMODULE;
- the historical cat pointer/icon frames decode and display correctly;
- native public-control `WinSubclassWindow` dispatch reaches the guest PFNWP;
- the 200 ms PM timer is delivered as OS/2 `WM_TIMER`;
- the cat moves correctly after the `WinSetWindowPos` move-only coordinate fix;
- the Cat Set-up Control Panel renders all three `WC_SLIDER` controls and they
  operate through the Win32 trackbar bridge;
- `~Hide` no longer hides Explorer or unrelated Windows applications because
  the OS/2 `HWND_DESKTOP` namespace is isolated from the host desktop;
- the previously frozen PM regression programs still run after the R19 changes.

This is the completion point for M31G. Do not reopen NEKO work unless a concrete
regression is found.

## Frozen parent

M31F FINAL R7 remains the immutable PM regression baseline. M31G R19 FINAL is
an additive milestone on top of it.

Known-good PM applications across the preserved regression chain include:

- WMCHAR
- HANOI
- BIO
- HELLO with the original guest OPENDLG.DLL
- Sarien PM
- JIGSAW with untouched YOSEMITE.BMP
- NEKO / Cat and Mouse with original NEKO.DLL

## Final NEKO architecture

### Loader/runtime

NEKO.EXE is a flat 32-bit LX application. NEKO.DLL is effectively a resource
DLL for this V1 path: no entry point, no imports and no fixups. All three DLL
objects declare preferred base zero, so R16 added generic relocatable packed
object placement instead of trying to map them all at address zero.

The successful runtime checkpoint is:

    GUESTMOD LOAD: NEKO -> ...NEKO.dll handle=00010000 refs=1
    LX object preferred ranges overlap; using packed relocation arena ...
    mapped object 1 ...
    mapped object 2 ...
    mapped object 3 ...
    GUESTMOD RESOURCES: NEKO handle=00010000 published=36
    GUESTMOD READY: NEKO ... imports=0 named=0

Module discovery intentionally follows `OS2LIBPATH`; include `.` explicitly if
NEKO.DLL is in the current directory.

### PMWP

The real OS/2 2.0 GA PMWP.DLL is mixed 16/32-bit and is not executed by V1.
PMWP remains a native compatibility personality.

Re-analysis of historical PMWP ordinal 203 proved that its successful path uses
`DosLoadModule` to load the named module and write its HMODULE. This is how
NEKO obtains the resource HMODULE for NEKO.DLL.

### Cat window/subclass/timer

NEKO creates a public `WC_STATIC` / `SS_ICON` window, subclasses it, and uses a
200 ms timer. R14 added real native-public-control subclassing while preserving
the older managed/private-class behavior. The old public STATIC procedure
supports the OS/2 static-control `SM_QUERYHANDLE` / `SM_SETHANDLE` messages.

R18 fixed `WinSetWindowPos`: for move-only calls (`SWP_MOVE` without
`SWP_SIZE`), OS/2 ignores supplied `cx/cy`; the bridge must use the current
native window height when converting bottom-left OS/2 Y to top-left Win32 Y.
The old bug moved the 32-pixel cat from OS/2 y=20 to queried y=-12, exactly one
cat height off-screen.

### Safe HWND_DESKTOP namespace

R19 establishes an important V1 safety boundary. `HWND_DESKTOP` represents the
OS/2 PM desktop namespace; it does not grant authority over arbitrary native
Windows applications.

`WinBeginEnumWindows(HWND_DESKTOP)` now filters real Win32 top-level windows to
those owned by the current os2host32 process. `WinShowWindow` refuses foreign
process HWNDs and refuses hiding the native desktop itself. This is required
because NEKO's historical `~Hide` feature intentionally hides other PM windows.
Without this filter it could hide Explorer and unrelated Win32 applications.

### WC_SLIDER

R19 maps OS/2 public class 38 (`WC_SLIDER`) to the Win32 trackbar common
control for both dialog resources and direct `WinCreateWindow` calls.

Implemented NEKO-proven messages:

- `SLM_QUERYSLIDERINFO` (`0x036C`)
- `SLM_SETSLIDERINFO` (`0x0371`)
- `SLM_SETTICKSIZE` (`0x0372`)
- arm position with increment values
- arm sizing / tick placement
- native tracking/change -> OS/2 `WM_CONTROL`

This produces the three visible controls in the Cat Set-up Control Panel:
Play time, Speed and Step.

## Regression gate

Run:

    make m31g-neko-r19-check

The full chained gate must remain green from M31E/M31F through M31G R19.
The R19-specific final lines are:

    M31G R19 WC_SLIDER + desktop namespace regression PASS
      class 38 maps to a Win32 trackbar with SLM 036C/0371/0372 translation
      slider tracking/change is translated to OS/2 WM_CONTROL notifications
      HWND_DESKTOP enumeration excludes windows outside the os2host32 process
      WinShowWindow refuses HWND_DESKTOP hide and foreign-process HWNDs

## Recommended manual regression

    make PMWIN.dll os2host32.exe
    set OS2LIBPATH=.;C:\cl386-research\os2_2.0\x\OS2\APPS
    os2host32 --run C:\cl386-research\os2_2.0\x\OS2\APPS\NEKO.EXE

Expected behavior:

- cat is visible and animated over normal Windows applications;
- the setup dialog shows all three sliders;
- moving the sliders continues to update NEKO;
- `~Hide` does not hide Explorer or unrelated host applications.

For diagnostic tracing:

    set OS2_PM_TRACE=1

## Separate e.exe thread

`E.EXE` is also a flat 32-bit LX V1 target, but it is **not part of the frozen
M31G NEKO success criterion**. Work completed opportunistically while walking
NEKO added several APIs needed by E, including PMWIN.773/844/833,
DOSCALLS.219/218/212 and NLS.5 `DosQueryCtryInfo`.

The last E-specific compatibility addition was R17 NLS.5. The next actual E
runtime boundary after that has not been frozen here. If work resumes on E,
run the untouched executable and continue from the first real failure rather
than guessing from its static import order.

## Next-context rule

Start future work from **M31G R19 FINAL**, with M31F FINAL R7 as the immutable
regression parent. Keep the V1/native-Win32 branch separate from the frozen
WHP/V2 experiments and from frozen EMX work.

For a new PM application:

1. `--scan` the untouched binary.
2. Confirm flat-32-bit LE/LX/direct-host suitability.
3. Run it unchanged.
4. Fix only the first real generic loader/API/behavior boundary.
5. Add a regression before moving on.
6. Never add executable-name-specific behavior.

