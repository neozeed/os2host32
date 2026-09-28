# OS2HOST32 — C/386 HACK FAR16 BRIDGE R1 HANDOFF

## Purpose

Extend the existing Microsoft C/386 mixed 32/16 migration-thunk bridge for the
user's larger `hack.exe` specimen without adding general 16-bit CPU execution.

The supplied `os2host32.exe --scan hack.exe` report shows a normal C/386-style
mixed image:

- three objects: 32-bit code, a tiny 16-bit object (0xC4 bytes), 32-bit data;
- 28 SEL16 sites, 14 PTR16:16 sites, 14 PTR16:32 sites;
- 90 external sites;
- `DOSCALLS.425` (`DosFlatToSel`) and `DOSCALLS.426` (`DosSelToFlat`);
- several historical 16-bit VIO/KBD/DOS imports not covered by the previous
  Microsoft C/386 descriptor table.

The loader already implements the proven C/386 model in which the tiny 16-bit
migration thunk is metadata only. The 32-bit transition helper is patched to a
native 32-bit bridge and the 16-bit code is never entered.

## Important diagnosis: DOSCALLS.426 was not the blocker

`DOSCALLS.426` (`DosSelToFlat`) already exists and remains a special
register-ABI helper paired with `DOSCALLS.425` (`DosFlatToSel`). In the native
personality the packed pointer is retained as a flat-address token in EAX, so
both helpers are effectively register-preserving token conversions.

`DOSCALLS.426` therefore deliberately does **not** receive a Pascal-frame
`C386Far16ApiDesc`.

The scan remained fail-closed because the migration-fixup set contained
historical 16-bit API targets missing from the C/386 descriptor table.

## New C/386 far16 descriptor coverage

Historical ordinals/signatures were taken from the supplied OS/2 SDK headers
and Microsoft Programmer's Reference material in the tree.

The descriptor engine stores widths in low-to-high packed Pascal-frame order,
which is right-to-left source argument order when widened onto the native cdecl
stack.

| Module | Ordinal | API | Packed frame widths |
| --- | ---: | --- | --- |
| DOSCALLS | 50 | Dos16Beep | `{2,2}` |
| KBDCALLS | 10 | KbdGetStatus | `{2,4}` |
| KBDCALLS | 11 | KbdSetStatus | `{2,4}` |
| VIOCALLS | 24 | VioReadCellStr | `{2,2,2,4,4}` |
| VIOCALLS | 26 | VioWrtNAttr | `{2,2,2,2,4}` |
| VIOCALLS | 52 | VioWrtNCell | `{2,2,2,2,4}` |

Existing descriptors needed by the same scan, including `DOSCALLS.14`,
`DOSCALLS.32`, `DOSCALLS.89`, VIO cursor/mode calls and KBD calls, are preserved.

## New VIO semantics required by the bridge

`KBDCALLS.10/.11` and `DOSCALLS.50` already had native targets. The three VIO
ordinals did not, so R1 adds real common-VIO implementations and Win32 backend
operations rather than success stubs.

### VioReadCellStr — VIOCALLS.24

- reads character/attribute pairs from the backend-neutral common VIO cell
  image;
- treats the requested length as bytes and returns an even byte count;
- starts at explicit row/column and advances linearly across rows;
- clips at the end of the current screen;
- lazily snapshots host cells only when the common cell image is dirty;
- does not move the logical cursor.

### VioWrtNAttr — VIOCALLS.26

- writes one supplied attribute to `cb` cells from explicit row/column;
- wraps linearly across rows and clips at screen end;
- updates common attribute state and the Win32 console backend;
- does not move the logical cursor.

### VioWrtNCell — VIOCALLS.52

- repeats one character/attribute cell `cb` times from explicit row/column;
- wraps linearly across rows and clips at screen end;
- updates common character and attribute state plus the Win32 backend;
- does not move the logical cursor.

The Win32 backend uses `FillConsoleOutputAttribute` and
`FillConsoleOutputCharacterA`; it does not reintroduce the old eager
whole-console synchronization behavior fixed in VIO R2B.

## Fail-closed execution model remains unchanged

This milestone does **not** make arbitrary 16-bit LE/LX code executable.

The loader continues to accept a mixed image only when all non-flat migration
fixups form the already-recognized Microsoft C/386 migration-thunk pattern and
every thunk target has a known descriptor. Otherwise `--scan` continues to
report:

```text
Direct host path: needs additional execution machinery
```

When all migration thunks are recognized, the existing path reports:

```text
Direct host path: supported via native far16 bridges
```

The supplied `hack.exe` itself was not available in this build environment, so
that exact second scan result is **not claimed yet**. The user's next native
scan is the acceptance test.

## Files changed

- `Makefile`
- `common/api/os2_api_catalog.inc`
- `common/include/os2_vio.h`
- `common/vio/os2_vio.c`
- `common/win32/os2_vio_win32.c`
- `dlls/viocalls/viocalls.c`
- `dlls/viocalls/viocalls.def`
- `loader/os2host32.c`
- `tests/vio/vio-core-check.c`
- `tests/vio/vio-win32-shim-check.c`
- `tools/check_c386_hack_bridge.py`
- `tools/check_vio_life_surface.py`
- `tools/check_sesmgr_r2.py` (extends the frozen-loader whitelist for this intentional later bridge addition)

## Verification

The complete project `make verify` suite passes under both GCC and Clang in
strict C89 mode.

New/extended tests cover:

- exact six new C/386 descriptor ordinals and packed-frame widths;
- preservation of `DosSelToFlat @426` as a register-ABI helper, not a Pascal
  descriptor;
- VIOCALLS.24/.26/.52 exports;
- common VIO read-cell, repeated-attribute and repeated-cell behavior;
- Win32 backend routing for those operations;
- the fail-closed distinction between recognized migration metadata and
  arbitrary 16-bit execution;
- all earlier VIO/DOSCALLS/QUEUE/KBD/SESMGR/NLS/MOU/SIGNAL16 regressions.

## Explicit non-goals

- no 16-bit x86 interpreter/emulator;
- no real 16-bit protected-mode execution;
- no arbitrary 16-bit object entry;
- no handwritten per-API transition assembler;
- no WHP or ReactOS OS2SS changes;
- no VIO API expansion beyond the three APIs required by this specimen;
- no claim that `hack.exe` executes until the user's real scan/run proves it.

## Suggested native acceptance

Rebuild the normal tree, especially `os2host32.exe` and `VIOCALLS.dll`, then:

```text
os2host32.exe --scan C:\proj\vio\os2host32\proj\hack\hack.exe
```

Expected if the tiny 16-bit object consists only of the standard migration
thunks represented by the supplied scan:

```text
C/386 far16     : recognized ... migration thunks
Direct host path: supported via native far16 bridges
```

If the scanner still reports `needs additional execution machinery`, preserve
the new scan output (or supply `hack.exe`) before adding further machinery.
That would mean there is another migration pattern or descriptor target not
visible from the ordinal list alone.

Only after the scan accepts the bridge should `--run` be attempted. Runtime may
then expose ordinary API-semantics gaps unrelated to 16-bit execution.

Suggested status before native scan:

`C386_HACK_BRIDGE_R1_STATIC_VERIFIED_SCAN_PENDING`
