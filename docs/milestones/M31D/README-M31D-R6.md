# M31D R6 - presentation-space state isolation

BIO R5 proved the main chart, resources, menus and icon path.  The remaining blank Legend and partial/low repaint after maximize were traced to native HDC state leaking between OS/2 presentation-space lifetimes.

BIO is the first regression sample to use GpiSetClipRegion.  The compatibility layer selected the region directly into the Win32 HDC but did not restore the HDC before EndPaint/ReleaseDC.  Common DC reuse can therefore carry BIO's chart clipping into the Legend or later repaint cycles.

R6 adds SaveDC/RestoreDC ownership to CompatPS for WinBeginPaint, WinGetPS and GpiCreatePS.  This scopes clip regions and other GDI attributes to the lifetime of one OS/2 HPS.

No BIO-specific drawing behavior is hard-coded.
