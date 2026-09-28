# OS2HOST32 VIO R2B — console hot-path correction

## Status

`VIO_R2B_HOTPATH_FIXED_RUNTIME_RETEST_REQUIRED`

This is a narrow performance/correctness correction on top of DOSCALLS R2A.
The real MinGW build of R2A compiled successfully, but live testing exposed a
severe console regression: CMD echoed each key only after seconds of cursor
activity, and Life rendered extraordinarily slowly.

## Root cause

The R2 full-cell-state refactor made two host-console operations far too eager:

1. every successful `VioWrtTTY` called the former `opportunistic_resync`,
   which queried size and then re-read the entire console buffer row-by-row
   with both `ReadConsoleOutputCharacterA` and `ReadConsoleOutputAttribute`;
2. `ensure_initialized` called `refresh_size` on every screen-oriented VIO
   call, so direct character-at-a-time programs repeatedly paid
   `GetConsoleScreenBufferInfo` even though the logical VIO mode had not
   changed.

CMD commonly performs tiny stream writes while editing a command line, so the
first bug turned a one-character echo into an O(screen-size) readback.  Life's
cell-at-a-time updates exposed the second polling cost.

## R2B correction

`common/vio/os2_vio.c` now follows these hot-path rules:

- once initialized, ordinary VIO operations use the cached logical rows/columns
  instead of polling host size on every call;
- `VioGetMode` is the explicit normal resize-observation point and refreshes
  dimensions there;
- `VioWrtTTY` performs the backend stream write and **no console readback**;
- if the common cell image already exists, TTY marks it dirty because stream
  control processing/wrapping may have changed arbitrary cells;
- `VioGetCurPos` still queries the actual backend cursor when requested.  This
  is intentional because `DosWrite(stdout)` is not yet routed through VIO and
  can move the same cursor outside the VIO session.

No DOSCALLS API, export, ordinal, far16 descriptor, loader path, KBD behavior,
or Win32 renderer primitive changed in R2B.

## Regression coverage

`tests/vio/vio-core-check.c` now counts host-facing operations and asserts:

- cold `VioWrtTTY` causes zero size polls, zero cell snapshots, and zero cursor
  polls beyond the actual stream write;
- initialized `VioWrtTTY` likewise performs no eager readback and marks the
  common cell image dirty;
- a later `VioGetCurPos` obtains the cursor lazily;
- direct character writes no longer poll the host mode on every call.

Full `make verify` passes with GCC and Clang under strict C89 settings.

## Runtime status

The user's R2A MinGW build is known to compile.  R2B cannot be executed in this
environment, so the authoritative next test is the same Win32 host that exposed
the regression:

```text
make clean
make all

cmd32os2.exe
lifeos2.exe
```

Expected qualitative result: command-line echo should be immediate again and
Life should return to approximately the Phase-1/R1 interactive rendering speed.
