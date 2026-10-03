# R9 handoff: terminal child geometry and deterministic display tests

Date: 2026-10-03. Baseline: complete R8 font-rendering source, preserving the
R7 SimCity bitmap/palette merge and the R6 TCP/IP implementation.

## Finding

The supplied `phase2-r8-run(1).txt` shows a Terminal font requested and realized
at 8x12 with match result 2. The terminal child's first full paint is 640x267
pixels, however. At 24 rows, its intended content height is 24*12 = 288.
The first drawn baseline at y=266 is just one pixel below the top boundary;
another nominal row baseline (278) lies outside the client rectangle.
This is stronger evidence of a geometry fault than of a wrong font size.

In the December 1993 TELNETPM image, object 1+259C..25A8 computes the terminal
height as rows times font-cell height. The WinSetWindowPos call returning at
object 1+85AE passes that stored height and the computed width to the child.

PMWIN.875 applied the top-level title-bar adjustment to every window, including
WS_CHILD windows. With a 23-pixel title metric it subtracted 21 pixels from the
requested child height: 288 becomes 267, matching the trace. A child with a
border was also incorrectly expanded by AdjustWindowRect.

The rotating BBS screenshots cannot establish which exact source rows should
match. The fixed test server provides that missing comparison. A remaining
one-pixel baseline/clipping issue is not ruled out until the fixed strips have
been compared on Windows and OS/2.

## Changes

- `dlls/pmwin/pmwin.c`: child windows now use the requested cx/cy as complete
  native window extents. Move-only calls still use the current size; size-only
  calls preserve position. Requested cx/cy are included in the optional trace.
- `tests/pmcompat/window-size-smoke.c`: real ordinal calls check borderless
  and bordered children, three 24-row sizes, PM/native queries, paint extents,
  and ignored move/size arguments.
- `tests/net/telnet-display-server.c`: portable C89 standalone Windows/POSIX
  server with five fixed screens, Telnet option handling, and raw fixture dumps.
- `tests/net/test_telnet_display.py`: actual TCP integration and independent
  ANSI cell-grid expectations.
- `Makefile`: native smoke/server and host server-test targets; cleanup entries.
- This handoff and `TELNET-DISPLAY-TESTS.md`: build and comparison instructions.

The existing top-level frame sizing convention is retained. PMGPI, DOSCALLS,
PMSHAPI, SO32DLL/TCP32DLL, loader behavior, exports and the shared presentation
space layout are unchanged. No font size, baseline or glyph offset is patched
to compensate for the geometry fault.

## Validation and limits

See `verification/` in the package for the actual build and test results.
The server's POSIX build and loopback behavior can be executed here. The
Windows server and window-size regression are cross-built PE32/i386 binaries.
Native Windows GUI smoke tests and live R9 TELNETPM/SimCity runs are pending;
no Windows or Wine runtime is available in the build environment.

Run `runtime/run-window-size-smoke.cmd`, `run-font-render-smoke.cmd`, and
`run-pm-merge-smoke.cmd`, then compare TELNETPM against the fixed server.
Check SimCity with `run-pm-app.cmd` from its normal asset directory. Child
sizing affects other PM applications too, even though their intended child
dimensions are now honored; this is the relevant live regression check.

## Integration and milestone

Use this complete source tree, or apply `integration/r8-to-r9.patch` to the
complete R8 source. `integration/FILES-TO-COMMIT.txt` enumerates the seven
changed/new files. Full network source is included; there is no dependency on
an earlier TCP/IP overlay. Build the active native targets with GNU make and
32-bit MinGW GCC; Python is needed only for host verification scripts.

Milestone: R8 achieved visible terminal text, working font selection and a
live BBS connection. R9 corrects a demonstrated child-size truncation and adds
a repeatable display target. Acceptance is all 24 rows and equal top/middle/
bottom strip heights at 8x12, correct erase/scroll screens, and no SimCity
regression. This is not a declaration that every PM rendering issue is fixed.
