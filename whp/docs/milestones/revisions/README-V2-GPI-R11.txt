WHP OS/2 V2 R11 - PMGPI bitmap test
=================================
R11 FIX 1: corrected gpi1.c's palette-buffer declaration. The supplied Beta2
BITMAPINFO2 has no trailing RGB2 entry, so the original wrapper allocated only
15 entries and failed its sizeof check before the first PMGPI call. The test
now declares a BITMAPINFOHEADER2 followed by 16 RGB2 entries explicitly, with
a compile-time 128-byte size check. SDK C89 checking passed for this fix.
Only rebuild the guest: build-c386-gpi.cmd, then run-gpi.cmd.
The host source/executable does not need rebuilding for this correction.

Cumulative source package based on R10. The user confirmed R10 live on WHP:
  pmhello PASS (5 paints, 3 character messages)
R11 adds the first PMGPI bitmap path. No new guest or Windows host executable
is prebuilt; build with your existing MSVC and C/386 environments.

BUILD AND RUN
  build-v2.cmd                  (x64 MSVC developer prompt)
  build-c386-gpi.cmd             (Microsoft C/386 environment)
  run-gpi.cmd                   (Windows WHP)

Guest library directory defaults to \c386\lib; override C386LIB if needed.
Uses the original os2.h declarations and os2386.lib/libc.lib, with the same
LINK386 response-file build as R10. No resource compiler is needed.
Keep the resulting gpi1.exe beside whp_os2_v2_hi.exe.

The window should show:
  top-left RED, top-right GREEN, bottom-left BLUE, bottom-right YELLOW;
  a white L-shaped marker at the TOP LEFT;
  one red source pixel at the BOTTOM LEFT (the explicit GpiSetPel test).
Resize the window: the image should scale to fill it, with square-edged
nearest-neighbor pixels. Uncover it to check repainting. Close it or press
Escape after viewing the image. Expected console:
  gpi1 PASS (<n> paints; bitmap readback, copy and cleanup)

Short automatic smoke test:
  run-gpi.cmd --auto
This performs the same data checks, paints once, then closes itself. It does
not check what the displayed colors look like; do the interactive run too.
Diagnostics go to gpi1-trace.txt, overwritten on each run.

The earlier focused regressions remain available:
  run-pmhello.cmd --auto
  run-callback.cmd
  run-sync.cmd
No long torture batch is needed for this graphics increment.

WHAT THE GUEST TEST CHECKS
- Opens two memory DCs and associated memory presentation spaces.
- Builds a 67x48, 4-bpp bitmap: odd width and DWORD-padded 36-byte scanlines.
- Uploads pixels and all 16 RGB2 palette entries through GpiCreateBitmap and
  GpiSetBitmapBits. The non-square source and corner marker expose flips.
- Switches one PS to direct RGB colors and changes its bottom-left pixel
  with GpiSetColor/GpiSetPel; exercises GpiSetBackMix(BM_LEAVEALONE).
- Reads the bitmap and palette back and checks every byte/color.
- Queries the BITMAPINFOHEADER2 and checks dimensions and bit depth.
- Copies between the two memory PSs with GpiBitBlt and compares readback.
- Confirms deletion of a selected bitmap fails, and selecting that bitmap
  into a second PS fails without losing the second PS's original bitmap.
- Uses GpiBitBlt from the memory PS into WinBeginPaint's window PS, scaled to
  the full client rectangle. Ends paint, destroys the window, deselects and
  deletes both bitmaps, destroys both PSs and closes both DCs.

IMPLEMENTED PMGPI ORDINALS
  355 GpiBitBlt               369 GpiCreatePS
  371 GpiDeleteBitmap         379 GpiDestroyPS
  505 GpiSetBackMix           506 GpiSetBitmap
  517 GpiSetColor             544 GpiSetPel
  592 GpiCreateLogColorTable  598 GpiCreateBitmap
  599 GpiQueryBitmapBits      601 GpiQueryBitmapInfoHeader
  602 GpiSetBitmapBits        604 DevCloseDC
  610 DevOpenDC
These cover the bitmap-related imports in the supplied Sarien scan, plus
resource cleanup. Matching an import does not yet imply all its modes work.

