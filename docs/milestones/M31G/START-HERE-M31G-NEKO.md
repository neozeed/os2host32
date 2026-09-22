# START HERE - M31G NEKO (OS/2 2.0 GA Cat and Mouse)

## Frozen parent

M31F FINAL R7 is the frozen parent.  Preserve the full known-good regression
set: WMCHAR, HANOI, BIO, HELLO + real historical OPENDLG.DLL, Sarien PM, and
JIGSAW with untouched YOSEMITE.BMP.

This branch remains **V1/native Win32 only**.  Do not introduce WHP/V2 or EMX.
PMEXEC remains out of scope because it is genuine segmented 16-bit LE.

## Target specimen

`examples/m31g-neko/NEKO.EXE` is the untouched OS/2 2.0 GA Cat and Mouse LX
program supplied for M31G.

SHA-256:
`3315d6ccbb8ec2f2a1ffb53c3444257bf798f7af4cad9527738cb54cb1d45b0b`

The executable is a valid V1 target:

- LX, 3 objects, all 32-bit
- OFF32 internal fixups only
- REL32 external fixups only
- one resource: type 1, id 1, size 3242
- 63 distinct ordinal imports from DOSCALLS, PMGPI, PMWIN, PMSHAPI, HELPMGR,
  and PMWP

## M31G R1 scope

R7 fails before import resolution because NEKO uses standard LX iterated
pages.  R1 changes only the generic LX mapper.

NEKO page inventory relevant to R1:

- page 5: PAGE_ITERATED, 1856 encoded bytes -> 4096 logical bytes
- page 6: PAGE_ITERATED, 2622 encoded bytes -> 3244 logical bytes

`os2host32.c` now parses the LX iterated-page base and expands flag-1 records
with strict input/output bounds.  PAGE_COMPRESSED remains unsupported.

## First test on Win32

From the tree root:

    make os2host32.exe
    os2host32.exe --run examples\m31g-neko\NEKO.EXE

Or from `examples\m31g-neko` run `m31g-neko-r1-test.cmd`.

**R1 success criterion is not a running cat yet.**  Success is that the old
`iterated/compressed/invalid LX page is not supported yet` failure disappears,
all three objects map, and execution advances to the next generic boundary
(expected to be unresolved imports/API coverage).

## Regression checks

    make m31g-neko-r1-check

This first runs the frozen M31F static regression gate, then checks NEKO's
identity, flat-32-bit execution model, fixup shapes, resource inventory,
iterated pages, import inventory, and presence of the generic loader support.

## What comes next

Do not pre-implement all NEKO imports blindly.  Capture the first R1 Win32
failure and implement the smallest generic API tranche needed to move that
boundary forward.  HELPMGR and PMWP should become compatibility personalities
only where the historical program genuinely reaches them.  Keep every fix
generic and add NEKO to the manual regression set once it first runs visibly.


## M31G R2 - first import boundary

The first Win32 R1 run maps all three objects successfully and stops at
`PMSHAPI.129`.  Historical ordinal material identifies this as
`WinRemoveSwitchEntry(HSWITCH)`.  R2 adds that generic export only, using the
existing PMSHAPI model where the switch handle is a compatibility token and
the native Win32 top-level window already belongs to the host task switcher.
The call therefore returns 0 (success) and optionally traces under
`OS2_PM_TRACE`.

After rebuilding `PMSHAPI.dll`, run NEKO again and use the next actual runtime
boundary to choose R3.

## M31G R3 - PMWIN.837 WinQueryWindowPos

The R2 Win32 run resolves PMSHAPI.129 and then stops at `PMWIN.837`.
Historical ordinal tables identify 837 as `WinQueryWindowPos(HWND, PSWP)`.

R3 adds the real flat-32-bit OS/2 SWP layout and a generic native mapping:
Win32 `GetWindowRect` geometry is converted from screen/top-left coordinates to
OS/2 parent-relative/bottom-left coordinates; minimized/maximized state and
basic Z-order/handle fields are returned as well.

