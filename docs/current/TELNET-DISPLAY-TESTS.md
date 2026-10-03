# Repeatable Telnet display tests (PM DISPLAY R1)

The included `telnet-display-server.exe` is a standalone Windows console
program. It requires neither Python nor the OS/2 compatibility DLLs. It sends
five fixed ANSI screens and waits for your keys. There are no rotating banners,
timestamps, login screens or remote dependencies. It serves one client at a
time and accepts another connection after you disconnect.

## Start and connect

From the package's `runtime` directory:

```bat
start-display-server.cmd
```

This listens on **127.0.0.1, port 2323**. In TELNETPM connect to that host and
port. Your December 1993 executable contains this command syntax:

```text
open 127.0.0.1 2323
```

For the real OS/2 VM, run the server on the Windows host with a reachable bind
address, for example:

```bat
start-display-server.cmd 0.0.0.0 2323
```

Connect OS/2 to the **Windows host's reachable IPv4 address**, not 127.0.0.1
(which means the OS/2 VM itself), on port 2323. Allow this server through the
Windows firewall for that connection if prompted. Both clients can use the
same Windows host address; disconnect one before connecting the other. If your
connection dialog offers only a hostname, start the server on port 23 instead:

```bat
start-display-server.cmd 0.0.0.0 23
```

Select ANSI emulation, 80 columns, 24 rows, and code page 437 in both clients.
Start with the same nominal font cell size, preferably Terminal 8x12. Screen
dimensions may be scaled by Windows DPI settings or the VM viewer; compare
unscaled captures when judging a single pixel. The server does not change the
client's configured number of rows, font, or code page.

Press **1..5** to choose a page, **R** to redraw it, or **Q** to disconnect.
The server negotiates server echo and suppress-go-ahead; it deliberately does
not echo the page-selection keys into the test screen. If your client remains
in line mode, press Enter after the key, or select character mode. CR/LF does
not add a line to the display. Ctrl+C in the server console stops the server.

## What to compare

| Key | Screen | Expected result |
| --- | --- | --- |
| 1 | Numbered rows and column ruler | All `ROW 01` through `ROW 24` labels appear. TOP is first; BOTTOM is last. Every row ends with `#` in column 79. Column 80 is blank. |
| 2 | CP437 pixel strips | Rows 1, 12 and 24 contain identical strips: full block DB, upper half DF, lower half DC, shades B0/B1/B2, horizontal line C4, vertical line B3. Top, middle and bottom strip heights should agree. |
| 3 | Colors and font attributes | Eight foreground/background samples; normal, bold, underline and reverse lines. Client palette and bold-intensity preferences can affect appearance. |
| 4 | Erasure and overwrite | Row 4 has empty brackets. Rows 6 and 14 end with KEEP. Rows 8 and 15 are blank. Row 10 contains 0123456789. Row 12 has KEEP at column 15. No old Xs or ERASE-ME text remains. |
| 5 | Scrolling | The final top row says SCROLL 07; bottom says SCROLL 30. Every line's expected-final-row number agrees with its actual row (01..24). |

Page 2 requires the client to accept the Telnet BINARY output option. If it
does not, the server shows an ASCII explanation instead of sending high-bit
bytes in NVT mode. Enable binary negotiation and press R. Other pages work
without binary mode. Wrong accented letters in place of blocks suggest an
encoding/font problem; a missing ROW 01 label suggests sizing or scrolling.

The strips use ten characters per group, except the final group of nine.
Compare the same group at the top and middle, including the solid block, to
separate missing scan lines from normal glyph whitespace. Page 1 is entirely
ASCII, so it remains useful if the OS/2 CP437 font mapping differs.

All pages leave column 80 blank and park the cursor at row 23, column 79.
This avoids bottom-right autowrap differences and keeps the cursor out of the
top/middle/bottom strip comparison. There is no animation. Page 5 writes 30
lines with CRLF between them and no newline after the last line.

## A useful comparison run

1. Start the server, connect with the R8 runtime, and capture pages 1 and 2.
2. Disconnect. Connect with R9, select the same font, and capture pages 1 and 2.
3. Disconnect. Connect from real OS/2 and capture the same pages.
4. Check pages 4 and 5 on R9. Resize or select another fixed font, then press R
   to see whether the geometry stays correct.

For the first R9 run, also execute `run-window-size-smoke.cmd` and
`run-font-render-smoke.cmd`. To capture the application geometry:

```bat
set OS2_PM_TRACE=1
run-telnetpm.cmd C:\OS2\tmp\telnetpm-phase1\telnetpm.exe
set OS2_PM_TRACE=
```

At 80x24 with an 8x12 font, the terminal child's full client/paint dimensions
should be **640x288** (hex **280x120**). A smaller paint rectangle is normal for
a partial repaint; use the child's `WinQueryWindowRect` or initial full paint.
The requested-size trace now records the guest's cx/cy alongside native size.
Please return the two smoke logs, `phase2-r9-run.txt`, and matching screenshots
of pages 1 and 2 if any clipping remains. A complete font change/resize test is
still needed on native Windows.

## Source, fixtures and verification

Source: `tests/net/telnet-display-server.c`. Build the Windows executable:

```sh
make telnet-display-server.exe MINGW=i686-w64-mingw32-gcc
```

The server also builds on POSIX hosts. The host-only test needs Python 3:

```sh
make telnet-display-check
```

It exercises actual loopback TCP, fragmented Telnet commands/subnegotiation,
duplicate and rejected options, redraws, reconnects, and an independent ANSI
cell-grid oracle for erasure, attributes and scrolling. It does not validate
Windows or OS/2 font pixels.

`runtime/display-fixtures/page-N.ans` contains each exact raw display stream
(before Telnet IAC escaping), and `page-N.txt` contains the expected final
80x24 character grid in UTF-8. The text grids omit color/attribute metadata;
use the table and page 3 for that comparison. Reproduce an ANSI fixture with:

```bat
bin\telnet-display-server.exe --dump 1 > page-1.ans
```

The Windows dump uses binary stdout, preserving CRLF and CP437 bytes.
See the package checksum manifest for exact fixture hashes.

Protocol references: [RFC 854](https://www.rfc-editor.org/rfc/rfc854),
[RFC 856](https://www.rfc-editor.org/rfc/rfc856),
[RFC 857](https://www.rfc-editor.org/rfc/rfc857),
[RFC 858](https://www.rfc-editor.org/rfc/rfc858), and the
[XTerm control sequence reference](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html).
