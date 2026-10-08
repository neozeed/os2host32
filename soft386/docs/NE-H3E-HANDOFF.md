# Soft386 NE-H3E: compact and huge memory-model regressions

Built from the full H3D source snapshot. Only soft386/Makefile, tests, and this documentation changed. No Soft386 runtime source, system DLLs, or bridge logic changed.

## New regression inputs

- COMPACT.EXE (Microsoft C /AC): 3 segments, 13 relocation records, 9 ordinal DOSCALLS imports; stdout `hello from compact memory huge\r\n`; Dos16Exit rc=0.
- HUGE.EXE (Microsoft C /AH): 3 segments, 14 relocation records, 9 ordinal DOSCALLS imports; stdout `hello from tiny memory huge\r\n`; Dos16Exit rc=0.
- Both have the same nine imported ordinal values as LARGE.EXE: 5, 34, 38, 49, 58, 77, 89, 92, 138.
- HUGE and LARGE share the same segment layout and relocation signature; these short programs do not independently validate huge-pointer normalization or data spanning a 64 KiB boundary.

Run `make check-ne` or `make check-quick` in `soft386/`.

## Next test

Use an NE program with a genuinely >64KiB allocation and reads/writes that cross a 64KiB boundary, and another with distinct far code/data pointers; this distinguishes model-specific runtime functionality from startup/printf alone.
