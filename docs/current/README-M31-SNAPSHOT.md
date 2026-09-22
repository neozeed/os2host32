# os2host32 — Milestone 31A FINAL

**Frozen V1 checkpoint: original Microsoft OS/2 2.0 Beta-2 WMCHAR is now an
interactive Presentation Manager regression on the native Win32 compatibility
path.**

Read these first:

1. `MILESTONE31A-FINAL.md` — what M31A proved and the architectural discoveries.
2. `START-HERE-M31B-PM.md` — the next PM target and freeze rules.
3. `MILESTONE30-FINAL.md` — prior frozen V1/EMX handoff.
4. `START-HERE-M31-PM.md` — original M31 starting plan, retained for history.

## Build and regression

    make clean
    make tools compat
    make m31a-final-check
    m31a-final-test.cmd

`examples/m31a-wmchar/WMCHAR.EXE` is the untouched historical SDK executable.
The M31A runtime may promote very small eligible PM stacks to provide safe
headroom for modern USER32/GDI calls; it does not rewrite the historical image.

The historical milestone READMEs and analysis files remain in the tree so later
work can recover design decisions without reconstructing the conversation.


## M31G NEKO active branch

Current additive NEKO work is documented in `START-HERE-M31G-NEKO.md`.
M31G R8 adds PMWIN.727 `WinDestroyPointer` with compatibility-owned HPOINTER
tracking; M31F FINAL R7 remains the frozen regression baseline.

M31G R18: NEKO now reaches real animation. WinSetWindowPos move-only calls use
the current window size for OS/2 bottom-left -> Win32 top-left conversion when
SWP_SIZE is absent, fixing the observed 32-pixel-per-move Y drift.

### Current M31G branch: R19
R19 follows the runtime-proven OS/2 2.00 GA NEKO milestone. It adds WC_SLIDER
translation through native Win32 trackbars and isolates HWND_DESKTOP enumeration
from unrelated host windows so guest PM desktop operations cannot hide Explorer.

## M31G FINAL

M31G R19 is runtime-proven and frozen: untouched OS/2 2.00 GA NEKO runs and
animates on Windows 10 x64 with its original NEKO.DLL resources, working
WC_SLIDER controls, and an isolated/safe HWND_DESKTOP namespace. See
`MILESTONE31G-FINAL.md` and `NEXT-CHAT-HANDOFF.md` before starting new PM work.
