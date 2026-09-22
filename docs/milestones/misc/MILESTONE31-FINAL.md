# MILESTONE 31 FINAL — Presentation Manager + CMD32 quality-of-life freeze

## Status

**FROZEN / PROVEN V1 NATIVE-WIN32 MILESTONE**

Milestone 31 closes the Presentation Manager expansion phase that began with the
Microsoft OS/2 2.0 Beta 2 SDK examples and ends with untouched OS/2 2.00 GA
NEKO plus a substantially more usable native CMD32 shell.

This is the V1/native-Win32 track. Do not mix WHP/V2, V86, emulation or the
frozen EMX work into this branch.

Frozen parent history:

- M30 FINAL: flat 32-bit LE/LX loader/runtime foundation.
- M31F FINAL R7: WMCHAR/HANOI/BIO/HELLO+OPENDLG/Sarien/JIGSAW PM baseline.
- M31G R19 FINAL: untouched OS/2 2.00 GA NEKO with original NEKO.DLL.
- M31 FINAL: M31G R19 plus the post-freeze CMD32 QoL series through TABCOMP R3.

## Runtime-proven PM applications

Known-good applications in the preserved regression set:

- WMCHAR
- HANOI
- BIO
- HELLO using the original guest OPENDLG.DLL
- Sarien PM
- JIGSAW using the untouched YOSEMITE.BMP
- NEKO / Cat and Mouse using the original NEKO.DLL

NEKO proves several important generic V1 behaviors: resource-only flat-32 LX
DLL loading, zero/overlapping preferred-base relocation, resource HMODULE
publication, public-control subclassing, timers, WC_SLIDER translation and safe
OS/2 HWND_DESKTOP isolation.

## Frozen CMD32 quality-of-life layer

The shell is still the M29P-era command processor architecture, but M31 FINAL
freezes the following additive usability work:

### STARTUP.CMD

On normal interactive launch CMD32 optionally runs:

    C:\OS2\ENV\STARTUP.CMD

through the existing batch engine before the first prompt. Ordinary SET changes
persist; SETLOCAL still scopes correctly. Missing STARTUP.CMD is silent. `-c`
pipeline workers do not rerun it.

### 30-command history + line editor

Interactive input now supports:

- 30-entry in-memory history;
- Up/Down recall, including restoration of the unfinished draft;
- Left/Right/Home/End;
- Backspace/Delete;
- insertion at the cursor;
- horizontally-windowed editing for long command lines.

History/editor storage is static/BSS so it does not consume the small C/386
stack.

### COPY/MOVE/DIR path quality-of-life

- `COPY source` means `COPY source .`.
- Self-copy is rejected before destination open/truncation, including equivalent
  normalized paths.
- Directory operands with a trailing slash are accepted consistently by DIR
  and the shared file-command directory probe. For example:

      DIR bin\
      COPY env.cmd bin\
      MOVE env.cmd bin\

