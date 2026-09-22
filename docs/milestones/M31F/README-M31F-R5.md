# M31F R5 - JIGSAW deep GPI trace pass

R4 is manually regression-green on Win32: WMCHAR, HANOI, BIO, HELLO/OPENDLG
and Sarien all run.  JIGSAW is therefore unfrozen again.

R5 deliberately does **not** change guest-visible PM/GPI semantics.  It extends
tracing around the exact area where the JIGSAW worker was known to enter
`CalcTransform()` and fail to reach the final `Redraw()`.

The original `YOSEMITE.BMP` remains untouched.  It is the 640x480x4 OS/2 1.x
core-header bitmap shipped with the sample.

## Added diagnostics

With `OS2_PM_TRACE=1` PMGPI now reports:

- thread ID, HDC, WindowFromDC result, requested PS size and flags from
  `GpiCreatePS`;
- every `GpiSetBitmap` attach/detach and its bitmap geometry;
- bitmap palette samples after `GpiSetBitmapBits` consumes an information
  table (particularly useful for Yosemite's 16-entry OS/2 1.x palette);
- source/destination bitmap geometry and thread ID on every `GpiBitBlt`;
- explicit success/failure, Win32 error value, and geometry for the `PatBlt`
  ROP_ZERO/ROP_ONE mask-building path and for the general `StretchBlt` path;
- path Begin/End, spline and clip-path success/failure plus Win32 error value;
- first/last converted points from `GpiConvert`;
- the resulting default-view matrix after `GpiSetDefaultViewMatrix`.

This should identify the first operation that fails or does not return during
scaled shadow-bitmap and 36-piece mask generation without modifying JIGSAW.

## Runtime test

Build as usual, then run:

    m31f-jigsaw-r5-test.cmd

Open `YOSEMITE.BMP`.  If JIGSAW stalls, leave it for only long enough to make
that state obvious, then close/terminate it so the batch file can finish.
The useful artifact is `jigsaw-r5-err.txt`.

Run `python tools/summarize_m31f_jigsaw_trace.py jigsaw-r5-err.txt` for a compact
summary, but preserve the full log for diagnosis.
