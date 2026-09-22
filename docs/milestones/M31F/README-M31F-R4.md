# M31F R4 - repair GpiQueryBitmapBits / GpiSetBitmapBits palette round-trip

R4 continues the regression-first work.  M31E R4 remains the frozen known-good
baseline until the Win32 regression suite is manually green again.

## Root cause found from the real Sarien PM source

Sarien's FastGPI-derived path creates an 8-bpp memory bitmap, establishes its
logical CGA palette with `GpiCreateLogColorTable`, writes one pel for each color,
then calls `GpiQueryBitmapBits` with an allocated `BITMAPINFO2`.  It later reuses
that same information table in repeated `GpiSetBitmapBits` calls.

M31E ignored the information table on both Query and Set, so that worked by
accident.  M31F began correctly consuming the RGB table in `GpiSetBitmapBits`,
but `GpiQueryBitmapBits` still failed to return the RGB table.  The caller's
allocated RGB2 entries therefore remained zero and M31F subsequently replaced
the established logical palette with black entries.  The application continued
to run and blit, but its framebuffer appeared blank.

## R4 compatibility rule

`GpiQueryBitmapBits` now returns bitmap information as well as scan data:

- OS/2 1.x `BITMAPINFO` (`cbFix == 12`) gets its 12-byte header and RGB table;
- OS/2 2.x `BITMAPINFO2` gets the BITMAPINFOHEADER2 fields and RGB2 table;
- the bitmap's compatibility palette is used as the returned table.

This makes the generic OS/2 Query -> modify -> Set round trip coherent.  It is
not keyed to Sarien and it preserves M31F's old-bitmap palette handling needed
by JIGSAW/Yosemite.

R3's three-point memory-bitmap -> device SRCCOPY compatibility blit remains in
place.

## Verification order

Build normally, then test Sarien first.  With `OS2_PM_TRACE=1`, look for both:

    PMGPI: GpiQueryBitmapBits OK ...
    PMGPI: GpiBitBlt DIB-SRCCOPY OK ...

If Sarien draws again, re-run WMCHAR, HANOI, BIO and HELLO/OPENDLG before
resuming JIGSAW.
