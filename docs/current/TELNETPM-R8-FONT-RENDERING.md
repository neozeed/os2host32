# TELNETPM R8: font selector and terminal text

2026-10-03. Based on the complete R7 SimCity/TELNETPM merge. Native Win32 V1
only; WHP and the ReactOS subsystem remain outside this change.

## Evidence and diagnosis

The user's R7 screenshots show Micropolis/2 drawing its tool palette, buildings,
roads and status windows again. That is the live milestone for the R7 merge.
TELNETPM connects and paints, but its terminal text overlaps and leaves stale
characters, and its Font Selection dialog contains only three buttons.

The supplied `phase2-r7-run.txt` contains 417 calls to `PMGPI.368`
(`GpiCreateLogFont`), 444 to `.359` (`GpiCharStringAt`), nine to `.505`
(`GpiSetBackMix`), and no calls to `.513` (`GpiSetCharSet`). The log ends with
Ctrl+C; it does not prove clean application shutdown. The font-dialog screenshot
and log are separate evidence; the log need not contain that dialog session.

The exact old TELNETPM image remains 273,544 bytes, SHA-256
`16f34d712cdefb8b3f4fa93b7956fc97429697419deccd80b62b8d838d4960e2`.
Resource and relocated-object inspection found:

* Dialog resource 300 has four children. Control 301 is `WC_COMBOBOX` (class 2),
  style `0x80030004` (`CBS_DROPDOWNLIST`). R7's dialog loader falls back to
  an empty STATIC for that class. The application's initialization inserts
  font labels with `LM_INSERTITEM` and selects the first item with
  `LM_SELECTITEM`. Those messages also need combo-box translation.
* At object 1+`2D04`, painting creates logical font ID 1. At +`2D20`, it compares
  the result with 2 (`FONT_MATCH`); only that branch calls `GpiSetCharSet`.
  R7 always returned 1 (`FONT_DEFAULT`), explaining the absent selection calls.
  This is not a default-font-ID-zero call.
* Each subsequent style span can redefine that same, already selected ID.
  Rejecting replacement of a current text font would prevent later spans
  from getting their requested attributes.
* At +`2AA6`, painting requests `BM_OVERPAINT` (2). R7 ignored the mix and
  forcibly drew transparent text. Spaces therefore could not erase old text.
* TELNETPM copies enumerated font dimensions into its cell calculations and
  sets `FATTRS.usCodePage=0`. That means the OS/2 process code page, not the
  Windows ANSI code page.

## Changes

`dlls/pmwin/pmwin.c` creates native combo boxes from dialog resources and maps
the three OS/2 combo styles for both resource and `WinCreateWindow` paths.
The shared list-message dispatcher now translates supported `LM_*` operations
to native list-box or combo-box messages. It covers insertion (including the
existing sorted insertion), count, selection/deselection, bounded text reads,
text length, deletion, item data and reset. `CBM_SHOWLIST`/`CBM_ISLISTSHOWING`
are supported. Native combo notifications from dialogs map to OS/2
`WM_CONTROL` notifications, including `CBN_LBSELECT=4` and `CBN_ENTER=7`.

`dlls/pmgpi/pmgpi.c` realizes a candidate font and checks its actual face,
requested cell dimensions and supported style flags before returning
`FONT_MATCH=2`. Substitution still returns `FONT_DEFAULT=1`; this is not an
unconditional success change. Known OS/2 family aliases retain their native
equivalents. Current text-font IDs can be replaced, with old GDI objects
deselected/deleted and the new definition retained. Default ID 0 can be
redefined and restored too. The shared `CompatPS` binary layout is unchanged.

Background mix now selects opaque or transparent GDI text output. New PMWIN
and PMGPI presentation spaces start with PM's transparent default. Optional
OR/XOR backgrounds fall back to leave-alone; invalid mix values fail.
`GpiCharStringAt` uses a baseline reference and updates the current position
by the measured advance. It returns `GPI_OK=1`, not the input byte count.
It validates the documented 1..512-byte string length.

Text is decoded to Unicode using the selected font's explicit code page, or
the process code page obtained through `DOSCALLS.291`. Standalone PM without
DOSCALLS uses the personality's CP437 default. This removes dependence on the
host Windows ANSI/OEM setting for CP437 box drawing and accented characters.
Text-box and single-byte width queries use the same decoding. Rendering uses
non-antialiased logical fonts to suit the existing terminal cell grid.

