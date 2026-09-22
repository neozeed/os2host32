# M31F R3 - restore three-point bitmap SRCCOPY compatibility

R3 is a regression-first correction on top of M31F R2.  M31E R4 remains the
frozen known-good baseline until the full Win32 regression suite is manually
re-verified.

## Why this exists

M31F R1 replaced the earlier buffer-backed `GpiBitBlt` presentation path with a
native DIB-section/HDC `StretchBlt` path for all bitmap transfers.  That was too
broad a semantic change: the established PM framebuffer pattern used by the
pre-existing Sarien regression still runs its window/message loop but its drawn
client becomes blank.

That older pattern is generic OS/2 PM behavior:

1. create an 8-bpp memory bitmap and associate it with a memory HPS;
2. update the compatibility bitmap with `GpiSetBitmapBits`;
3. present it with a three-point `GpiBitBlt(..., ROP_SRCCOPY, ...)`.

## R3 rule

`GpiBitBlt` now has two deliberately separate paths:

- **three-point memory-bitmap -> device `ROP_SRCCOPY`** restores the proven
  pre-M31F `StretchDIBits` path, including its historical Y conversion and
  direct use of the compatibility bitmap buffer/palette;
- **bitmap-to-bitmap, four-point/scaled transfers, masks and other ROPs** keep
  the M31F native DIB-section/HDC `StretchBlt` implementation needed by JIGSAW.

This is not keyed to an application name and does not modify guest data.

## Regression guard

`tools/check_m31f_sarien_gpi_regression.py` verifies that the compatibility
SRCCOPY path remains present *before* the generalized native path.  It is wired
into both `m31f-jigsaw-static-check` and `m31f-jigsaw-check`.

## Win32 verification order

Build normally, then verify in this order before doing any more JIGSAW work:

    make clean
    make tools compat
    make m31f-jigsaw-static-check

    rem Existing manual regressions
    WMCHAR
    HANOI
    BIO
    HELLO / real OPENDLG.DLL
    Sarien PM

For Sarien with tracing enabled, the expected presentation marker is:

    PMGPI: GpiBitBlt DIB-SRCCOPY OK ...

Only after all frozen regressions are green should JIGSAW/Yosemite debugging
resume.
