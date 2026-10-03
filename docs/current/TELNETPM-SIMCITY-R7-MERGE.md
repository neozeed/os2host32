# R7: merge SimCity PM fixes with TELNETPM R6

Date: 2026-10-03. Track: native Win32. Status: source merge and cross-build
verified; new Windows pixel smoke and live application retests pending.

R6 established a live TELNETPM BBS exchange, but the native source baseline
used for that work did not include several later SimCity/Micropolis PM
fixes. The supplied comparison against `os2host32-2caf7a5` made those missing
changes recoverable. R7 restores them while preserving the TELNETPM APIs,
font layout, accelerator ownership, profiles, switch-list and networking.

## How the merge was established

The supplied `pmgpi.diff`, `pmshapi.diff` and `pmwin.diff` compare the user's
SimCity tree on the left with the delivered R6 tree on the right. Applying
them as an ordinary forward patch would remove SimCity fixes rather than
combine the two lines of work.

Every right-hand hunk was checked against the current R6 source; all twelve
compared files matched after line-ending normalization. Reversing the hunks
in a private directory reconstructed the SimCity side. A three-way comparison
then used the original uploaded `os2host32-ebb2009rd.zip` as the common
source baseline. No current remote GitHub HEAD was assumed or modified.

Only PMWIN and PMGPI runtime C sources needed new merged changes. The
WinStopTimer export was already present in both lines of work. The supplied
PMSHAPI left-hand source matched the original pre-TELNETPM baseline; its
differences are the newer profile and switch-list work already in R6.
Keeping the R6 PMSHAPI implementation retains those fixes.

There was one textual merge conflict: the SimCity blit trace helper was
inserted beside the old FONTMETRICS declaration, which R6 had replaced.
The resolution retains the helper and R6's 228-byte `font_query.h` layout,
font enumeration and logical-font ownership code.

## Restored behavior

| Area | Merged change | Purpose |
|---|---|---|
| GpiCreateBitmap | Retain caller-supplied indexed color table | Keep asset colors instead of the initial neutral palette |
| GpiSetBitmap | Publish the stored palette to the selected native DIB | Make native drawing/blitting see that palette |
| GpiBox | Use the native DC for memory and window presentation spaces | Respect packed/true-color pixel formats and draw outlines |
| WinFillRect | Derive memory-PS coordinates and default extent from its bitmap | Fill the requested bitmap rows instead of using an absent client HWND |
| WinCreateStdWindow / WinGetMsg | Keep the supplied pending-window list | Show each deferred visible window after initialization |
| GpiBitBlt tracing | Sample repeated 16-by-16 blit messages | Keep tile-map traces manageable |

The restored pending-window implementation retains the supplied 32-entry
process-global limit. This merge does not redesign PM queue/thread ownership.
The bitmap helpers retain their existing accepted formats and API limits.

The implementation uses SetDIBColorTable after selecting the bitmap because
the native API updates the DIB currently selected in the given DC. See
[Microsoft's SetDIBColorTable reference](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-setdibcolortable).

## TELNETPM behavior preserved

All eight R6 exports remain present, including accelerator creation/copy/
destruction, queue/window association, WinLoadMessage and GpiQueryFonts.
The 228-byte font records, bounded resource decoder, per-thread PM state,
clipboard/cursor helpers and logical-font release handling remain intact.

PMSHAPI's profile and switch-list source, the socket adapters, DOSCALLS,
the loader bridge, API catalogue and DLL export definitions are unchanged
from R6. The existing WinStopTimer implementation/export is retained once.
WHP and the ReactOS subsystem are unchanged.

R7 does not claim to resolve all TELNETPM text corruption. The supplied
SimCity changes do not replace the GpiSetBackMix stub, unconditional
transparent GpiCharStringAt path or text codepage handling. Restoring memory
surface operations is useful, but terminal font/cell/background behavior
still needs its own diagnosis and Windows evidence.

## Verification

The source merge is checked against both reconstructed inputs. Untouched
R6 files are compared with the preserved full R6 source snapshot. Existing
host checks pass: profile 81, switch-list 281, accelerator/text 290, fonts 23.
The API catalogue still covers 345 ordinal exports with 347 entries.
The existing SimCity PMWIN.885 static contract check passes.

The full native source tree is cross-built with i686 MinGW GCC 13.2.0:

```sh
make -j2 MINGW=i686-w64-mingw32-gcc all socket-smoke.exe telnetpm-api-smoke.exe pm-merge-smoke.exe
```

The DLL build retains existing warnings. No Windows runtime is available in
the build worker, so cross-build success is separate from live GDI behavior.

The new `tests/pmcompat/pm-merge-smoke.c` loads actual DLL exports by ordinal
and is intended to run on Windows. It checks:

* Fill and outline pixels on 1/4/8/24/32-bit memory bitmaps.
* WinFillRect partial, whole-bitmap and clipped rectangles with bottom-up
  OS/2 coordinates.
* Palette retention and publication for 1/4/8-bit DIBs using both 12-byte
  and 64-byte OS/2 bitmap headers, plus indexed-color-to-32-bit blits.
* Multiple deferred windows, a destroyed pending window and later windows.

The native test does not use a historical executable or make network
connections. Its pixel checks use GetPixel/GetDIBColorTable rather than
the DLLs' private structure layout. The supplied R6 API smoke remains a
separate test of the TELNETPM API surface.

## Windows test sequence

The full-source package includes an isolated `runtime/` directory with the
new PMWIN/PMGPI DLLs, new PM merge smoke, and unchanged R6 companion files.
Open CMD there and run:

```bat
run-pm-merge-smoke.cmd
run-api-smoke.cmd
run-telnetpm.cmd C:\OS2\tmp\telnetpm-phase1\telnetpm.exe
```

For SimCity/Micropolis, run `runtime\run-pm-app.cmd` with the same executable
you used for the comparison. Keep its normal asset working directory; the
runner does not change it. Example from that application's directory:

```bat
C:\path\to\package\runtime\run-pm-app.cmd simcity.exe
```

Return `phase2-r7-pm-merge-smoke.txt`, `phase2-r7-api-smoke.txt`, the
TELNETPM/PM application logs, and screenshots of both displays. Logs are
written beside the runners. The optional socket smoke uses the unchanged
R6/R5 networking binaries.

## Source integration and preservation

This package contains the complete working native source tree, including
SO32DLL/TCP32DLL source, and needs no earlier source overlay. Compare/merge
it with your checkout while keeping unrelated newer work. The frozen WHP
and ReactOS directories were absent from the native working tree; do not
delete them from a larger repository to match this snapshot.

An optional `integration/r6-to-r7.patch` records only the two changed PM
sources, Makefile target, new smoke source and this document. The supplied
comparison files and merge verification are kept separately for review.
The original R6 live milestone and source archive remain preservation points.
No commit, tag or push was performed for R7.
