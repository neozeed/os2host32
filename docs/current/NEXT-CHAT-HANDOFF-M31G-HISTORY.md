# NEXT CHAT HANDOFF - os2host32 V1 native Win32

Read this first, then `MILESTONE31F-FINAL.md` and the relevant START-HERE files.

## Project goal

Run Microsoft OS/2 2.0 Beta 2 / 6.78-era **32-bit LE Presentation Manager
applications directly on Win32**, translating OS/2 APIs through compatibility
DLLs such as PMWIN, PMGPI, DOSCALLS and PMSHAPI.

This is the **V1 native-Win32 track**. Do not introduce emulation, WHP, V86 or
other V2 mechanisms here. The WHP proof-of-concept exists separately and is
frozen for later. EMX work is also frozen.

## Current frozen baseline

**M31F FINAL = R7.**

R7 is manually regression-green and should be treated as immutable until a new
sample proves a generic compatibility behavior needs to change.

Manual regression suite green together:

- WMCHAR
- HANOI
- BIO
- HELLO + real guest OPENDLG.DLL
- Sarien PM
- JIGSAW with original untouched YOSEMITE.BMP

JIGSAW now loads the old 640x480x4bpp OS/2 1.x bitmap, constructs the puzzle,
draws pieces, drags them at the right coordinates without trails, and repaints
properly after maximize/restore.

## Critical compatibility behaviors not to regress

- Runtime stack promotion to 256 KB for old tiny-stack PM programs.
- Preserve OS/2 class styles; map OS/2 `CS_CLIPCHILDREN` to Win32
  `WS_CLIPCHILDREN`.
- Execute real guest OPENDLG.DLL; do not replace it with a host shim.
- Support prerelease `InitLibrary(hPDLL,hmod)` as well as later
  `DllInitTerm(hmod,flag)` DLL initialization.
- Preserve OS/2 type-5 string tables, Beta-2 FILEFINDBUF behavior and OPENDLG
  redraw/lifecycle handling.
- `GpiQueryBitmapBits` must return RGB/RGB2 palette data so Query -> Set bitmap
  round-trips remain valid (this is what fixed the Sarien blank-window
  regression).
