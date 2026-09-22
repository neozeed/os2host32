# NEXT CHAT HANDOFF — START HERE — os2host32 M31 FINAL

We are continuing the **os2host32** project from a completed Milestone 31.

## Project goal and branch rule

Run Microsoft OS/2 2.x-era **32-bit LE/LX applications directly on Win32** by
loading the original x86 guest code and translating OS/2 APIs through native
compatibility DLL personalities such as DOSCALLS, PMWIN, PMGPI, PMSHAPI,
SESMGR, VIOCALLS and KBDCALLS.

This is the **V1/native-Win32 track**. Do not introduce emulation, WHP, V86 or
other V2 mechanisms into this branch. A separate Win64/WHP V2 proof-of-concept
exists and is frozen for later. EMX exploration is also frozen.

## Frozen baseline to use

**M31 FINAL** is the current immutable working baseline.

Read `MILESTONE31-FINAL.md` first. `MILESTONE31G-FINAL.md` contains the detailed
NEKO architecture. Older M31/M30 notes remain historical reference only.

M31 FINAL consists of:

- M30 FINAL flat-32 LE/LX direct-host runtime;
- M31F FINAL R7 PM regression baseline;
- M31G R19 FINAL untouched OS/2 2.00 GA NEKO success;
- post-freeze CMD32 QoL work through **TABCOMP R3**.

## Runtime-proven application set

Preserve these together:

- WMCHAR
- HANOI
- BIO
- HELLO + original guest OPENDLG.DLL
- Sarien PM
- JIGSAW + untouched YOSEMITE.BMP
- NEKO + original NEKO.DLL

Do not regress them while starting the next milestone.

## CMD32 capabilities frozen in M31 FINAL

CMD32 now provides:

- automatic `C:\OS2\ENV\STARTUP.CMD` on interactive launch;
- 30-command Up/Down history;
- editable recalled/current commands with Left/Right/Home/End, Backspace,
  Delete and insertion;
- `COPY source` => destination `.`;
- normalized self-copy protection;
- consistent trailing-directory operands such as `DIR bin\`,
  `COPY file bin\`, `MOVE file bin\`;
- `PS` and `KILL task-id` for related SESMGR sessions;
- shared task registry with PID/executable/title and `DosSMSetTitle` updates;
- automatic PM-session routing for WINDOWAPI executables;
- `START /PMC program` to force synchronous console-attached PM debugging;
- DOSCALLS.163 and **DOSCALLS.323 DosQueryAppType** compatibility;
- filesystem Tab completion with longest-common-prefix expansion, candidate
  listing, bell, editable redraw and trailing `\` for unique directories.

The shell banner/VER identifies the QoL TABCOMP branch rather than old M29P.

## Important known behavior — do not change casually

### NEKO Z-order

NEKO itself repeatedly requests HWND_TOP with SWP_ZORDER while moving the cat.
Modern Windows may still allow another active native application to obscure it.
This is accepted for M31 FINAL. **Do not map it to Win32 HWND_TOPMOST.** Preserve
normal host Windows behavior unless a future generic OS/2 Z-order design proves
a better mapping.

### OS2LIBPATH

Current-directory DLL lookup is not implicit. If NEKO.DLL or another guest
module is in cwd, OS2LIBPATH must contain `.`. A recommended baseline is:

    set OS2LIBPATH=.;C:\OS2\DLL

Be careful that `C:\OS2\ENV\STARTUP.CMD` does not overwrite this with an
unrelated C/386 development environment.

### PS/KILL scope

The current task registry intentionally models related START/auto-PM sessions.
Ordinary/detached DosExecPgm children are not yet unified into a global process
list. If extending process management, design that lifecycle boundary explicitly
rather than pretending the two models are identical.

### Historical blue VIO status row

The blue `OS/2 Ctrl+Esc = Task Manager / Type HELP = help` line belongs to the
future VIO/session-host presentation layer, not CMD32 itself. It is not an M31
gap requiring a cosmetic shell hack.

## Regression gates before and after new work

Run:

    make cmd32-tabcomp-check
    make m31g-neko-r19-check

Both pass in the M31 FINAL package.

If a new change breaks an existing sample or CMD32 QoL check, stop expansion and
fix the regression architecturally before proceeding.

## Build/runtime reminders

Native Win32 components use the Makefile. Common rebuild set:

    make DOSCALLS.dll SESMGR.dll PMWIN.dll PMGPI.dll PMSHAPI.dll \
         KBDCALLS.dll VIOCALLS.dll cmd32os2.exe os2host32.exe

The Microsoft C/386 OS/2 shell is built on the Windows/i3 system with:

    build-os2-shell.cmd

When replacing compatibility DLLs, verify the copies under the active
`C:\OS2\DLL`/OS2LIBPATH are the newly built files.

## How to begin the next milestone

Start a **new milestone number/branch** from this exact M31 FINAL package. Do not
continue naming new work M31G or TABCOMP R4 unless it is a true regression fix
to this freeze.

For the next untouched OS/2 application:

1. scan the original binary;
2. confirm it is suitable for the V1 flat-32 direct-host path;
3. run it unchanged;
4. stop at the first real loader/API/behavior boundary;
5. implement only a generic compatibility behavior;
6. add a regression test;
7. rerun both final gates above;
8. never add executable-name-specific hacks.

The separate `E.EXE` exploration remains unfinished and may be resumed by
running the untouched binary and following its next actual failure. Its last
known opportunistic addition was NLS.5 `DosQueryCtryInfo` during M31G R17.

## What not to reopen without evidence

Do not rework these merely because they look unusual:

- NEKO resource DLL packed relocation;
- public-control subclass/timer bridge;
- NEKO slider bridge;
- HWND_DESKTOP process isolation;
- move-only WinSetWindowPos coordinate conversion;
- normal-Windows NEKO Z-order behavior;
- STARTUP/history/Tab behavior already covered by QoL tests.

M31 FINAL is a preservation point. New capability belongs in the next milestone.
