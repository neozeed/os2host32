# Soft386 I386-H2S — deterministic Win32 vessel teardown

## Baseline
H2S is based directly on I386-H2R (bounded PM string copyout), preserving the live-proven Neko fix and all H2K-era PM semantics.

## Problem
A guest could terminate normally through `DosExit(EXIT_PROCESS)` and print the final guest termination record, while the Win32 `soft386_os2.exe` vessel remained alive. Sarien reproduced this after a successful SQ2 run; TelnetPM had shown the same class of symptom earlier.

The old final path performed deep host cleanup only after printing the guest-ended banner: synchronous PM HWND destruction, provider `FreeLibrary`, queue/system bridge unload, DOSCALLS bridge close, etc. That is inappropriate for a one-guest Win32 process vessel once guest process termination is already complete: native windows, DLL mappings, and possibly blocked provider worker threads are process-owned resources and can enter arbitrary teardown/re-entrant paths if synchronously dismantled while the guest is dead.

## H2S architecture
H2S separates reusable bridge deep-close semantics from final Win32 process-vessel teardown.

### New PM process quiesce
`soft386_pm_quiesce_process()`:
- marks the PM bridge closing;
- removes the private PM scheduler idle hook;
- clears all Soft386 callback bindings so no native PM message can re-enter dead guest code;
- deliberately does **not** destroy residual native HWNDs or unload PM provider DLLs.

`soft386_pm_close()` remains unchanged as the deep cleanup API for unit tests and reusable/in-process bridge users.

### New network process quiesce
`soft386_net_quiesce_process()`:
- fences every tracked socket generation before close;
- closes tracked sockets to wake/cancel native socket operations;
- harvests already-finished jobs;
- records whether a worker is still pending;
- does **not** unload SO32DLL/TCP32DLL providers during final process teardown.

`soft386_net_close()` remains the deep-close API and now calls quiesce first, then unloads providers only when no worker is pending.

### Final Win32 vessel path
After normal or forced guest termination:
1. network quiesce;
2. PM quiesce;
3. destroy Tiny386 and free guest modules/image/RAM;
4. flush final diagnostics;
5. call Win32 `ExitProcess(rc)`.

This is intentional process-vessel semantics, not a blind workaround: all guest re-entry paths are first disabled, all jar-owned guest state is released, and process-owned native HWNDs/provider DLLs/remaining native workers are reclaimed atomically by Win32 process termination.

POSIX builds retain the existing full/deep cleanup path and return normally from `main`.

## Diagnostics
A normal Win32 exit should end with:

```
---------------- guest ended -----------------
soft386: termination=DosExit(EXIT_PROCESS) rc=0 cycles=...
soft386: cleanup enter net-quiesce
soft386: cleanup leave net-quiesce pending=...
soft386: cleanup enter pm-quiesce
soft386: cleanup leave pm-quiesce
soft386: cleanup enter guest-memory
soft386: cleanup leave guest-memory
soft386: vessel exit rc=0
```

The process should disappear immediately afterward.

## Validation
Source-side validation completed:
- `make -C soft386 check-quick` PASS
- PM bridge regression PASS
- queue bridge regression PASS
- network bridge/generation-fence regression PASS
- existing Tiny386 compiler warnings remain unchanged

A Win32 RosBE/i386 runtime test is still required. Primary acceptance test: run Sarien, exit normally, verify the above cleanup tail and immediate disappearance of `soft386_os2.exe`. Also regression-test Neko and TelnetPM.