After rebuilding `PMWIN.dll`, run NEKO again and capture the next actual
boundary.  Static import order suggests PMWIN.778 is next, but do not implement
it until the Win32 run confirms that boundary.


## M31G R4 - PMWIN.778 WinLoadMenu

The R3 Win32 run resolves PMWIN.837/838/903/840 and then stops at
`PMWIN.778`.  Historical ordinal tables identify 778 as
`WinLoadMenu(HWND, HMODULE, ULONG)`.

R4 reuses the existing registered `RT_MENU` resource lookup and recursive
OS/2 menu-template parser.  It returns the resulting compatibility `HMENU`
handle on success and `NULLHANDLE` for a missing or invalid resource.  It does
not invent a menu when none exists and does not attach one to a native frame
speculatively.

After rebuilding `PMWIN.dll`, run NEKO again and capture the next actual
boundary before implementing R5.


## M31G R5 - PMWP.203 native personality

The R4 run resolves PMWIN.778, then attempts to load the real OS/2 2.0 GA
`PMWP.DLL` and stops because that Workplace Shell module is genuinely mixed
16/32-bit and uses selector/far-pointer fixups. Do not pull that machinery into
V1.

R5 instead makes PMWP a normal host compatibility personality and exports only
the API NEKO actually reaches: ordinal 203. The GA export and NEKO call site
prove a four-32-bit-argument ABI and zero success return, but a trustworthy
historical public function name remains unidentified, so the export stays
ordinal-only (`PMWPOrdinal203 @203 NONAME`).

If the historical IBM DLL was copied into the working directory, rename it
before building R5; `PMWP.dll` must now be the native Win32 compatibility DLL.
Run NEKO and capture the next boundary before implementing R6.

## R6 - WinLoadPointer boundary

After R5, the real runtime advances to PMWIN.780.  R6 identifies this as
WinLoadPointer(HWND, HMODULE, ULONG), exports ordinal 780, and reuses the
registered RT_POINTER resource path.  The shared OS/2 bitmap-array converter
now handles IC/CI icons plus PT/CP pointers (including pointer hotspots), while
frame/dialog icon behavior remains icon-only.  Capture the next real runtime
boundary before adding R7.

## M31G R7 - PMWIN.909 WinCreateWindow

The R6 Windows run resolves PMWIN.780 at all 26 sites, then existing
PMWIN.716/781, and stops at PMWIN.909.  R7 identifies this as the ordinary
13-argument `WinCreateWindow` API and maps common OS/2 predefined WC_* classes
to native Win32 controls.  PM bottom-left coordinates are converted relative
to the parent; desktop controls use screen height.  WC_STATIC/SS_ICON `"#N"`
references use the guest executable's registered RT_POINTER resource instead
of host-DLL resources.  Registered private classes retain the established
pm_wndproc path.  Capture the next real runtime boundary before adding R8.


## M31G R8 - PMWIN.727 WinDestroyPointer

The R7 Windows run resolves PMWIN.909 and then existing 910/848/912/915/726,
stopping at PMWIN.727. R8 identifies this as `WinDestroyPointer(HPOINTER)`.
`WinLoadPointer` now records compatibility-owned pointer/icon handles and
`WinDestroyPointer` destroys only those handles, refusing system/shared handles.
The native handles come from `CreateIconIndirect` and are released with
`DestroyIcon`. Capture the next real runtime boundary before adding R9.


## M31G R9 - PMWIN.804 WinQueryCapture + PMWIN.773 WinIsWindowEnabled

The R8 Windows run resolves PMWIN.727 and then the existing pointer/window APIs,
stopping at PMWIN.804. Historical ordinal material identifies 804 as
`WinQueryCapture(HWND_DESKTOP)`. R9 pairs it with the already-established
`WinSetCapture` mapping and returns the current native capture HWND through the
normal guest HWND conversion.

At the same time, the separate flat-32-bit `e.exe` regression reaches PMWIN.773.
That ordinal is `WinIsWindowEnabled(HWND)`, implemented generically with the
native HWND enabled-state query. This is deliberately independent of NEKO and
adds no executable-name checks or application-specific behavior.

