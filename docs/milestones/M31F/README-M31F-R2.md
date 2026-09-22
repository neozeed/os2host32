# M31F R2 - JIGSAW bitmap/piece-preparation trace pass

R1 reaches the async UM_LOAD worker, accepts the original 640x480x4 OS/2
YOSEMITE.BMP, creates the main off-screen bitmaps, and enters CalcTransform().
The worker does not return to the final Redraw(), so R2 adds focused tracing
rather than changing guest-visible behavior.

## New trace coverage

With OS2_PM_TRACE=1:
- GpiCreateBitmap: OS/2 header form, dimensions, planes/bpp, stride/size
- GpiSetBitmapBits: scan count, header form and copied byte count
- GpiSetDefaultViewMatrix / GpiConvert
- GpiBeginPath / GpiEndPath / GpiSetClipPath / GpiPolySpline
- GpiBitBlt geometry and result

With OS2_TRACE_IO=1:
- DosOpen path/full path, flags and resulting HFILE/size
- DosQueryFileInfo level/size
- existing DosRead tracing

This pass is deliberately diagnostic: it should tell us exactly where the
36-piece mask preparation stops without modifying the untouched SDK JIGSAW.
