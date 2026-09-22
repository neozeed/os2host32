# Milestone 30H - EMX DosSetRelMaxFH

M30H continues the EMX startup path after M30G locale support.

## New API

`DOSCALLS.382` is implemented as `DosSetRelMaxFH(PLONG delta, PULONG currentMax)`.

The compatibility layer now models the OS/2 relative file-handle ceiling:

- delta `0`: query only
- positive delta: grow the logical HFILE ceiling
- negative delta: shrink it, but never below 20 or an already-open HFILE
- implementation ceiling: 256, matching the current DOSCALLS HFILE table

The allocator also honors the logical ceiling.

With `OS2_TRACE_EMX_SELF=1`, calls are logged as:

```
M30H FILES: DosSetRelMaxFH delta=0 -> max=20 (floor=20)
```

Build with the usual `make clean` / `make tools compat`, then run
`m30h-emx-hi-test.cmd`.