After rebuilding PMWIN.dll, run NEKO again and capture its next actual boundary.
The next likely NEKO gap after 804 is PMSHAPI.102, but do not implement it until
the Win32 run confirms that boundary.

## M31G R10 - PMSHAPI.102 + PMWIN.844

The real R9 Windows runs prove two new independent boundaries: untouched GA
NEKO reaches PMSHAPI.102 and the separate flat-32-bit e.exe reaches PMWIN.844.
R10 identifies them as PrfOpenProfile(HAB,PCSZ) and
WinQueryWindowUShort(HWND,LONG). Private HINI values now retain their requested
INI path for the existing host-profile bridge, while WinQueryWindowUShort
implements the public QWS_ID window/control identifier query. No executable
name checks were added. Run both specimens again and use their next actual
failures to select R11.


## M31G R11
NEKO next reaches HELPMGR.52.  R11 adds a native HELPMGR personality and only `WinDestroyHelpInstance(HWND)` at ordinal 52.  The original GA HELPMGR.DLL is mixed 16/32-bit and remains a reference specimen, not a V1 guest module.  Rename any copied IBM `HELPMGR.DLL` before building so the compatibility DLL owns that filename.

## M31G R12
The R11 Windows runs advance NEKO to PMWIN.884 and e.exe to DOSCALLS.219. R12 adds
`WinStartTimer` with WM_TIMER translation and `DosSetPathInfo` with real
FIL_STANDARD timestamp/attribute handling. Keep the two paths independent and use
the next runtime failures to choose R13.

## M31G R13 - import-complete tranche

After R12, the remaining NEKO import set was small and fully known, so R13
intentionally completes all 13 in one generic tranche to protect continuity.
The additions cover PM window enumeration/drawing/visibility/pointer state,
binary profile data, RT_BITMAP loading, and the native HELPMGR create/associate
lifetime. `tools/check_m31g_neko.py` must now report **0 remain outside current
.def coverage** for untouched NEKO.EXE. The separate e.exe path also gains
DOSCALLS.212 DosError.

R13 does **not** claim NEKO runtime behavior is complete. Once every import
resolves, treat the first crash/hang/incorrect UI as the next real boundary.
In particular, the long-standing collapsed/no-op WinSubclassWindow policy is a
prime suspect because NEKO subclasses its WC_STATIC/SS_ICON cat window and then
starts timers.

## M31G R14 - first post-import runtime boundary

The first import-complete R13 run did not display the cat.  Its trace provided
two independent causes: PMWP.203 left NEKO's animation-module HMODULE at zero,
so all later WinLoadPointer animation-frame loads missed, and native WM_TIMER
messages reached the WC_STATIC hwnd without entering the guest because
WinSubclassWindow was still the old collapsed/no-op implementation.

R14 corrects PMWP.203 from stronger reverse-engineering evidence: the original
GA export calls DOSCALLS.318 DosLoadModule with arg3 as the module name and arg2
as PHMODULE.  The generic guest resource publisher also supports resource-only
DLLs that do not themselves import PMWIN.

For native PM public controls, WinSubclassWindow now installs pm_wndproc as a
real Win32 subclass bridge, preserves the original native WNDPROC, and returns
a guest-callable original-control PFNWP.  The latter implements OS/2 static
SM_QUERYHANDLE/SM_SETHANDLE and delegates core native control processing.
Already-managed/private compatibility windows retain the frozen collapsed
behavior used by BIO.

The separate e.exe path gains DOSCALLS.218 DosSetFileInfo with FIL_STANDARD
handle metadata updates.

For NEKO, put the directory containing NEKO.DLL on OS2LIBPATH before the R14
run.  The next evidence should be module/resource loading and actual guest
WM_TIMER dispatch, not another missing ordinal.

## M31G R15 - R14 NEKO rerun prerequisite + e.exe WinQueryVersion

R14's first traced NEKO run exited before window creation. PMWP.203 correctly
called DosLoadModule for the bare module name `NEKO`, but DOSCALLS returned 126
(ERROR_MOD_NOT_FOUND), so NEKO took its own WinAlarm failure path. Put the
directory containing NEKO.DLL on OS2LIBPATH and rerun R14/R15 traced; the
subclass/timer work has not yet been exercised by that run.

