# M31F FINAL STATUS

**R7 is manually verified FINAL and is the frozen baseline.** The original
untouched `YOSEMITE.BMP` loads; puzzle pieces render, drag at correct positions
without trails, and redraw after maximize/restore. WMCHAR, HANOI, BIO,
HELLO/real OPENDLG.DLL and Sarien are also green on the same R7 tree.

See `MILESTONE31F-FINAL.md` before making further changes.

---

# Start here - M31F JIGSAW

M31F starts from the proven M31E R4 tree and adds the untouched OS/2 2.0 Beta
2 JIGSAW sample under `examples/m31f-jigsaw`.

Quick test:

    make clean
    make tools compat
    make m31f-jigsaw-check
    m31f-jigsaw-r1-test.cmd

The first runtime goal is not perfect puzzle rendering in one jump.  The useful
checkpoints are: main frame/menu/status dialog, async worker queue staying
alive, Open loading `YOSEMITE.BMP`, 4-bpp bitmap creation, and then path/spline
piece generation.  Keep WMCHAR, HANOI, BIO and OPENDLG as regressions.

## Current regression status / R6 file-open fix

R4 is manually regression-green for WMCHAR, HANOI, BIO, HELLO/OPENDLG and
Sarien.  R5 was diagnostic-only and proved that JIGSAW was not reaching the
bitmap parser: the cheap Open dialog was dismissed during `WM_INITDLG`, leaving
`LOADINFO.hf == 0`, after which the worker failed its first
`DosQueryFileInfo`.

R6 corrects generic PM dialog message translation.  Win32 child-control
notifications now become OS/2 `WM_CONTROL`; pushbutton clicks and menu commands
remain `WM_COMMAND`.  This specifically prevents entry-field focus/configure
notifications from being mistaken for dialog commands, without any
JIGSAW-specific behavior.  R5's deep GPI tracing remains enabled.

Run `m31f-jigsaw-r6-test.cmd`, open the untouched `YOSEMITE.BMP`, and preserve
`jigsaw-r6-err.txt` if loading proceeds to a later failure.

## R7 - device-PS height and unified bitmap blits

R6 achieved the major runtime milestone: the untouched Yosemite bitmap loads,
the puzzle is generated, and JIGSAW displays real pieces.  Manual regression
checks also kept HANOI and Sarien green.

The next R6 trace exposed a device-PS coordinate error rather than a puzzle or
bitmap-decoding error.  `GpiCreatePS` could identify the native HWND for trace
output but did not save it into the compatibility PS, so screen `GpiBitBlt`
operations did not know the real client height needed to translate OS/2
bottom-left Y coordinates to Win32 top-left Y coordinates.  A requested drag
rectangle `(258,98)..(425,225)` was therefore presented at native y=-98.

R7 stores the HWND in `GpiCreatePS`/`GpiAssociate` and removes the temporary R3
screen-only direct-DIB SRCCOPY path.  All native bitmap blits now use the same
surface-height-aware DIB-section/HDC `StretchBlt` path.  The R4 Sarien palette
round-trip fix remains unchanged.

Run `m31f-jigsaw-r7-test.cmd`; load `YOSEMITE.BMP`; test dragging, trail cleanup,
and maximize/restore.  Preserve `jigsaw-r7-err.txt` on failure.  R7 remains
experimental until JIGSAW and the existing regression suite are manually green.
