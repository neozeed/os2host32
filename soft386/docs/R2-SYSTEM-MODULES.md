# Soft386 R2 system-module personality

R2 expands the R1 DOSCALLS-only import personality to the character-mode OS/2
system module set seen in CMD32/NLSINFO: DOSCALLS, VIOCALLS, KBDCALLS and
SESMGR. Direct NLS module ordinals are backed by the same jar-owned common NLS
state as the DOSCALLS NLS aliases.

## Hostcall classes

| Class | Module |
|---:|---|
| `0x01xxxxxx` | DOSCALLS |
| `0x03xxxxxx` | VIOCALLS flat 32-bit |
| `0x04xxxxxx` | KBDCALLS flat 32-bit |
| `0x05xxxxxx` | SESMGR flat 32-bit |
| `0x06xxxxxx` | NLS jar/common |
| `0x07xxxxxx` | validated/lowered C/386 VIO/KBD helper descriptor |

The `0x02xxxxxx` class remains private runtime control.

## C/386 rule

R2 is still a 32-bit execution milestone. It does not switch Tiny386 into a
16-bit code segment to service VIO/KBD. Instead it recognizes the already
live-proven Microsoft C/386 migration helper family used by CMD32/NLSINFO,
validates all associated non-flat fixups, and redirects the 32-bit helper
prologue to a packed-argument hostcall veneer. Unknown non-flat relocation
patterns are rejected.

## Native DLL rule

The existing repository DLLs are unchanged. `soft386_system_bridge.c` resolves
exports by historical ordinal and deep-marshals guest pointers. No guest
linear address is given to a native DLL as a pointer.

VIO and KBD output/input structures use bounded local copies. SESMGR.17 receives
a reconstructed native STARTDATA with deep copies of PgmTitle, PgmName,
PgmInputs, TermQ, Environment, IconFile and ObjectBuffer where present.

## Kernel ownership

Native calls are host service backends, not a second OS/2 kernel. Guest memory,
threads, TIB/PIB, waits, event/mutex objects, process exit and NLS process state
remain Soft386-owned.
