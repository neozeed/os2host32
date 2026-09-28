# OS2HOST32 — C/386 HACK SIGNAL R3
## Microsoft C/386 LIBC signal.asm direct Pascal32 ABI adapter

Baseline: `os2host32-C386-HACK-SWITCH-R2.zip`.

## Trigger

With the switch-table relocation repair in place, the signal-enabled Hack build
advanced into normal relocated 32-bit code and then failed with:

```
os2host32: guest exception C0000005 at 01015065
  access violation: write at 00000001
```

The same application ran when its signal calls were commented out.

## Root cause

The attached earlier signal-enabled specimen (SHA256
`8201dd2e40dc4205cd649373d3f4c6f62ff675663f906143059ccc6a65f278bd`)
shows that Microsoft C/386 `LIBC.LIB(signal.asm)` does not reach
`DOSCALLS.14` / `DOSCALLS.89` through the generic C/386 32->16 migration
thunks.

Its external fixups are direct 32-bit REL32 call sites in object 1:

- object 1 + `0x2508A` -> `DOSCALLS.14` (`DosSetSigHandler`)
- object 1 + `0x250A9` -> `DOSCALLS.89` (`DosSetVec`)

Disassembly around those fixups establishes a hybrid 32-bit Pascal ABI:

```
; SYSSETSIGHANDLER / DOSCALLS.14
push routine
push &prev_handler
push &prev_action
push action
push signal_number
call DOSCALLS.14
; no caller stack cleanup

; SYSSETVEC / DOSCALLS.89
push vector
push handler
push &previous_handler
call DOSCALLS.89
; no caller stack cleanup
```

All arguments occupy 32-bit stack slots, source arguments are pushed
left-to-right, and the callee removes the frame.

The native `DOSCALLS.dll` veneers intentionally use ordinary 32-bit cdecl.
Directly resolving these imports to the cdecl veneers therefore reverses the
argument interpretation and leaves ESP unbalanced.  For `DosSetSigHandler`,
`signal_number == 1` was interpreted as the first `routine` argument and the
following scalar was interpreted as an output pointer, explaining the observed
write through address `0x00000001`.

## R3 fix

`loader/os2host32.c` now creates loader-local Pascal32 adapters for direct
REL32 calls to exactly:

- `DOSCALLS.14` (`SYSSETSIGHANDLER` / `DosSetSigHandler`)
- `DOSCALLS.89` (`SYSSETVEC` / `DosSetVec`)

The adapters are selected only when all of these are true:

1. the image is already recognized as a Microsoft C/386 mixed image;
2. the source object is 32-bit;
3. the external fixup is a direct REL32 import;
4. the byte immediately preceding the displacement is `E8` (`CALL rel32`);
5. the target module is `DOSCALLS` and the ordinal is 14 or 89.

The adapter reads the incoming Pascal32 frame low-to-high and pushes each
DWORD onto a new cdecl frame.  This produces the normal cdecl argument order
expected by the unchanged host veneer.  After the host function returns, the
adapter removes its temporary cdecl arguments and executes `RET n` to remove
the original Pascal32 frame:

- ordinal 14: 5 x DWORD = 20-byte callee cleanup;
- ordinal 89: 3 x DWORD = 12-byte callee cleanup.

The underlying DOSCALLS exports remain cdecl.  This preserves EMX and generic
far16 bridge behavior.

## Not changed

- no 16-bit CPU execution;
- no LDT/selector execution;
- no asynchronous signal delivery;
- no IRET frame synthesis;
- no change to DOSCALLS.14/.89 semantic state;
- no change to the generic C/386 far16 descriptor engine;
- no change to EMX signal/far16 routing;
- no Hack-specific address constants.

## Verification

- full `make verify`: PASS with GCC strict C89;
- full `make verify HOSTCC=clang`: PASS with Clang strict C89;
- C/386 static checker verifies the Pascal32 adapter guards, 32-bit slot
  stepping, and callee-cleanup `RET n`;
- previous C/386 far16 and switch-table checks remain PASS.

Runtime with the R3 adapter remains pending on the user's Win32 host.
