# Soft386 R2C — C/386 DOSCALLS.32 correction

R2B recognized C/386 migration helpers for the VIOCALLS/KBDCALLS subset used by
CMD32, but omitted the frozen loader descriptor for historical DOSCALLS ordinal
32 (`Dos16Sleep`, semantic `DosSleep`).  The Life demo therefore stopped during
helper discovery with `unsupported C/386 helper DOSCALLS.32`.

R2C adds exactly that descriptor: one packed 32-bit millisecond argument.  The
helper is still lowered to 32-bit code; no 16-bit execution is introduced.  At
runtime the packed call is dispatched directly to Soft386 `guest_sleep()`, so
thread blocking/wakeup remains owned by the jar scheduler and no native
DOSCALLS object/state is involved.

The authoritative native C/386 descriptor remains:

    { "DOSCALLS", 32UL, "DosSleep", 1, { 4, ... } }

No existing DLL ABI or source is modified.
