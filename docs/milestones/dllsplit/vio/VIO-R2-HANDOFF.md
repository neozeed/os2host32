# OS2HOST32 — VIO R2 handoff
## Common character/attribute state with Win32 Console backend

## Status

**VIO_R2_FULL_CELL_STATE_STATIC_VERIFIED_RUNTIME_PENDING**

The preceding Phase-1 VIO/Life implementation was reported by the user as
working well enough to proceed.  This R2 package performs the requested
backend-neutral state refactor on that working baseline and passes all host
regressions available here.  The newly changed R2 code has not been run on a
real Windows console in this worker environment, so the final R2 live-pass
label is intentionally pending.

## Scope

R2 changes VIO architecture only.  It does not add or alter the Life-required
VIO ordinals, C/386 descriptors, KBD behavior, DOSCALLS.32 sleep bridge, LE/LX
loader architecture, PM/AVIO, ANSI, WHP, ReactOS OS2SS, or 16-bit CPU
execution.

## What changed

1. Added backend-neutral `Os2VioCell { character, attribute }` state to each
   `Os2VioSession`.
2. Added dynamic screen-buffer allocation tied to actual backend rows/columns;
   no fixed 80x25 assumption was introduced.
3. Added backend `read_cells` support.  Win32 seeds/resynchronizes common state
   with `ReadConsoleOutputCharacterA` plus `ReadConsoleOutputAttribute`.
4. `VioWrtCharStr` now updates the common characters after successful backend
   rendering while preserving each cell's prior attribute.
5. `VioWrtCharStrAtt` updates both common character and OS/2 attribute state
   after successful rendering.
6. `VioScrollUp` mirrors successful partial/full scroll mutations into the
   common cell image, including the Life `0xffff` clear idiom.
7. `VioWrtTTY` remains the proven `WriteFile` stream path.  On real console
   output it re-snapshots the cell/cursor state afterwards; redirected stdout
   still succeeds without requiring a console buffer.
8. Added `os2_vio_session_destroy` for deterministic common-state lifetime in
   tests/future process teardown.
9. Expanded common tests to prove initial cell snapshot, raw CP437 byte state,
   attribute preservation, attributed writes, partial scrolling, full clear,
   TTY resynchronization, and cell-buffer teardown.
10. Extended static architecture checks so future changes cannot silently
    remove the common cell image or Win32 snapshot path.

## Architecture

```text
VIOCALLS.dll ABI facade
    |
    v
common/vio/os2_vio.c
    OS/2 validation + mode/cursor + cell image
    |
    v
Os2VioBackendOps
    |
    v
common/win32/os2_vio_win32.c
    Win32 snapshot/render/TTY implementation
```

No `windows.h`, `HANDLE`, `COORD`, or `CHAR_INFO` leaks into the common VIO
header or semantic source.

## Verification performed

`make verify` passes:

- personality core regression;
- NLS table/core regressions;
- API catalogue regression;
- shared-personality wiring regression;
- `vio-core-check` with full common cell-state checks;
- `vio-win32-shim-check` compiling the production VIO facade/common/backend
  against the deterministic Win32 console shim;
- `check_vio_life_surface` including the new R2 architecture assertions.

The two VIO suites also compile warning-clean with Clang in strict C89 mode and
both pass.

## What to run on the Windows test host

Build the tree normally, then rerun the exact Phase-1 Life acceptance:

```text
os2host32 --run lifeos2.exe
os2host32 --run lifeos2.exe C
```

Exercise:

- screen clear and instruction output;
- raw Life cell characters;
- color attributes;
- cursor hide/show and editing position;
- arrows, Insert/Delete, S, +/-, ESC;
- generation/population in-place updates;
- redirected VioWrtTTY/CMD regression if part of the normal harness.

Then rerun the existing C/386 far16 VIO/KBD, EMX bridge, loader and DOSCALLS
regressions.

If those remain clean, promote the milestone to:

**VIO_R2_COMMON_STATE_WIN32_BACKEND_PASS**

## Known boundaries

- Win32 cursor shape remains an approximation of OS/2 scan-line semantics.
- OS/2 attribute bit 7 (blink) is not representable directly in this Win32
  renderer and is dropped for display, while the common image retains the
  original OS/2 attribute byte.
- `DosWrite(stdout)` is intentionally not routed through VIO in this milestone;
  external writes can temporarily make the VIO shadow image stale.  VioWrtTTY
  itself is resynchronized after native console writes.
- Win32 Console is still the only backend.

## Stop condition honored

R2 stops at common text-mode VIO state plus Win32 rendering.  No ReactOS,
WHP, PM/AVIO, ANSI/VT, VioRegister, or broader loader work was added.
