# M31D R3 - BIO paint/dialog geometry fix

R3 is a focused runtime correction on top of M31D R2.

## Fixed

1. PMWIN now translates the ordinary OS/2 `CLR_*` logical palette as well as
   `SYSCLR_*`. BIO clears its main client with `CLR_WHITE == -2` and the Legend
   client with `CLR_PALEGRAY`; R2 passed those values directly to Win32 as
   `COLORREF`s, producing the black/near-black windows.

2. `WinDlgBox` now honors the type-4 `DLGTEMPLATE` root x/y coordinates
   relative to the supplied parent, converting OS/2 bottom-left coordinates
   to Win32 top-left coordinates. BIO passes `HWND_DESKTOP` as the parent and
   a deliberately narrow right-edge main window as the owner. R2 centered the
   Dates dialog on the owner, which pushed most of it off-screen.

No BIO-specific resource IDs, dates, window sizes, or coordinates are hard-coded.