- Translate Win32 child-control notifications to OS/2 `WM_CONTROL`; pushbutton
  clicks and menu/accelerator commands remain `WM_COMMAND` (this is what fixed
  JIGSAW's prematurely dismissed Open dialog).
- Device PS objects must retain their HWND so OS/2 bottom-left Y coordinates can
  be converted using the actual client height. Bitmap/device blits use the
  common height-aware DIB-section/HDC `StretchBlt` path (this fixed JIGSAW drag
  offsets, trails and resize repaint).
- No application-name-specific hacks.

## Known sample notes

- The literal `%%` error in OPENDLG appears to be an original Microsoft SDK
  sample bug. Do not special-case it in os2host32.
- PMEXEC is truly 16-bit segmented LE with selectors/PTR16:16 fixups; leave it
  for V2/WHP.

## What to do next

Choose the next **flat 32-bit LE** Microsoft OS/2 2.0 Beta 2 PM SDK example and
start the next M31 milestone from this exact tree. Read its imports first and
advance one generic API/behavior at a time. Before freezing any new milestone,
rerun the full regression suite above.

If a new change breaks any existing sample, stop feature expansion and fix the
regression architecturally before proceeding.

## Active post-freeze branch: M31G NEKO

M31F FINAL R7 remains the immutable regression baseline.  M31G is an additive
NEKO branch using the untouched OS/2 2.0 GA flat-32-bit LX `NEKO.EXE`.

Progress through M31G R5:

- R1: generic LX PAGE_ITERATED decoding; NEKO maps all three LX objects.
- R2: PMSHAPI.129 = WinRemoveSwitchEntry(HSWITCH).
- R3: PMWIN.837 = WinQueryWindowPos(HWND, PSWP), with the true 32-bit OS/2 SWP
  layout and Win32 top-left -> OS/2 parent-relative bottom-left conversion.
- R4: PMWIN.778 = WinLoadMenu(HWND, HMODULE, ULONG), reusing the existing
  registered RT_MENU parser/compatibility HMENU path without synthetic menus.
- R5: PMWP becomes a native compatibility personality. The real OS/2 2.0 GA
  PMWP.DLL was proven mixed 16/32-bit with selector/far-pointer fixups and WPS
  dependencies, so V1 does not execute it. Only reached ordinal 203 is exposed
  as an ordinal-only four-argument compatibility entry returning full-EAX zero;
  its trustworthy historical public name remains unidentified.

The full M31F static regression chain remains green through R5. NEKO has 63
distinct ordinal imports and 20 remain outside current .def coverage. The next
Win32 run should be captured before further work; do not pre-implement R6 from
static import order alone. If an IBM PMWP.DLL specimen is present, keep it
renamed (for example PMWP-GA.DLL) because PMWP.dll now names the native host
personality.

M31G R6 added PMWIN.780 WinLoadPointer after the R5 runtime proved PMWP.203.
The real R6 Windows run resolved all 26 WinLoadPointer sites plus existing
PMWIN.716/781, then stopped at PMWIN.909.

M31G R7 adds PMWIN.909 WinCreateWindow.  The untouched NEKO call site creates
a desktop WC_STATIC/SS_ICON window at OS/2 (20,20), text "#1", HWND_TOP,
then subclasses it.  R7 implements this generically: common WC_* public-class
translation, parent/screen bottom-left coordinate conversion, z-order mapping,
and WC_STATIC/SS_ICON "#N" lookup through the guest executable's registered
RT_POINTER resource.  Registered private classes keep the existing pm_wndproc
path.  No NEKO name or hard-coded resource-id logic was added.

The next step is to run untouched GA NEKO.EXE on Windows and capture the next
actual boundary before implementing another import.  Note that WinSubclassWindow
is still the pre-existing collapsed/no-op policy; if NEKO eventually reaches
runtime behavior and needs real subclass dispatch, treat that as a separate
proved behavioral boundary rather than folding it speculatively into R7.


M31G R8 adds PMWIN.727 WinDestroyPointer after the real R7 Windows run proved
WinCreateWindow and stopped at ordinal 727. WinLoadPointer-created native
HPOINTER values are now tracked in a small generic ownership registry;
WinDestroyPointer releases only compatibility-owned handles via DestroyIcon and
refuses shared/system handles. No NEKO-specific logic was added. Run untouched
GA NEKO.EXE and capture the next actual boundary before implementing R9.

## M31G R9 delta

R8 runtime-proved PMWIN.727. NEKO's next boundary is PMWIN.804, identified as
WinQueryCapture(HWND_DESKTOP). R9 maps it to GetCapture() plus guest_hwnd(),
paired with the existing WinSetCapture implementation.

A separate flat-32-bit e.exe reaches PMWIN.773, identified as
WinIsWindowEnabled(HWND). R9 adds that generic native HWND state query in the
same PMWIN increment. Neither implementation contains application-specific
behavior. After R9, use the next actual NEKO runtime failure to select R10;
PMSHAPI.102 is the likely next gap from import order, but remains intentionally
unimplemented until confirmed.

## M31G R10 delta

R9 runtime-proved PMWIN.804 for NEKO and PMWIN.773 for e.exe. The next actual
boundaries are PMSHAPI.102 and PMWIN.844 respectively. R10 adds generic
PrfOpenProfile(HAB,PCSZ) with a small private-HINI/path registry feeding the
existing Win32 INI bridge, plus WinQueryWindowUShort(HWND,LONG) for QWS_ID via
the native control/window ID. No NEKO/e.exe-specific behavior was added. Run
both programs and capture their next actual runtime boundaries before R11.


M31G R11: native HELPMGR personality added because OS/2 2.0 GA HELPMGR.DLL is mixed 16/32-bit. Only ordinal 52 WinDestroyHelpInstance(HWND)->BOOL is implemented. HELPMGR.51 and .54 remain intentionally unresolved; follow the next actual loader boundary. If the IBM HELPMGR.DLL specimen is in the run directory, rename it so the Win32 compatibility HELPMGR.dll is loaded.

M31G R12 adds two runtime-proven generic boundaries: PMWIN.884 WinStartTimer for
NEKO, and DOSCALLS.219 DosSetPathInfo for e.exe. WinStartTimer maps to SetTimer and
translates WM_TIMER=0x24 on compatibility window/dialog procedures. DosSetPathInfo
implements the five-argument 32-bit FIL_STANDARD metadata path including zero
packed timestamp pairs meaning leave unchanged. Await the next actual runtime
boundaries; do not prefill later imports.

## M31G R13 - controlled import-complete jump

R12 runtime proved PMSHAPI.117 was next for NEKO and DOSCALLS.212 for e.exe.
Because conversation context was becoming a risk, R13 intentionally implements
all 13 ordinals still absent from the frozen NEKO import set rather than walking
them one-by-one. The NEKO checker now requires zero uncovered imports.

New NEKO coverage: HELPMGR 51/54, PMGPI 399, PMSHAPI 117/118, PMWIN
702/730/737/756/775/822/823/872. The independent e.exe path gains DOSCALLS.212.
All implementations remain generic; no application-name checks were added.

Next action: build PMWIN/PMGPI/PMSHAPI/HELPMGR/DOSCALLS plus os2host32 and run
untouched NEKO. If all imports resolve, stop treating loader order as the work
queue. Capture the first runtime crash/hang/incorrect rendering. NEKO creates a
native WC_STATIC/SS_ICON cat window, calls WinSubclassWindow on it, and uses
WinStartTimer; the current WinSubclassWindow remains the older collapsed/no-op
implementation and is the leading likely behavioral boundary.

## M31G R14 - current active runtime branch

R13 was loader/import complete but the first Windows run produced no visible
cat.  The PM trace proved this is now a runtime-semantics problem:

1. every animation WinLoadPointer used HMODULE 0;
2. the native STATIC cat hwnd received repeated Win32 WM_TIMER 0x0113 messages,
   but no OS/2 WM_TIMER reached the guest because WinSubclassWindow was the
   historical no-op/collapsed implementation;
3. immediately after subclassing NEKO sends static-control SM_QUERYHANDLE 0x101
   and SM_SETHANDLE 0x100.

Re-analysis of the original GA PMWP.DLL supersedes the R5 advisory-stub
assumption: PMWP ordinal 203's successful path calls DOSCALLS.318 DosLoadModule
using arg3 as module name and arg2 as PHMODULE.  NEKO passes "NEKO" and later
uses the written HMODULE for its animation RT_POINTER resources.

R14 therefore:
- bridges PMWP.203 to DOSCALLS.318 without inventing a historical symbolic name;
- lets resource-only guest DLLs publish resources through process PMWIN;
- adds real native-public-control WinSubclassWindow -> pm_wndproc routing while
  preserving the frozen private/managed-window behavior;
- returns a guest-callable public-control old PFNWP with WC_STATIC
  SM_QUERYHANDLE/SM_SETHANDLE support;
- adds DOSCALLS.218 DosSetFileInfo(FIL_STANDARD) for the independent e.exe path.

Before testing NEKO, ensure its DLL is discoverable with normal OS2LIBPATH
semantics, e.g. `set OS2LIBPATH=C:\cl386-research\os2_2.0\x\OS2\APPS;.`.  If
NEKO.DLL fails to load because it is mixed-mode, scan that module and stop; do
not add a NEKO-specific resource shim.  If it loads, expect a nonzero PMWP HMOD,
GUESTMOD resource publication, successful animation WinLoadPointer calls, a
`WinSubclassWindow native` trace, and `WM_TIMER -> guest` for OS/2 message 0x24.

## M31G R15 - e.exe PMWIN.833 + NEKO R14 rerun diagnosis

The real R14 Windows run proves e.exe next reaches PMWIN.833, identified as
WinQueryVersion(HAB). R15 adds that generic one-argument API and advertises the
OS/2 2.0 PM contract (`0x00020000`), rather than exposing the Win32 host version.

NEKO's first R14 run did not reach the subclass/timer behavior: the trace shows
PMWP.203 -> DosLoadModule("NEKO") returned 126 / ERROR_MOD_NOT_FOUND, followed by
NEKO's own WinAlarm error path. Ensure the directory containing NEKO.DLL is on
OS2LIBPATH (for the supplied SDK layout,
`C:\cl386-research\os2_2.0\x\OS2\APPS;.`) and rerun with OS2_PM_TRACE=1. Do not
special-case executable-directory DLL/resource lookup. If NEKO.DLL is found but
cannot be loaded because it needs unsupported mixed-mode machinery, scan that
DLL and stop at that architectural boundary.

## M31G R16 delta

The first R14 NEKO runtime attempt with a discoverable NEKO.DLL proved a loader
bug before any subclass/timer behavior ran.  Historical NEKO.DLL is a flat
32-bit resource-only LX DLL: 3 objects, all preferred base 0, 36 resources,
zero imports and zero fixups.  The mapper had treated preferred base 0 as a
literal Win32 address and also assumed base-derived arena offsets were unique.
R16 makes base 0 relocatable and detects overlapping preferred object ranges;
those modules use distinct 64-KB-aligned packed slots in one relocation arena.
No NEKO-specific branch was added.  R15 PMWIN.833 for e.exe remains present.
Run NEKO with NEKO.DLL on OS2LIBPATH and capture the first behavior after
`GUESTMOD RESOURCES ... published=36`; that should finally exercise the R14
subclass/timer bridge.

## M31G R17 delta

A subsequent R16 NEKO run returned PMWP.203 rc=126 before `GUESTMOD LOAD`.
That means NEKO.DLL was not found; it does not invalidate R16. NEKO.DLL is
resource-only for V1 purposes (entry object 0, zero imports/fixups) and is
loaded only to obtain an HMODULE/resource namespace. The guest loader already
appends `.dll` and searches OS2LIBPATH. OS/2 does not implicitly search cwd
unless `.` appears in LIBPATH, so test with e.g.
`set OS2LIBPATH=.;C:\cl386-research\os2_2.0\x\OS2\APPS` or the real directory
containing NEKO.DLL. Do not add automatic EXE-directory lookup.

E.EXE next reaches NLS.5. R17 adds generic 32-bit
DosQueryCtryInfo(ULONG,PCOUNTRYCODE,PCOUNTRYINFO,PULONG), using the 44-byte OS/2
COUNTRYINFO layout and Win32 current-locale/OEM-codepage data. Foreign explicit
country/codepage requests reject rather than returning incorrect locale data.
R16 zero-base DLL mapping and R15 PMWIN.833 remain intact.

## M31G R18 active state

NEKO is now visibly rendering the historical cat. The successful trace proves:
- NEKO.DLL packed zero-base LX mapping works;
- all 36 resource-DLL resources publish;
- animation WinLoadPointer conversions work;
- native WC_STATIC WinSubclassWindow bridge works;
- 200 ms WM_TIMER is delivered to guest PFNWP 010107BF;
- the cat image itself is correct.

The remaining near-invisibility was traced to WinSetWindowPos move-only
semantics. The bridge used cx/cy even without SWP_SIZE. NEKO's 32x32 cat moved
from requested OS/2 y=20 to queried y=-12, exactly -32. R18 initializes the
coordinate conversion from the current native window dimensions and only uses
requested cx/cy when SWP_SIZE is set. WinSetWindowPos now also traces flags,
native position and effective height.

Next run should focus on cat motion/visibility. If still wrong, capture the first
few WinSetWindowPos + WinQueryWindowPos pairs; do not revisit resource loading,
subclassing or timer delivery unless those explicit checkpoints regress.

## M31G R19 active state

R18 is visually proven: OS/2 2.00 GA NEKO animates its original cat frames on
Windows 10 x64. The original resource DLL mapping, HMODULE resource namespace,
public-control subclass bridge, timers and move-only coordinate conversion are
all working.

R19 adds two generic fixes exposed by that success:
- WinBeginEnumWindows(HWND_DESKTOP) filters the real Win32 desktop to windows
  owned by the current os2host32 process; WinShowWindow rejects foreign-process
  HWNDs and cannot hide the native desktop.
- WC_SLIDER (resource/public class 38) maps to a Win32 trackbar. The bridge
  implements SLM_QUERYSLIDERINFO 0x036C, SLM_SETSLIDERINFO 0x0371,
  SLM_SETTICKSIZE 0x0372, arm position/size, ticks, and translates native
  slider changes/tracking into OS/2 WM_CONTROL notifications.

Next runtime test: verify NEKO's three setup sliders render and move, then test
~Hide cautiously. Explorer/unrelated applications must remain untouched; only
os2host32-owned guest PM windows may be hidden.

## M31G R19 FINAL — runtime proven, freeze here

User runtime confirmation on Windows 10 x64 completes the M31G NEKO milestone.
The untouched OS/2 2.00 GA Cat and Mouse application now visibly animates its
original cat frames, using its original NEKO.DLL resource module. The Cat
Set-up Control Panel renders working Play time / Speed / Step sliders, and the
historical `~Hide` feature no longer hides Explorer or unrelated native Windows
applications. The prior PM regression applications were also manually reported
working after the R19 changes.

Treat **M31G R19 FINAL as frozen/proven**. Do not reopen NEKO implementation
unless a concrete regression appears.

Key final fixes:
- R16: generic packed relocation for zero/overlapping-base LX DLL objects;
- R14/R16: NEKO.DLL HMODULE/resource publication, public-control subclassing,
  static SM_QUERYHANDLE/SM_SETHANDLE, guest WM_TIMER delivery;
- R18: correct move-only WinSetWindowPos bottom-left/top-left conversion using
  current window size when SWP_SIZE is absent;
- R19: WC_SLIDER -> Win32 trackbar with SLM 036C/0371/0372 and WM_CONTROL;
- R19: HWND_DESKTOP enumeration/show isolation to current os2host32 process so
  guest PM code cannot hide arbitrary host windows.

Full chained check: `make m31g-neko-r19-check` passes.
See `MILESTONE31G-FINAL.md` for the concise frozen architecture and restart
instructions.

The separate E.EXE exploration is not part of the M31G freeze. R17 added its
last known runtime boundary NLS.5 / DosQueryCtryInfo. If E work resumes, rerun
untouched E.EXE and follow its next actual runtime failure.
