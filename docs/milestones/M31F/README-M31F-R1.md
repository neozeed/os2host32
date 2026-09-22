# M31F R1 - JIGSAW broad first PM/GPI pass

Target: untouched Microsoft/IBM OS/2 2.0 Beta 2 SDK `JIGSAW.EXE`.

JIGSAW is a flat 32-bit LE PM application and is deliberately much broader
than WMCHAR/HANOI/BIO: it exercises an asynchronous PM worker queue, modeless
dialogs, horizontal and vertical frame scrollbars, old 4-bpp OS/2 bitmaps,
regions, paths, splines and the default-view transform.

## New PMWIN surface

R1 adds the JIGSAW-imported helpers including:

- `WinSetDlgItemText` / `WinQueryDlgItemText`
- modeless resource-backed `WinLoadDlg`
- `WinEnableWindow`, focus/capture/pointer/parent helpers
- rectangle union/intersection/point tests and update-region queries
- `WinMapWindowPoints`
- `WinPostQueueMsg` / `WinPeekMsg`; HMQs are backed by the owning Win32
  thread message queue, preserving JIGSAW's asynchronous drawing thread
- horizontal frame scrollbars with separate horizontal/vertical proxy handles
- `MM_QUERYITEM` submenu lookup used by the Status menu
- `WM_MOUSEMOVE`, button double-click and horizontal-scroll translation
- JIGSAW system values (`SV_CX/CYFULLSCREEN`, byte alignment)

## New PMGPI surface

R1 adds:

- 1/4/8/24/32-bpp native DIB-section backing for `GpiCreateBitmap`
- `GpiAssociate`, bitmap deletion and native-HDC `GpiBitBlt`
- region set/combine/query/paint/destroy operations
- begin/end path, clip path and `GpiPolySpline`
- default-view matrix query/set and `GpiConvert`
- `GpiQueryPel` and attribute-mode state

The bundled `YOSEMITE.BMP` is the original 640x480 4-bpp OS/2 core-header
bitmap and is intentionally retained unchanged.

## Other additions

- DOSCALLS.236 `DosSetPriority` (the source-era `DosSetPrty` alias)
- PMSHAPI.123 `WinChangeSwitchEntry`
- WC_SCROLLBAR support in translated dialog templates

## Test

Build the normal host and personalities, then run:

    make m31f-jigsaw-static-check
    make tools compat
    make m31f-jigsaw-check
    m31f-jigsaw-r1-test.cmd

The test script starts in `examples\m31f-jigsaw` so the original
`YOSEMITE.BMP` is immediately available to the sample's Open dialog.

This is a broad first pass. It is statically regression-checked here, but the
Win32 runtime behavior is intended to be refined from the first real JIGSAW
trace rather than hidden behind sample-specific shortcuts.
