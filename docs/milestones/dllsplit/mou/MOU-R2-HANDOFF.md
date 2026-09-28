# OS2HOST32 — MOUCALLS R2 handoff

## Baseline

Built from `os2host32-NLS-R2.zip`, whose NLS state/backend split was live-tested
successfully by the user before this milestone.

## Result

MOUCALLS is introduced directly in backend-neutral form.  There was no existing
`dlls/moucalls` implementation in the baseline to preserve/refactor.

### Architecture

- `dlls/moucalls/moucalls.c`: thin public ABI veneer.
- `common/mou/os2_mou.c`: OS/2 mouse session/state/event semantics.
- `common/include/os2_mou*.h`: portable state and backend contract.
- `common/win32/os2_mou_win32.c`: Win32 pointer/button sampling backend.

### Historical ordinals

The ordinal map is taken from the supplied historical `sdk/os2h/bseord.h`:

1 GetPtrShape, 2 SetPtrShape, 3 GetNumMickeys, 4 GetThreshold,
6 GetScaleFact, 7 FlushQue, 8 GetNumButtons, 9 Close, 10 SetThreshold,
11 SetScaleFact, 13 GetNumQueEl, 14 DeRegister, 15 GetEventMask,
16 SetEventMask, 17 Open, 18 RemovePtr, 19 GetPtrPos, 20 ReadEventQue,
21 SetPtrPos, 22 GetDevStatus, 23 Synch, 24 Register, 25 SetDevStatus,
26 DrawPtr, 27 InitReal.

All are represented in the canonical API catalogue and the C/386 far16
bridge-descriptor table.  `MOUCALLS` is recognized as a native personality DLL.

### Common semantics

Implemented common state includes HMOU lifetime, event mask/filtering,
translated event queue, absolute and relative/mickey reporting, device status,
logical pointer position/shape, scale factor, threshold and draw/exclusion
state.  `os2_mou_try_read()` provides the future nonblocking WHP/OS2SS seam.

### Win32 backend choice

The backend intentionally avoids `ReadConsoleInputA`/`PeekConsoleInputA` so it
cannot consume KBDCALLS keyboard records.  It samples the GUI pointer/buttons
and translates the pointer into console-cell coordinates.  See
`MOU-BACKEND-DESIGN.md` for limitations.

### Verification

On the build host:

- strict-C89 GCC common mouse test: PASS;
- actual MOUCALLS veneer test: PASS;
- deterministic Win32 backend shim test: PASS;
- MOU static architecture/ordinal check: PASS;
- complete existing `make verify` under GCC: PASS;
- complete existing `make verify` under Clang 17: PASS.

Real `i686-w64-mingw32-gcc` compilation and native Windows runtime remain for
the user environment.

### Suggested live test

    make clean
    make MOUCALLS.dll
    make all
    make verify

Then build/run `tests/mou/c386-mou-probe.c` with the normal Microsoft C/386
mixed-mode toolchain.  It exercises MouOpen, button/mickey queries, pointer
position, event mask, DrawPtr, NOWAIT event reading and MouClose.

## Status

`MOU_R2_COMMON_STATE_WIN32_BACKEND_STATIC_VERIFIED_RUNTIME_PENDING`
