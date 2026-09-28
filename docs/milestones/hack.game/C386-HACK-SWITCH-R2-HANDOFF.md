# OS2HOST32 — C/386 HACK SWITCH-TABLE RELOCATION R2

## Status

BUILD/STATIC VERIFIED. Native Win32 runtime test pending.

Baseline: `os2host32-C386-HACK-BRIDGE-R1.zip`.

Exact investigated specimen:

- `hack.exe`
- SHA-256: `8201dd2e40dc4205cd649373d3f4c6f62ff675663f906143059ccc6a65f278bd`

## Trigger

The previous bridge milestone recognized all 14 Microsoft C/386 far16 migration
thunks and reported the direct host path as supported, but native execution
later faulted with:

```
os2host32: guest exception C0000005 at 00025051
  access violation: execute at 00025051
  ESP=01054ACC
```

The stack address proves the image had been rebased by approximately
`+0x00ff0000` from its preferred LE layout.  `0x00025051` is inside object 1's
preferred executable range rather than its actual mapped range.

## Root cause

Microsoft C/386 generated absolute switch/jump tables containing preferred
32-bit code addresses.  LINK386 emitted an ordinary internal OFF32 fixup for
the indirect JMP operand that names each table, but emitted no LE fixups for
the DWORD code pointers stored inside the table itself.

The faulting dispatch is:

```
35024:  jmp *%cs:0x34fd0(%ebx)

34fd0:  00025051
34fd4:  00025051
34fd8:  00025039
34fdc:  0002504a
34fe0:  00025045
```

The loader correctly relocates the JMP's reference to table `0x34fd0`, but
without this milestone the table contents remain preferred addresses.  Thus an
indirect jump attempts to execute `0x00025051` instead of the corresponding
rebased address (`0x01015051` in the observed layout).

This is a 32-bit relocation issue.  It is not evidence that the 16-bit C/386
migration object must execute.

## Exact specimen inventory

The conservative detector identifies 21 compiler-style indirect jump tables in
the exact specimen, containing 313 DWORD code-pointer entries total.

The candidate table bases (preferred addresses) and entry counts are:

```
00010104   6
0001532c  14
00015394  15
0001d828  84
0001ff84  10
00024e90  16
000251cc  15
00025364  15
00026078   9
000274d4  20
0002a1d4  16
0002bd88   9
0002ca00   8
0002ccb8   8
0002f618   6
0002fedc   8
0002fff8   8
00031030   6
000326b4  25
00034ed8  10
00034fd0   5
```

## Implementation

`loader/os2host32.c` now contains a narrow switch-table relocation repair.

Eligibility requires all of the following:

1. The image has already passed the existing C/386 far16 bridge recognizer.
2. The table is named by a real internal LE OFF32 relocation.
3. The relocation source is attached to one of the observed C/386 indirect
   `CS:` jump encodings:
   - `2e ff 24 85 disp32`
   - `2e ff 24 45 disp32`
   - `2e ff a3 disp32`
4. The candidate table has at least two consecutive DWORD entries.
5. Every patched entry is a preferred address inside a mapped executable LE/LX
   object.

Each entry is translated object-by-object to:

```
actual_object_mapping + (preferred_pointer - preferred_object_base)
```

The scan stops at the first DWORD that does not identify an executable
preferred object address.  This avoids applying a blanket relocation delta to
arbitrary data.

The repair occurs while objects are still PAGE_READWRITE, during ordinary
internal OFF32 fixup application and before final object protections.

## Deliberately unchanged

- All 14 existing C/386 far16 migration thunks remain descriptor-driven.
- The 16-bit object remains metadata and is not executed.
- DOSCALLS.425/426 remain the established register-ABI token helpers.
- No 16-bit interpreter/emulator or LDT execution was introduced.
- No VIO/KBD/DOSCALLS ABI changes were introduced.

## Verification

- full `make verify` with GCC: PASS
- full `make CC=clang verify`: PASS
- existing C/386 HACK bridge architecture check: PASS, extended to require the
  narrow switch-table repair guards
- exact `hack.exe` offline analysis: 21 eligible tables / 313 entries
- faulting table includes exact stale target `0x00025051`

Real i686 MinGW/native runtime remains pending.

## Expected native diagnostic

On a relocated native run, non-quiet loader output should now include lines
similar to:

```
C/386 switch tbl: 01024FD0 5 entries rebased
```

(the actual mapped table address depends on the chosen arena).

The previous execute AV at preferred `0x00025051` should no longer occur.  If a
new fault appears, preserve the complete loader output and exception register
dump; do not broaden this repair heuristically without identifying the new
address source.