## What is preserved

R7 bitmap palette handling, bitmap fills/outlines, blits and deferred window
display remain unchanged. PMSHAPI, DOSCALLS, loader/probe, sockets and all
ordinal export definitions are unchanged. R6 accelerator, resource-message
and font enumeration support remains present. The full source package includes
`common/win32/os2_socket_win32.c`, `common/include/os2_net.h`, `dlls/so32dll/`
and `dlls/tcp32dll/`; earlier source overlays are not required.

## Verification and Windows run

The package contains a clean MinGW C89 build and host regression logs.
Host checks cover profiles (81), switch lists (281), accelerator/text helpers
(290), font enumeration (23), the ordinal catalog and the SimCity timer.
These are existing host checks, not proof of the new Windows renderer.

`tests/pmcompat/font-render-smoke.c` is a new native Windows executable. It
loads the actual PM DLL exports by ordinal and tests a synthetic class-2
dialog resource, list operations, selected text and callback notification
packing. Its pixel checks compare output with independently positioned Unicode
GDI text, including opaque spaces, transparent spaces, baseline bounds,
CP437/850/1252, process-code-page changes, font match versus substitution,
selected-ID replacement and font restoration. It needs no TELNETPM executable,
network access or Python. It requires the standard Windows Courier New font.

The smoke executable is cross-built, **not executed here**: this environment
has no Windows or Wine runtime. Live R8 font-dialog and terminal results are
still pending. Use the isolated runtime folder, keeping the normal ETC setting:

```bat
run-font-render-smoke.cmd
run-pm-merge-smoke.cmd
run-api-smoke.cmd
run-telnetpm.cmd C:\OS2\tmp\telnetpm-phase1\telnetpm.exe
```

If drawing still looks wrong, collect a brief traced session:

```bat
set OS2_PM_TRACE=1
run-telnetpm.cmd C:\OS2\tmp\telnetpm-phase1\telnetpm.exe
set OS2_PM_TRACE=
```

The trace includes requested/realized face and cell size, match result,
code page and text background mode. Open Font Selection, expand the list,
select a fixed-pitch font and press OK. Reconnect or repaint the terminal;
compare box drawing, column alignment and erasure of old characters. Send
the smoke logs, a font-dialog screenshot and a terminal screenshot. Retry
SimCity using `run-pm-app.cmd` from its asset directory as a graphics regression.

## Limits and next work

This is a focused text/control implementation, not a complete GPI font engine.
Outline character transforms, exact OS/2 bitmap-font rasterization, private
font loading, full DBCS width tables, device-font matching and owner-drawn combo
boxes are not implemented. Font enumeration still exposes Windows-installed
fonts and metrics. The existing process-wide logical-font table has 256 slots.
Selected non-default font deletion still requires selecting another font first.
Full presentation-space attribute preservation across DC reassociation remains
limited. Non-dialog control-notification routing is unchanged.

The Sarien screenshot also shows bitmap artifacts; this update does not claim
to diagnose or fix those. That needs its own executable/version and trace
comparison if it persists. TCP/IP behavior is unchanged and need not be
reimplemented for this graphics update.

## Contracts consulted

* Included IBM SDK: `DLGTEMPLATE`/`DLGTITEM`, `WC_COMBOBOX`, `CBS_*`, `LM_*`,
  `CBN_*`, `FATTRS`, `BM_*`, `GPI_OK` and `GPI_ERROR` declarations.
* IBM toolkit [GpiCreateLogFont parameters](https://www.os2.kr/komh/os2books/os2tk45/gpi2/336_L2H_GpiCreateLogFontPara.html)
  and [remarks](https://www.os2.kr/komh/os2books/os2tk45/gpi2/337_L2H_GpiCreateLogFontRema.html).
* IBM toolkit [drawing a character string](https://www.os2.kr/komh/os2books/os2tk45/gpi4/121_L3_DrawingaCharacterStr.html).
* Microsoft [SetBkMode](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-setbkmode)
  and [TextOutW](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-textoutw).

Git integration: the only existing source files changed from R7 are `Makefile`,
`dlls/pmwin/pmwin.c` and `dlls/pmgpi/pmgpi.c`. Add this note and the new smoke
source. The archive supplies an optional R7-to-R8 patch and file manifest.
No repository commits, tags or pushes were made.
