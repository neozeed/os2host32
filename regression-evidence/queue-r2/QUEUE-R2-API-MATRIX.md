# QUECALLS R2 API matrix

| API | Ordinal | R2B implementation | QUECALLS R2 implementation | Common-owned semantics | Win32 backend role | Runtime status |
|---|---:|---|---|---|---|---|
| DosReadQueue | 9 | monolithic Win32 DLL | thin veneer -> common queue | validation, ownership, dequeue, outputs, NOWAIT | lock + availability wait | static verified; live pending |
| DosWriteQueue | 14 | monolithic Win32 DLL | thin veneer -> common queue | priority validation, entry insertion, FIFO/LIFO/priority | lock + availability signal + PID | static verified; live pending |
| DosOpenQueue | 15 | monolithic Win32 DLL | thin veneer -> common queue | case-insensitive name lookup, owner/handle result | lock only | static verified; live pending |
| DosCreateQueue | 16 | monolithic Win32 DLL | thin veneer -> common queue | name/order validation, handle/object creation, owner state | lock + PID + availability object | static verified; live pending |

## ABI

`dlls/quecalls/quecalls.def` remains unchanged:

```text
LIBRARY QUECALLS
EXPORTS
    DosReadQueue       @9 NONAME
    DosWriteQueue      @14 NONAME
    DosOpenQueue       @15 NONAME
    DosCreateQueue     @16 NONAME
```

## Known approximations / intentionally retained limits

- native queues remain process-local;
- no Close/Peek/Purge/Query export yet;
- `hev` notification on NOWAIT reads remains ignored by the native four-call surface;
- queue payload memory is not copied; the 32-bit address/value is retained opaquely;
- maximum native common-session queues: 16;
- maximum entries per queue: 256.
