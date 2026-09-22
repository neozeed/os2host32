# M31D R1 - BIO first runnable pass

Target: untouched Microsoft/IBM OS/2 2.0 Beta 2 SDK `BIO.EXE` (Biorhythm).

This pass extends frozen M31C FINAL in the areas BIO actually uses.

## New architecture

- Multiple guest PM classes are registered simultaneously (`Biorhythm` and `Legend`).
- Each native compatibility window carries its own guest wndproc instead of relying on one global callback.
- Child `WinCreateStdWindow` calls use the requested parent and can be independently shown/hidden.
- Tiny historical PM stacks still receive the M31A 256 KB runtime headroom.

## New PMGPI surface

- 359 GpiCharStringAt
- 370 GpiCreateRegion
- 379 GpiDestroyPS
- 398 GpiLine
- 404 GpiMove
- 489 GpiQueryTextBox
- 492 GpiQueryWidthTable
- 516 GpiSetClipRegion
- 530 GpiSetLineType
- 604 DevCloseDC

BIO's 24-bit memory bitmap path is accepted by GpiCreateBitmap/GpiBitBlt so Copy can place a native bitmap on the Windows clipboard.

## New PMWIN surface

- 707 WinCloseClipbrd
- 710 WinCopyRect
- 733 WinEmptyClipbrd
- 767 WinInvertRect
- 781 WinLoadString
- 793 WinOpenClipbrd
- 854 WinSetClipbrdData
- 883 WinShowWindow
- 892 WinUpdateWindow
- 929 WinSubclassWindow (collapsed frame/client compatibility behavior)

Also added BIO-required system metrics, native resize/scroll/mouse translation, Ctrl-character accelerators, vertical-scrollbar plumbing, group boxes and auto-check boxes in resource dialogs, and `BM_QUERYCHECK` translation.

## PMSHAPI profile calls

- 114 PrfQueryProfileInt
- 116 PrfWriteProfileString

They persist the OS/2 user-profile values in `os2user.ini` beside the host executable.

## Resources

BIO adds generic RT_STRING handling. Its resource set is:
- icon id 1
- menu id 1
- About dialog id 2
- Dates dialog id 3
- string table (`Biorhythm`, `Legend`)
- accelerator table: Ctrl+D, Ctrl+L, Ctrl+C

## First Windows test

Run `m31d-bio-r1-test.cmd`.

Expected progression:
1. scan resolves all 69 imported ordinals;
2. main window title is `Biorhythm`, with Options menu/icon;
3. first launch opens Dates dialog because no saved birth date exists;
4. valid birth/display dates paint the chart;
5. Legend child window paints three colored samples and can hide/show;
6. keyboard/scrollbar navigation moves through dates;
7. Ctrl+D / Ctrl+L / Ctrl+C dispatch the resource accelerators;
8. Copy places a bitmap on the host clipboard;
9. checking Update OS2.INI persists the birth date to `os2user.ini`.

M31A/M31B/M31C remain regression gates.
