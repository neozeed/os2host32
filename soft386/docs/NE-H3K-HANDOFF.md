# Soft386 NE-H3K — 16-bit C runtime command line

Baseline: user-provided trimmed H3J-derived tree. All runtime modifications remain within `soft386/`. No 32-bit personality/system DLL/CPU modifications.

## Root cause
`build_ne_environment()` assembled an environment followed by `program\0arguments\0`, but returned the offset of `arguments` in startup BX. The Microsoft 16-bit CRT interprets BX as the start of the two-string pair, so `ARGS.EXE 1 2 3` became argc=1, argv[0]="1 2 3".

## Fix
Return the start offset of `program` rather than the argument string; keep the two consecutive NUL-terminated strings. Quote individual host arguments containing spaces to keep CRT tokenization intact, and validate 16-bit environment bounds. The code and ABI are unchanged for 32-bit LE/LX.

## Regressions
- Added exact user-provided `ARGS.EXE` fixture under `soft386/tests/fixtures/args-ne16.exe`.
- New `tests/check-ne-args.py` tests zero arguments, `1 2 3`, and `"two words" 3` including argv[0].
- Wired into `make check-ne`.
- `make check-quick CFLAGS='-O0 -std=gnu99 -w'` passed after restoring executable permissions on binaries in the trimmed archive. Covers LINK386 loader boundary, LIB object-add and creation, five memory models, 32-bit LE, guest threads, synchronization, memory, and 32-bit bridges.
- Native Windows runtime and default `-O2` compiler build remain unverified in this package.

## Follow-up
The historical 16-bit NLS.4 LINK386 import is still unsupported and deliberately fails explicitly. Expand NLS/KBD/MSG 16-bit ABI bridges as separately tested steps.
