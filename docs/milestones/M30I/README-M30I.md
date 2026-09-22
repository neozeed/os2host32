# Milestone 30I - EMX OS/2 memory suballocation

M30I continues the real EMX startup path after M30H's `DosSetRelMaxFH` support.

## New DOSCALLS APIs

The OS/2 32-bit memory-suballocation family is now exported:

- ordinal 344: `DosSubSetMem(PVOID base, ULONG flags, ULONG size)`
- ordinal 345: `DosSubAllocMem(PVOID base, PPVOID block, ULONG size)`
- ordinal 346: `DosSubFreeMem(PVOID base, PVOID block, ULONG size)`
- ordinal 347: `DosSubUnsetMem(PVOID base)`

The compatibility implementation preserves the observable OS/2 pool model:

- the supplied memory object remains the backing store; no host pointer is returned to guest code
- the first 64 bytes are reserved for the pool manager
- allocation sizes are rounded upward to 8-byte boundaries
- freed adjacent ranges are coalesced
- `DosSubSetMem` can grow an existing pool inside the mapped memory object
- pool metadata itself is held by `DOSCALLS.dll`, avoiding Win32 pointers in guest memory

M30's current `DosAllocMem` commits its backing memory up front.  Therefore sparse-pool
commit/decommit policy is intentionally not emulated yet, and serialized/shared-pool
behavior remains process-local.  These limitations do not affect the normal single-thread
EMX startup path we are exercising with `hi.exe`.

With `OS2_TRACE_EMX_SELF=1`, look for lines such as:

```
M30I SUBMEM: DosSubSetMem base=... flags=... size=...
M30I SUBMEM: DosSubAllocMem base=... ask=... rounded=... -> ...
M30I SUBMEM: DosSubFreeMem ...
```

Build with the usual:

```
make clean
make tools compat
```

Then place the known-good bound `hi.exe` and `emx.dll` beside the included unbound
`hi` sidecar and run `m30i-emx-hi-test.cmd`.
