# TELNETPM R6: first live BBS session on native Win32

Date: 2026-10-03. Milestone: `TELNETPM-PHASE2-R6-LIVE`.

The original OS/2 TELNETPM application reached a remote BBS through the
native Win32 os2host32 compatibility layer. It opened its Presentation
Manager UI, connected to `vert.synchro.net`, displayed the server banner,
sent typed login text and displayed the server's response.

This is the first live TELNETPM network-session proof in this project.
The display still has substantial rendering defects. Preserve R6 as the
working execution/network baseline before improving text rendering.

## What ran

The target is the original 273,544-byte LX TELNETPM.EXE, SHA-256:

```text
16f34d712cdefb8b3f4fa93b7956fc97429697419deccd80b62b8d838d4960e2
```

It runs through `--telnetpm-probe` on the V1/native Win32 track. The input
file is unmodified. The existing exact-image bridge applies in-memory
relocations and replaces known mixed-mode/exception-chain transitions;
this result does not establish support for arbitrary 16-bit LX applications.
WHP/V2 and the ReactOS subsystem were not involved or changed.

## What R6 added

The R5 run stopped at PMWIN.798, WinQueryAccelTable. R6 adds the PM APIs
needed to continue through accelerator and font initialization:

| Module | Ordinal | Function |
|---|---:|---|
| PMWIN | 709 | WinCopyAccelTable |
| PMWIN | 713 | WinCreateAccelTable |
| PMWIN | 723 | WinDestroyAccelTable |
| PMWIN | 776 | WinLoadAccelTable |
| PMWIN | 779 | WinLoadMessage |
| PMWIN | 798 | WinQueryAccelTable |
| PMWIN | 850 | WinSetAccelTable |
| PMGPI | 586 | GpiQueryFonts |

Accelerator resources now have owned table storage, queue/window
associations and keyboard dispatch. Message resources use bounded string
decoding. Font enumeration maps installed GDI fonts into the 228-byte
FONTMETRICS records requested by TELNETPM. Five formerly deferred imports
are resolved; the remaining three exports complete accelerator ownership.

The R5 socket adapters, shell switch-list implementation, DOSCALLS and
loader binaries are unchanged in the R6 runtime package.

## Windows evidence

The supplied `phase2-r6-api-smoke.txt` records:

```text
PMSHAPI switch-list ordinal, layout, query/change/remove checks PASS
GpiQueryFonts: 777 records, 75 regular fixed-pitch fonts, stride=228
R6 accelerator/message/font API smoke: 39 checks PASS
TELNETPM API smoke: 131 checks PASS
```

The 39-check R6 section is nested in the main smoke; the two counters
should not be added as independent top-level tests. Font counts reflect
this Windows installation and are not universal expected values.

The 30,127-line `phase2-r6-run.txt` records socket creation, hostname lookup,
connect, 23 send entries and 27 receive entries. It contains no
UNIMPLEMENTED call or exception marker. Entry counts do not represent
packet counts or prove the return value of each individual call.

Five supplied screenshots show the progression:

| Evidence file | Visible result |
|---|---|
| `image(20261003-073748).png` | TELNETPM frame and menu bar |
| `image(20261003-073812).png` | Remote server connection text |
| `image(20261003-073817).png` | BBS banner, connection details and ANSI artwork |
| `image(20261003-073826).png` | Further received text, with substantial display corruption |
| `image(20261003-073847).png` | Submitted username, server rejection and another login prompt |

The last screenshot establishes application input and a server response
in addition to received banner output. It does not establish successful
account authentication.

The preceding R5 native Winsock smoke passed 95 checks. Close during recv,
accept, recvfrom and infinite select returned successfully in 31, 31, 16
and 32 ms respectively. Those socket binaries are unchanged in R6.

Raw logs/screenshots are included in the companion source-handoff archive.
The milestone document can be committed without copying that evidence
directory into the repository.

## Build and regression evidence

R6 PMWIN.dll, PMGPI.dll and telnetpm-api-smoke.exe cross-build as PE32/i386
with MinGW GCC 13.2.0 using the project's C89/GNU make workflow.

* Accelerator/resource-text production helpers: 290 checks PASS.
* Font-query production enumerator with controlled GDI fixtures: 23 PASS.
* Existing profile and switch-list host checks: 81 and 281 PASS.
* Existing production x86 bridge checks under Unicorn: 285 PASS.
* API catalogue: 347 entries cover 345 ordinal exports.
* Actual PE export tables contain all eight additions.
* The R5-to-R6 source patch applies to the saved R5 files and reproduces
  all fifteen delivered source/build/test files.

Host checks use mocked Win32/GDI state where indicated; the separate
Windows smoke and application screenshots supply the live runtime proof.
Three pre-existing DLL-build warnings remain. The handoff adds no runtime
code changes to this live-tested source snapshot.

## Remaining work

Text overlaps, repainting leaves stale pixels, and ANSI artwork contains
incorrect glyphs. Font selection, cell geometry, background mixing,
codepage conversion and scroll/repaint behavior are the next targets.
The current GpiSetBackMix stub and unconditional transparent drawing in
GpiCharStringAt are concrete leads, not a complete diagnosis.

Seven imports remain deferred on the observed installation:

```text
DOSCALLS.300  PMWIN.890  PMCTLS.4  DOSCALLS.111
DOSCALLS.110  DOSCALLS.356  MSG.6
```

None was called in the supplied session log. A different UI path can
still hit one and stop the probe. The log ends in the message loop without
a normal process-exit record, so clean shutdown is not established.
External authentication DLLs, the newer TELNETPM specimen and general
mixed-mode LX support are outside this milestone.

## Preservation and next handoff

Suggested tag after source integration: `telnetpm-phase2-r6-live`.
The original delivered runtime archive is `telnetpm-phase2-r6-pm-init.zip`:

```text
SHA-256 d7d49a9ab13d9fe8a84c73d10579ea3452d2ce2568f9b7b3121b6a13dc160be8
```

Use [TELNETPM-R6-HANDOFF.md](../../current/TELNETPM-R6-HANDOFF.md) for the
complete file inventory, API invariants, source integration, test commands,
limits and rendering investigation plan. Continue rendering work as a new
revision while retaining this source/binary preservation point.
