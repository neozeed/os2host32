# Soft386 I386-H1B — DOSCALLS Dispatch Inventory

This inventory records the current 32-bit DOSCALLS ownership split after the H1 architecture reset. It is a coverage map, not an OS/2 semantic specification.

## H1B result

- Eight DOSCALLS exports are now descriptor-driven scalar/opaque calls with no API-specific dispatch body: 209, 220, 221, 254, 257, 263, 272, and 286.

- DOSCALLS.286 additionally carries the generic `MAY_BLOCK` execution flag and therefore uses the scheduler-safe asynchronous scalar worker.

- Pointer/string/buffer calls remain explicit marshallers.

- Guest CPU/process-local memory, thread, synchronization, module, exception/signal and selector operations remain jar-owned.

## Generic scalar descriptors

| Ordinal | API | Args | Reason |
|---:|---|---:|---|
| 209 | `DosSetMaxFH` | 1 | scalar/opaque only; native service owns semantics |
| 220 | `DosSetDefaultDisk` | 1 | scalar/opaque only; native service owns semantics |
| 221 | `DosSetFHState` | 2 | scalar/opaque only; native service owns semantics |
| 254 | `DosResetBuffer` | 1 | scalar/opaque only; native service owns semantics |
| 257 | `DosClose` | 1 | scalar/opaque only; native service owns semantics |
| 263 | `DosFindClose` | 1 | scalar/opaque only; native service owns semantics |
| 272 | `DosSetFileSize` | 2 | scalar/opaque only; native service owns semantics |
| 286 | `DosBeep` | 2 | scalar/opaque only; native service owns semantics; may block |

## Current ownership categories

| Category | Count | Notes |
|---|---:|---|
| Generic scalar native bridge | 8 | no per-API call body |
| Native bridge total | 38 | scalar descriptors plus explicit marshallers |
| Explicit native marshallers | 30 | guest strings/buffers/structures/output pointers |
| Explicit jar-owned list | 40 | guest-local CPU/address-space/process state |
| DOSCALLS catalog entries | 98 | canonical API catalogue |

## Next H1 audit

1. The duplicated native-bridge ordinal whitelist and argument-count switch have now been replaced by one descriptor registry.
2. Refine `MARSHALLED` into more specific reusable ABI classes (`STRING`, `BUFFER`, `STRUCT`, `HANDLE`, `CALLBACK`, or custom graph marshaller) where that reduces duplicated copy logic.
3. Extend descriptor-driven forwarding to the other service DLL bridges.
4. Apply the same descriptor/admission separation to PM without moving PM behavior into Soft386.

