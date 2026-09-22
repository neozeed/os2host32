# M31F R7 - JIGSAW device-PS / blit-coordinate fix

R6 is the first M31F build that successfully opens the original untouched
`YOSEMITE.BMP` and constructs/displays the puzzle.  Manual R6 testing also
confirmed HANOI and Sarien still run.  The remaining visible problems are
piece-drag blits landing at the wrong Y position, stale/trailing pixels during
dragging, and a maximized client being cleared without the puzzle appearing in
the repaint.

The R6 trace identifies a generic PMGPI device-PS coordinate bug.  `GpiCreatePS`
can recover the owning native HWND from its HDC (and already did so for trace
output), but it did not retain that HWND in `CompatPS`.  Consequently
`gpi_surface_height()` could not reliably determine the real client height for
a device PS.

That is fatal for OS/2 PM's bottom-left coordinates.  For example, JIGSAW asks
for a screen blit of OS/2 rectangle `(258,98)..(425,225)`.  With a 317-pixel
client, the Win32 top-left destination must be:

    317 - 225 = 92

R6 instead fell through the temporary R3 `StretchDIBits` presentation branch,
used the blit height (127) as the client height, and produced:

    127 - 98 - 127 = -98

which exactly matches the R6 trace and explains the drag offset/trails.

R7 fixes this generically:

- `GpiCreatePS` stores `WindowFromDC(p->dc)` in `CompatPS.hwnd`.
- `GpiAssociate` refreshes the stored HWND whenever the associated HDC changes.
- Device surface height therefore comes from `GetClientRect(hwnd)`.
- The temporary R3 screen-only `StretchDIBits` SRCCOPY branch is removed.
- Bitmap-to-bitmap and bitmap-to-device operations now share the native DIB
  section/HDC `StretchBlt` path, converting both source and destination from
  OS/2 bottom-left coordinates using their actual surface heights.
- R4's `GpiQueryBitmapBits`/`GpiSetBitmapBits` palette round trip remains
  unchanged, so Sarien no longer needs the old presentation exception.

No JIGSAW- or Sarien-name special case is used.

## Expected runtime changes

For the R6 drag example, the screen blit should change from a native destination
near:

    dst=(258,-98 167x127)

to approximately:

    dst=(258,92 167x127)

for a 317-pixel-high client.

After maximizing, a full-client redraw should use the same height-aware native
HDC path instead of the old direct-DIB screen exception.

## Runtime test

Build normally and run:

    m31f-jigsaw-r7-test.cmd

Load `YOSEMITE.BMP`, then test:

1. drag several pieces in different parts of the client;
2. verify the old position is restored rather than leaving trails;
3. maximize/restore the window and confirm the puzzle repaints;
4. rerun Sarien (and preferably HANOI) before promoting R7.

Preserve `jigsaw-r7-err.txt` if any of those still fail.
