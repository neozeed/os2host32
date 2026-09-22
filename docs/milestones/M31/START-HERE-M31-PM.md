# Start here — Milestone 31 Presentation Manager

This is the handoff note for a new conversation.  Use this **Milestone 30 FINAL**
source tree as the starting point.  Milestone 30 EMX work is frozen; the active
next goal is to grow native Presentation Manager compatibility one SDK sample
at a time.

## Current PM foundation

The tree already builds host-backed PM compatibility DLLs:

* `PMWIN.dll` from `pmwin.c` / `pmwin.def`
* `PMGPI.dll` from `pmgpi.c` / `pmgpi.def`

Current PMWIN ordinal surface:

* WinBeginPaint       @703
* WinCreateMsgQueue   @716
* WinDestroyMsgQueue  @726
* WinDestroyWindow    @728
* WinEndPaint         @738
* WinFocusChange      @746
* WinInitialize       @763
* WinOpenWindowDC     @794
* WinQuerySysValue    @829
* WinSetWindowPos     @875
* WinTerminate        @888
* WinCreateStdWindow  @908
* WinDefWindowProc    @911
* WinDispatchMsg      @912
* WinGetMsg           @915
* WinRegisterClass    @926

Current PMGPI ordinal surface:

* GpiBitBlt                 @355
* GpiCreatePS               @369
* GpiSetBackMix             @505
* GpiSetBitmap              @506
* GpiSetColor               @517
* GpiSetPel                 @544
* GpiCreateLogColorTable    @592
* GpiCreateBitmap           @598
* GpiQueryBitmapBits        @599
* GpiQueryBitmapInfoHeader  @601
* GpiSetBitmapBits          @602
* DevOpenDC                 @610

These exports reflect the compatibility surface already used by the existing PM
proofs; they are not meant to be an exhaustive PM implementation.

## Proven PM result

Before the EMX detour, the project successfully ran a real OS/2 Presentation
Manager program (Sarien PM) through `os2host32` on 64-bit Windows 10.  This is
important context: M31 is **not** a first-PM bring-up.  The architecture has
already displayed a real PM application; M31 should turn that success into a
systematic compatibility suite.

## Recommended M31 method

Use the Microsoft OS/2 2.0 Beta 2 SDK headers/libraries and examples already
recovered by the project.  Start with the smallest PM example and proceed in
increasing complexity.

For every SDK sample:

1. build the sample as its intended OS/2 LE/LX executable;
2. run `os2host32 --scan sample.exe` first;
3. inventory its imports by module, ordinal and name;
4. compare them against `PMWIN.def`, `PMGPI.def` and existing personality DLLs;
5. add the minimum missing behavior needed by that sample;
6. run the sample and verify visible behavior, not merely successful linkage;
7. keep the sample as a regression before moving to the next one.

Prefer implementing actual PM semantics over application-specific shims.
Preserve ordinal compatibility because these historical programs often import
by ordinal.

## Suggested first PM progression

A good order is roughly:

1. PM initialization / message queue / terminate only;
2. class registration + a single standard window;
3. message loop + WM_PAINT;
4. BeginPaint/EndPaint and basic GPI drawing;
5. window sizing/focus/system-value queries;
6. menus, controls and resources;
7. dialogs;
8. bitmap/GPI examples;
9. increasingly complex SDK applications.

Let the actual SDK example set determine the precise order; do not pre-implement
large PM areas without a test demanding them.

## Things to preserve while modifying PM

* Do not regress the C/386 direct OS/2 backend or far16 VIO/KBD bridge.
* Keep guest DLL/module lifecycle behavior from M29N2 intact.
* Keep PM compatibility in `PMWIN.dll` / `PMGPI.dll` where practical rather
  than burying PM-specific behavior in the loader.
* Maintain Win32-native execution for V1.
* Keep tracing useful and opt-in; PM message tracing will probably be valuable
  once examples become non-trivial.
* Add regression commands/scripts alongside each milestone.

## Useful diagnostic pattern

For a new example, capture:

    os2host32 --scan example.exe
    os2host32 --run  example.exe

If it imports a new guest DLL, remember M29N2 already provides recursive guest
DLL loading and lifecycle support; do not reimplement that path for PM.

## M30/EMX reminder

The sidecarless EMX inference code and M30M4 diagnostics are intentionally
retained in the source tree, but M31 should not depend on them.  EMX was an
architecture probe and compatibility-expansion exercise, not the prerequisite
for PM examples.

If a future PM example contains real 16-bit protected-mode code that cannot be
handled by the existing migration/far16 bridge, record it as a candidate for
the future V2 WHP execution layer rather than redesigning V1 around it.

## What to give a new conversation

Upload this final ZIP and say, approximately:

> This is Milestone 30 FINAL of os2host32.  M30 EMX research is frozen.  Read
> `MILESTONE30-FINAL.md` and `START-HERE-M31-PM.md`.  We are starting M31 and
> want to work through the Microsoft OS/2 2.0 Beta 2 Presentation Manager SDK
> examples one at a time, preserving each as a regression test.  Sarien PM has
> already run through this architecture, so extend the existing PMWIN/PMGPI
> implementation rather than designing a new PM layer.

Then provide the first SDK sample / source bundle.
