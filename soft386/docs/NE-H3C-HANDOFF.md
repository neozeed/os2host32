# SOFT386 NE-H3C: 16-bit medium-model printf

Base: NE-H3B. Additive Soft386-only change; all system DLLs/common cores untouched.

## Guest
Original MEDIUM.EXE (OS/2 1.x NE, 2 segments, entry 1:0022; 1 imported module, DOSCALLS).
Actual stdout (CRLF): `hello from tiny memory medium`. Returns 0.

H3B already loaded and executed guest but logged unsupported historical DOS16.92 and DOS16.49.
H3C implements ordinal 92 Dos16GetVersion(PUSHORT) using the existing OS/2 2.0 personality (returns 0x0200; major byte high), and 49 Dos16GetMachineMode(PBYTE) returning protected OS/2 mode=1, both with 16-bit Pascal 4-byte far pointer stack cleanup. These are guest-facing compatibility responses, not a new host DLL.

## Regression
`cd soft386 && make check-ne` tests untouched VOID, TINY, MEDIUM; MEDIUM checks exact CRLF stdout, no unsupported DOS16, and zero exit.
`make check-quick` passes including existing LE/thread/sync/memory/bridge regressions.
No full expanded `make check` claimed. Windows live testing still pending.

## Design boundary
Do not change Win32 system DLLs or the existing 32-bit LE/LX loader. Keep all new 16-bit ABI semantics inside Soft386. Further memory models should be tested as independent fixtures, not assumed to be compatible merely from medium passing.
