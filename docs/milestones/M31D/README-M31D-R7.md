# M31D R7 - WinBeginPaint DC-state fix

R6 added presentation-space DC isolation, but `WinBeginPaint()` allocated its
CompatPS before the native HDC existed.  `alloc_ps(..., NULL, ...)` therefore
left `saved_dc == 0`; after `BeginPaint()` supplied the real HDC, the code never
called `SaveDC()`.

BIO uses `GpiSetClipRegion()` while painting the main chart.  Without saving and
restoring the actual BeginPaint HDC, that clip region can survive `WinEndPaint()`
and affect later paints, including the Legend window.

R7 calls `SaveDC()` immediately after a successful `BeginPaint()` and restores
that state in the existing `WinEndPaint()` path.  The PS-state regression now
explicitly checks this BeginPaint-specific save so the R6 false-positive cannot
recur.

No BIO-specific drawing behavior is added.
