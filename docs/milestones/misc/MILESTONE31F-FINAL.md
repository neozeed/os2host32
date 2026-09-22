# M31F FINAL - JIGSAW / Presentation Manager expansion

Date frozen: 2026-09-18
Source baseline: M31F R7
Track: V1 native Win32 OS/2 compatibility layer

## Status

M31F R7 is manually regression-green and is the new frozen known-good baseline.
Do not make further speculative changes to JIGSAW or the bitmap layer without a
specific regression or a new SDK sample proving a missing OS/2 compatibility
behavior.

This branch is strictly the V1/native-Win32 track. Do not introduce WHP,
emulation, V86, segmented-CPU emulation, or other V2 mechanisms here.
EMX work remains frozen.

## Proven regression suite

The following programs were manually verified working together on R7:

- WMCHAR - 32-bit LE PM character/input example.
- HANOI - menus, icons, accelerators and animated graphics.
- BIO - dialog, graph, legend, resizing and class-style behavior.
- HELLO + the real historical 32-bit guest LE OPENDLG.DLL - DLL init,
  DLL-owned resources, exported functions, dialogs, directory enumeration and
  Open/Save behavior.
- Sarien PM - 320x200x8 framebuffer, palette round-trip, bitmap upload and
  presentation.
- JIGSAW - original untouched YOSEMITE.BMP loads, puzzle pieces are generated,
  masked/clipped, displayed, draggable without trails, and repaint correctly
  after maximize/restore.

Regression screenshots are under `regression-evidence/`.

## Historical prerequisites that remain part of the baseline

### M30 FINAL

Established the native 32-bit LE loader/runtime.

### M31A FINAL - WMCHAR

Established the 256 KB runtime stack promotion required by old PM applications
that carry tiny historical stack reservations.

### M31C FINAL - HANOI

Established menus, icons, accelerators and animated GPI behavior.

### M31D FINAL / R8 - BIO

Important compatibility behavior includes preserving OS/2 class styles and
mapping OS/2 `CS_CLIPCHILDREN` to Win32 `WS_CLIPCHILDREN`.

### M31E R4 - OPENDLG

Major milestone: HELLO executes the real historical 32-bit guest LE
OPENDLG.DLL. Do not replace OPENDLG with a host shim.

Prerelease DLLs such as OPENDLG may use the older INITINSTANCE ABI:

    InitLibrary(hPDLL, hmod)

rather than the later:

    DllInitTerm(hmod, flag)

The loader/runtime supports both. OPENDLG also established OS/2 type-5 string
tables, Beta-2 FILEFINDBUF behavior, redraw semantics and INIT-only DLL
termination handling.

The literal `%%` error in OPENDLG is believed to be an original SDK/sample bug;
do not add an os2host32 application-specific workaround for it.

## M31F chronology and generic fixes

### R1/R2 - broad JIGSAW surface and tracing

JIGSAW is entirely flat 32-bit LE and imports 99 ordinals. Its original bitmap
is an OS/2 1.x 640x480x4bpp bitmap (`cbSize 22602`, bits offset 74).

R1 added a broad PM/GPI surface but regressed Sarien, so M31E R4 remained the
known-good baseline until the regression was fixed. R2 was primarily tracing.

### R4 - Sarien palette round-trip fix

M31F taught `GpiSetBitmapBits` to consume a supplied bitmap palette. Sarien
uses `GpiQueryBitmapBits` to prepare a BITMAPINFO2 and then reuses that structure
with `GpiSetBitmapBits`. The compatibility `GpiQueryBitmapBits` implementation
had returned scan data but not the RGB/RGB2 table, so Sarien later consumed an
empty palette and rendered black.

The architectural fix was to make `GpiQueryBitmapBits` return the bitmap
information table including palette data for both OS/2 1.x RGB and OS/2 2.x
RGB2 forms. This restored proper Query -> Set round-tripping and preserved the
palette handling needed by old 1.x bitmaps such as YOSEMITE.BMP.

### R6 - PM child-control message translation fix

R5 tracing showed JIGSAW was not even opening YOSEMITE.BMP: its cheap Open
File dialog was dismissed during `WM_INITDLG`, leaving file handle zero.

Win32 delivers child-control notifications through native `WM_COMMAND`, while
OS/2 PM expects child-control notifications through `WM_CONTROL`. The bridge
was incorrectly forwarding edit-control notifications as OS/2 `WM_COMMAND`.
JIGSAW treats non-OK commands as dialog dismissal.

The generic fix:

- child-control notifications -> OS/2 `WM_CONTROL`;
- pushbutton clicks -> OS/2 `WM_COMMAND`;
- menu/accelerator commands remain `WM_COMMAND`;
- existing listbox notification mapping remains intact.

After this fix the untouched YOSEMITE.BMP opened and JIGSAW constructed the
puzzle.

### R7 - device-PS height / OS/2-to-Win32 Y-coordinate fix

R6 displayed the puzzle but dragging had an offset, left trails, and maximize
could clear the window without visibly repainting the puzzle.

The trace showed an OS/2 rectangle `(258,98)..(425,225)` being presented to a
Win32 device at native `y=-98`. For a 317-pixel-high client the correct Win32
top-left Y is:

    317 - 225 = 92

The problem was that `GpiCreatePS` could recover the native HWND from the HDC
but did not retain that HWND in `CompatPS`. Consequently the device surface
height was unknown during bottom-left -> top-left conversion.

R7 fixes this generically:

- `GpiCreatePS` stores `WindowFromDC(p->dc)` in the compatibility PS.
- `GpiAssociate` refreshes the stored HWND when the HDC changes.
- device surface height comes from the actual client rectangle.
- the temporary R3 screen-only direct-DIB SRCCOPY path is removed.
- bitmap-to-bitmap and bitmap-to-device use the common DIB-section/HDC
  `StretchBlt` path with correct source/destination surface heights.
- the R4 Sarien palette round-trip behavior remains intact.

This fixed JIGSAW dragging, cleanup/trails and resize/maximize repaint while
keeping Sarien green.

## JIGSAW-specific result, without JIGSAW-specific hacks

The original SDK file is used unchanged:

    YOSEMITE.BMP
    OS/2 1.x bitmap
    640 x 480 x 4 bpp
    cbSize 22602
    bits offset 74

The compatibility fixes are generic OS/2 PM/GPI behavior. There are no
application-name checks for JIGSAW or Sarien.

## Deferred sample

PMEXEC was inspected and is genuinely 16-bit segmented LE code using selectors
and PTR16:16 fixups. It is intentionally deferred to the future V2/WHP track.
Do not pull PMEXEC into V1.

## Next work

Start the next 32-bit flat LE Microsoft OS/2 2.0 Beta 2 Presentation Manager SDK
sample from this M31F FINAL tree. Continue the established process:

1. scan/import-audit the sample;
2. add the sample unchanged under `examples/`;
3. implement only generic missing OS/2 compatibility behavior;
4. preserve every successful sample as a regression;
5. rerun WMCHAR, HANOI, BIO, HELLO/OPENDLG, Sarien and JIGSAW before freezing
   the next milestone.

Do not reopen EMX or V2/WHP work in this branch.