- Drive roots such as `C:\` remain roots and are not stripped incorrectly.

### PS / KILL and session registry

SESMGR now has a small shared os2host32 related-session registry containing:

- OS/2-facing task/session ID;
- host PID;
- executable name;
- title;
- process-creation identity used to avoid PID-reuse mistakes.

`PS` lists live related START/auto-PM sessions. `KILL task-id` stops one such
session through SESMGR / DosStopSession rather than calling Win32 directly from
CMD32. Where supported, related sessions use a Win32 Job Object so termination
can cover the hosted process tree.

SESMGR.5 `DosSMSetTitle` updates the shared title. `DosSMSetTitle(0, title)` is
accepted by this V1 flat-32 personality as “my current registered session.”

Current boundary: PS/KILL intentionally covers related sessions, not every
ordinary/detached DosExecPgm child. Do not silently conflate those lifecycle
models without designing it explicitly.

### PM application auto-routing

CMD32 uses DOSCALLS application-type classification rather than executable-name
heuristics.

- DOSCALLS.163 compatibility `DosQAppType` is present.
- DOSCALLS.323 32-bit `DosQueryAppType` is present for C/386/OS2386.LIB.
- A plain WINDOWAPI executable typed at CMD32 is automatically launched with
  START /PM-like related-session semantics.
- `START /PMC program` preserves the old synchronous, console-attached launch
  path for printf/stderr/loader debugging.
- Non-PM / unknown executables retain the normal attached execution path.

This is why plain `hanoi`, `wmchar`, `neko`, etc. return to the prompt and then
appear in `PS`.

### Tab completion

Interactive Tab performs filesystem completion on the token at the cursor using
the OS/2-facing FindFirst/FindNext layer.

- unique file: completes the name;
- unique directory: completes the name and appends `\`;
- multiple matches: extends to the case-insensitive longest common prefix,
  lists matching names, rings the bell, then redraws the editable command;
- no match: rings the bell and leaves the command unchanged;
- directory prefixes are preserved.

Example with `table1.txt`, `table2.txt`, `table3.txt`:

    del t<Tab>

becomes an editable:

    del table

while listing the three matches.

R1 deliberately does not invent automatic quote insertion for filenames with
spaces; preserve that boundary unless a later change implements quoting
carefully.

## NEKO Z-order host behavior — DO NOT “FIX” WITH TOPMOST

Untouched NEKO repeatedly requests OS/2 `WinSetWindowPos(... HWND_TOP ...,
SWP_MOVE|SWP_ZORDER)` for the cat. The bridge maps the request using normal
Win32 Z-order behavior.

On modern Windows, another foreground/native window such as a WSL terminal can
obscure the cat. When moved away, the cat remains present and animated. This is
accepted host-platform behavior for M31 FINAL.

Do **not** translate NEKO or generic OS/2 HWND_TOP into permanent Win32
HWND_TOPMOST merely to make the cat overlay unrelated native applications. That
would give guest PM windows stronger host privileges than requested and would
change normal Windows behavior.

A future generic PM cleanup may improve HWND_TOP/HWND_BOTTOM/sibling-relative
mapping, but must not introduce application-specific NEKO topmost behavior.

## OS/2 console blue status/title row

The historical blue `OS/2  Ctrl+Esc = Task Manager  Type HELP = help` row is not
implemented in M31. Treat it as future VIO/session-host presentation work, not a
CMD32 drawing feature. Do not fake it inside CMD32 merely for appearance.

## Required environment note

OS/2 module discovery follows OS2LIBPATH semantics. Current-directory lookup is
not implicit. Include `.` when resource/support DLLs such as NEKO.DLL are in the
working directory, for example:

    set OS2LIBPATH=.;C:\OS2\DLL

A bad STARTUP.CMD can therefore make programs appear to launch and immediately
exit if it overwrites OS2LIBPATH incorrectly.

## Final regression gates

Both of these pass in the frozen M31 FINAL source tree:

    make cmd32-tabcomp-check
    make m31g-neko-r19-check

`cmd32-tabcomp-check` chains all post-M31G shell QoL checks:

- STARTUP.CMD
- history/editor
- COPY/path normalization
- PS/KILL
- PM auto-routing / DOSCALLS.323
- Tab completion
- trailing-directory DIR/COPY behavior

The complete M31G R19 gate also remains green through the preserved PM/JIGSAW/
Sarien/NEKO regression chain.

## Build notes

Native Win32 shell/runtime builds use the normal Makefile targets, e.g.:

    make DOSCALLS.dll SESMGR.dll cmd32os2.exe os2host32.exe

The C/386 OS/2 CMD32 executable is built on the Windows/i3 environment with:

    build-os2-shell.cmd

After DLL personality changes, ensure the freshly built DLLs are the copies
actually found through OS2LIBPATH / the chosen C:\OS2\DLL layout.

## Manual shell smoke test

A useful quick smoke test is:

    ver
    ps
    hanoi
    wmchar
    neko
    ps
    kill <task-id>
    dir bin\
    copy env.cmd bin\

and exercise Up/Down history plus Tab completion.

The native shell and the C/386 shell have both been exercised during this QoL
series. Tab completion was user-confirmed working on the first C/386 run. The
TABCOMP R3 trailing-directory COPY fix is protected by the host integration
regression; if restarting on a different machine, include `copy file dir\` in
the first smoke test.

## Freeze rule

Do not modify M31 FINAL simply for cosmetic differences or speculative APIs.
Start the next work as a new milestone from this exact tree. If a concrete M31
regression is found, fix it architecturally and retain/add a regression test.

No executable-name-specific compatibility hacks.
