# Milestone 31A — WMCHAR first Presentation Manager SDK regression

This tree starts from the frozen Milestone 30 FINAL checkpoint.  The EMX work
remains frozen.  M31A targets the original Microsoft/IBM OS/2 2.0 Beta 2
`WMCHAR` sample supplied with the recovered SDK.

The untouched SDK sample is preserved under:

    examples/m31a-wmchar/

including the historical `WMCHAR.EXE` used by the regression.

## Historical executable inventory

`os2host32 --scan examples/m31a-wmchar/WMCHAR.EXE` reports a flat 32-bit LE
program with four imported modules:

    PMGPI
    PMWIN
    PMSHAPI
    DOSCALLS

There are 34 unique ordinal imports.  The M31A compatibility DEF files cover
all 34.  DOSCALLS required no new entry points.

New PMWIN surface demanded by WMCHAR:

    WinQueryWindow         @834
    WinWindowFromID        @899
    WinQueryWindowProcess  @838
    WinQueryWindowRect     @840
    WinQueryWindowText     @841
    WinReleasePS           @848
    WinDrawText            @913
    WinScrollWindow        @849
    WinSendMsg             @920
    WinFillRect            @743
    WinGetPS               @757
    WinInvalidateRect      @765

New PMGPI surface:

    GpiQueryFontMetrics    @453

New PMSHAPI surface:

    WinAddSwitchEntry      @120

## Architectural change: shared HPS representation

The Sarien-era PM layer was sufficient when the application created its own GPI
presentation space, but `WMCHAR` calls both:

    WinGetPS(HWND_DESKTOP)
    WinBeginPaint(hwnd, NULL, &rcl)

The old `WinBeginPaint(hwnd, NULL, ...)` simply returned NULL.  M31A introduces
`pmcompat.h`, a private shared representation for HPS values created by PMWIN
and PMGPI.  This lets:

* WinGetPS create a usable HPS;
* WinBeginPaint create a paint HPS when the caller supplies NULL;
* WinFillRect / WinDrawText use that HPS;
* GpiQueryFontMetrics consume an HPS created by PMWIN;
* the existing Sarien GpiCreatePS path continue to use the same representation.

The loader does not know about HPS objects; this remains inside the PM
personality DLLs.

## WM_CHAR bridge

The Win32 window procedure now constructs the Beta-2 WM_CHAR layout used by
`WMCHAR.C`:

* SHORT1FROMMP(mp1): KC_* flags;
* CHAR3FROMMP(mp1): repeat count;
* CHAR4FROMMP(mp1): scan code;
* SHORT1FROMMP(mp2): character value;
* SHORT2FROMMP(mp2): OS/2 virtual-key value.

Navigation keys, modifiers and F1-F24 are mapped to the Beta-2 VK_* values.
The small historical Sarien F1-F12 low-word compatibility extension is retained
while the authentic OS/2 virtual key remains in the high word.

## PMSHAPI

WMCHAR uses `WinAddSwitchEntry`.  On native Windows the top-level window already
participates in the task switcher, so the first PMSHAPI personality accepts the
registration and returns a nonzero switch handle.  `PMSHAPI` has also been
added to the loader's host-personality module list.

## First-run target

Build on the normal i686 MinGW Windows environment:

    make clean
    make tools compat

The strict import-only regression can be run first with:

    make m31a-wmchar-scan-check

Then run the GUI test:

    m31a-wmchar-test.cmd

or directly:

    os2host32 --scan examples\m31a-wmchar\WMCHAR.EXE
    os2host32 --run  examples\m31a-wmchar\WMCHAR.EXE

Expected first-pass visible behavior:

1. a window titled `Char Messages` opens;
2. the headings paint on a normal window background;
3. typing printable characters adds rows;
4. arrows and function keys show OS/2 virtual-key values and KC_* flags;
5. scan code and repeat information are carried in mp1;
6. the program exits cleanly when the native window is closed.

## Deliberate M31A boundary

`WinCreateStdWindow` still does not decode the guest executable's OS/2 menu/icon
resource templates.  Therefore the WMCHAR resource menu is not yet materialized
as a native Windows menu in this first pass.  `WinWindowFromID` and `WinSendMsg`
have general native-menu handling ready for when resource creation is added,
but the Actions/Display menu is not a pass criterion for M31A.

That is intentional: the first regression is the PM paint/HPS/WM_CHAR path.
Do not hard-code WMCHAR's menu into PMWIN.  A later slice should add general
OS/2 resource/menu translation and then promote the menu interactions into the
WMCHAR regression.

## Validation performed in this handoff

The host-independent `os2host32` scanner builds successfully and scans the
preserved historical executable.  A generated import-coverage check confirms
that every ordinal imported by WMCHAR is present in the corresponding M31A
compatibility DEF file.  PMWIN, PMGPI and PMSHAPI also pass a C89 syntax-only
check against a local Win32 API shim.

The actual Win32 DLL build and GUI execution still need to be run on the i686
MinGW Windows machine, because this handoff environment does not contain the
MinGW Win32 headers/toolchain or a Windows runtime.

## M31A R2 — LINK386 biased internal OFF32 relocation

The first native Windows run exposed one loader assumption before PM execution
began.  WMCHAR contains a LINK386 source-list internal OFF32 relocation to
object 2 with target offset `FFFFF9F0`.  With object 2's preferred base of
`00020000`, the stored address constant is `0001F9F0`; both source sites in the
historical executable contain exactly that value.

This is a biased address constant used by scaled indexed machine instructions,
not a standalone pointer which must itself fall inside object 2.  M30 rejected
it because `apply_fixups()` required every internal target offset to be less
than the target object's size.

M31A R2 removes that invalid object-bound assumption for flat internal OFF32
relocations.  The target is rebased using modulo-32-bit address arithmetic, so
when object 2 moves to `01020000`, the constant becomes `0101F9F0`.

A host-independent regression now preserves this linker behavior:

    make m31a-wmchar-fixup-check

and the combined M31A gate is:

    make m31a-wmchar-check
