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
No Windows or Wine runtime is available in the build environment. The user's
first live R9 TELNETPM results are recorded below. Native smoke executables,
font changes/resizing, and the R9 SimCity regression remain unverified.

### First live R9 proof (2026-10-03)

The supplied `phase2-r9-run.txt` contains 6,887 lines. The terminal child is
requested at 640x288, receives WM_SIZE at 640x288, and its initial full paint
is 640x288. All 94 font realization traces report Terminal 8x12, actual 8x12,
match 2. There are 360 text-draw traces and no executed UNIMPLEMENTED trap or
exception. Two existing resource-icon load failures remain. The log ends in
Ctrl+C, so this is not evidence of clean application shutdown.

The supplied test screenshots show:

| Page | Live result |
| --- | --- |
| 1, numbered rows | All ROW 01..24 visible, including TOP and BOTTOM. Missing-height regression resolved in this run. |
| 2, CP437 strips | Not executed: the server displayed the BINARY-negotiation explanation. Fine pixel comparison remains open. |
| 3, colors | Foreground/background samples and normal/bold/underline/reverse lines render. The green underline appearance may be client policy; no original-OS/2 comparison establishes its correctness yet. |
| 4, erasure | Empty brackets, KEEP results, blank erased rows and numeric overwrite match the expected character grid visually. |
| 5, scrolling | Top SCROLL 07 through bottom SCROLL 30, with expected-final-row 01..24. |

The user also reports improved BBS appearance after reconnecting. The working
connection used standard port 23; use that port in this TELNETPM GUI workflow.
These results confirm the child-height repair, not yet pixel-perfect CP437
rendering or every PM API path.

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
live BBS connection. R9 now has live proof of the full 24-row terminal area,
expected erasure, and correct 24-row scrolling against a repeatable target.
Remaining acceptance checks are equal top/middle/bottom CP437 strip heights,
font changes/resizing, native smoke logs, and no SimCity regression. This is
not a declaration that every PM rendering issue is fixed.