IMPLEMENTATION
v2_gpi.h owns bitmap storage and opaque 32-bit guest handles. Native pointers,
HDCs and BITMAPINFO structures never appear in guest memory. The host decodes
12-byte OS/2 bitmap headers with packed RGB triples and 64-byte headers with
RGB2 entries explicitly. One-plane 1/4/8/24/32-bpp uncompressed bottom-up
images are supported, including DWORD row padding and partial row transfers.

GpiBitBlt takes a source snapshot, allowing overlapping copies, converts
palette entries to pixels, and either updates a memory bitmap or submits a
32-bpp top-down DIB to the native paint DC. Target coordinates are converted
from OS/2 bottom-left to Windows top-left. Scaling uses nearest neighbors.
Memory targets with a different palette choose the closest RGB entry.

There are 16 slots each for DCs, PSs and bitmaps, with new handle values on
slot reuse within a PM session. Bitmap dimensions are limited to 2048x2048;
total resident bitmap storage is limited to 32 MiB. Range, format, palette,
selection and resource-lifetime checks precede accesses. WinTerminate and
process cleanup release abandoned GPI allocations as well as PM resources.

The R10 native WM_NCCREATE path now calls DefWindowProcA after installing the
runtime pointer. This lets Windows perform default creation work, including
caption initialization, to address the blank title observed in R10. Please
check that the R11 title now appears during the Windows run.

CURRENT BOUNDARIES
- Same single PM owner thread and standard window pair as R10. Cross-thread
  GPI use is not implemented. PMWIN APIs outside R10 remain unsupported.
- DevOpenDC supports OD_MEMORY; token/driver metadata is checked for basic
  validity but is not interpreted. GpiCreatePS supports PU_PELS with default
  or LONG format, NORMAL/MICRO PS and optional association flag. Bitmap
  dimensions determine memory-surface size. No arbitrary units/transforms.
- GpiBitBlt supports SRCCOPY, three or four points, nonnegative ordered
  rectangles, and BBO_OR/AND/IGNORE (no monochrome reduction distinction).
  Target upper-right is exclusive, matching the existing V1 bridge.
  Source and memory-target rectangles must fit their surfaces; the native
  window clips its target. No mirrored rectangles or other raster ops yet.
- Bit transfers require matching bitmap dimensions/depth in the info header;
  no compressed images, color-depth conversion in Query/SetBitmapBits, or
  top-down OS/2 storage. Header sizes other than 12 and 64 are rejected.
- GpiCreateLogColorTable supports default and direct-RGB modes with zero
  supplied entries. Arbitrary logical color tables are deferred; bitmap
  palettes work through the bitmap APIs. GpiSetColor/SetPel work on memory
  PSs; direct drawing into a window PS beyond BitBlt remains deferred.
- GpiSetBackMix accepts OVERPAINT and LEAVEALONE. No text/area primitives that
  consume a background mix are implemented in this release.
- No fonts, regions, paths, metafiles, printers, resource-loaded bitmaps or
  general GPI drawing implementation. Unsupported PMGPI calls log and return
  zero. This is a focused test release, not a claim SarienPM now runs.

VALIDATION HERE
- gpi1.c and pmhello.c pass 32-bit C89 syntax/type checks against the supplied
  Beta2 SDK. Legacy compiler keywords are removed for GCC and existing SDK
  comment/pragma warnings suppressed. This is not a Microsoft build/link.
- Production PMGPI/PM/callback/scheduler helpers pass native execution tests
  with Windows/WHP doubles under AddressSanitizer/UndefinedBehaviorSanitizer.
  Covers all supported bit depths, odd widths/padding, both header layouts,
  palette readback, partial rows, pixel nibble preservation, memory blits,
  top-down DIB submission/coordinate conversion, invalid buffers/formats,
  selected/stale handles, slot exhaustion and abandoned-resource cleanup.
- R7/R8/R9/R10 native regression checks pass in the same suite.
  Reproduce with GCC on a Linux development host:
    ASAN_OPTIONS=detect_leaks=0 python3 tests/run-sync-check.py
  LeakSanitizer is disabled for this ptrace-based execution environment.
- Logs: validation/r11-native-check.* and validation/guest-syntax-r11.txt.
  Earlier logs/notes are historical. Actual MSVC/C/386 compilation, LX linking,
  WHP execution and visible Windows rendering remain tests on your machine.