The separate e.exe path reached PMWIN.833. R15 adds generic
WinQueryVersion(HAB), advertising the OS/2 2.0 PM compatibility contract as
0x00020000. No executable-name checks are introduced.

## M31G R16 - NEKO.DLL zero-base LX object relocation

NEKO's external resource DLL is V1-compatible (flat 32-bit LX, no imports or
fixups) but all three objects declare preferred relocation base 0.  R16 fixes
the generic loader so address zero is never treated as a directly requestable
VirtualAlloc preferred address and duplicate/overlapping preferred object ranges
receive distinct packed 64-KB arena slots.  With NEKO.DLL on OS2LIBPATH, the
next run should map all three objects and publish all 36 resources before
entering the R14 WinSubclassWindow/timer behavior.

## M31G R17 - NEKO lookup clarification + E NLS.5

NEKO.DLL remains a resource-only module for this V1 path: entry object 0, no
imports and no fixups. It still must be loaded to establish the HMODULE used by
WinLoadPointer and other PM resource APIs. A traced `PMWP ... rc=126` with no
preceding `GUESTMOD LOAD` is a path lookup failure, not a zero-base mapping or
execution failure. Ensure the actual DLL directory is present in OS2LIBPATH;
include `.` explicitly if NEKO.DLL is in the current directory.

The separate E.EXE path reaches NLS.5. R17 adds DosQueryCtryInfo with the real
32-bit COUNTRYINFO shape and current host locale/OEM-codepage formatting.

## M31G R18 - visible cat; move-only WinSetWindowPos fix

The first successful resource-DLL run proves the entire R14/R16 path: NEKO.DLL
loads and publishes 36 resources, WinLoadPointer decodes the animation frames,
the WC_STATIC is really subclassed, and WM_TIMER reaches the guest. A captured
screen shows the original 32x32 cat and Cat and Mouse dialog, but the cat is
visible only briefly.

The trace explains this exactly. The cat is created at OS/2 y=20, but the first
WinQueryWindowPos after a move-only WinSetWindowPos returns y=-12: one 32-pixel
cat height too low. WinSetWindowPos was using the ignored cy argument as the
height whenever SWP_SIZE was absent. R18 instead uses GetWindowRect's current
width/height unless SWP_SIZE is explicitly requested, making the desktop
bottom-left/top-left transform reversible for move-only calls.

## M31G R19 - WC_SLIDER + protected guest desktop namespace

R18 is runtime-proven: untouched OS/2 2.00 GA NEKO runs, displays the original
cat frames from NEKO.DLL, and moves across the Windows 10 x64 desktop.

The working program revealed two generic PM gaps. NEKO's `~Hide` feature uses
WinBeginEnumWindows(HWND_DESKTOP) to hide other PM windows; because V1 exposed
the real Win32 desktop, the guest could hide Explorer and unrelated host apps.
R19 filters desktop enumeration to windows owned by the os2host32 process and
adds a WinShowWindow foreign-HWND safety fence.

The setup dialog's three blank controls are class ordinal 38, WC_SLIDER. R19
maps them to Win32 trackbars and implements the SLM_QUERYSLIDERINFO,
SLM_SETSLIDERINFO and SLM_SETTICKSIZE behavior exercised by NEKO, including
WM_CONTROL change/tracking notifications.

## FINAL FREEZE — R19 runtime proven

M31G is complete at R19. User runtime testing on Windows 10 x64 confirms the
untouched OS/2 2.00 GA NEKO application visibly animates using its original
NEKO.DLL resources; all three Cat Set-up Control Panel WC_SLIDER controls render
and operate; `~Hide` is safely confined to the os2host32 guest window namespace;
and the earlier PM regression applications continue to work.

Do not continue changing NEKO merely for cleanup. Use `MILESTONE31G-FINAL.md`
and `NEXT-CHAT-HANDOFF.md` when starting the next context/application.
