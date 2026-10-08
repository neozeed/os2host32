# Soft386 OS/2 NE-H4C — C3 loader and Microsoft C/386 three-stage proof

Date: 2026-10-08. Baseline: `os2host32-SOFT386-NE-H4B-exec-env.zip`.

## Input and observed failure

The original C3_386.EXE (SHA-256 `690d7a413221c8399d192baa6c92f2dcc312dd0e0f1e246951bb07bc2e5c34b8`, 132,172 bytes) is a valid OS/2 1.x NE image with 5 segments and 54 Microsoft x87 OSFIXUP records. In H4B, it fails the OSFIXUP byte-pattern test and prints `unsupported or malformed OS/2 NE image`; in Windows parent execution the rejected child exits with `0xC0000005` on the current diagnostic trace, indicating that the host process also suffered an access violation. The C3 failure must not be mistaken for a missing file.

## Root cause 1: overly strict H3Y OSFIXUP validation

C3 segment 2 has two valid additive OFF16 OSFIXUP records with the expected 8-byte form (source 05, flags 07, IDs 5 and 6, reserved word 0). Their sources are segment-relative offsets `0x916e` (ID 5) and `0x9166` (ID 6). Both locations contain an immediate operand `00 00` in a `MOV AX,0000h` instruction sequence, not the `WAIT` opcode `9B` or `NOP WAIT` bytes `90 9B`. H3Y mistakenly required opcode-pattern matching at all OSFIXUP sources; this is not mandated by the relocation table format. H4C retains source-range, type, flags, OSFIXUP ID and reserved-field checks, and leaves image bytes unchanged for the native/emulated x87 path. It does not install software-coprocessor exception shims.

## Root cause 2: missing C3 startup APIs

After NE acceptance, C3 runs and reaches additional legitimate OS/2 1.x DOSCALLS ordinals:

* `DOSCALLS.43` — `DosCreateCSAlias(SEL data, PSEL code)`, 6-byte Pascal cleanup. Creates a new executable selector referencing the original private data selector's base and limit, leaves original data selector in place, rejects invalid selectors, and returns the selector via a validated guest far pointer.
* `DOSCALLS.52` — `DosDevConfig(PVOID out, USHORT item, USHORT reserved)`, 8-byte Pascal cleanup. Returns one byte. Items 0–6 are modeled with a simple emulated PC inventory; item 3 reports the emulated x87 present. Other items/invalid reserved fields return error 87. This is a minimal synthetic device configuration, not complete host-device discovery.
* `DOSCALLS.39` — `DosFreeSeg(SEL)`, 2-byte Pascal cleanup. Invalid/freed selectors fail, dynamically created selectors are released (descriptor cleared). Does not reclaim backing physical pages; allocations are per-process and fixed-size in this vessel.

The previous missing API handling returned status 1 but used no Pascal stack cleanup, resulting in an invalid far-transfer during the error path. H4C no longer crashes when C3 is launched standalone without its private compiler environment: it reports C1042 and exits rc 1, as expected for a compiler stage invoked without the driver.

## Independent local end-to-end result

Linux GCC unoptimized diagnostic build ran original unmodified CL386.EXE (42,705 bytes) on `void.c` with source contents `int main(void){return 0;}`. H4B environment-passing logic was preserved. CL386 launched the originals in order:

* C1_386.EXE: `Dos16Exit` rc=0, 48,275 cycles.
* C2_386.EXE: `Dos16Exit` rc=0, 86,453 cycles.
* C3_386.EXE: `Dos16Exit` rc=0, 82,351 cycles.
* Parent CL386.EXE: `Dos16Exit` rc=0, 90,382 cycles.

Compiler generated **`void.OBJ`, 234 bytes**, identified by the host `file` utility as `8086 relocatable (Microsoft), "void.c"`, SHA-256 `0a2e579b8c12c5e52dc859d5ee8f0d780398f0a19ba6c609eb77b2b1aebac940`. A real three-stage compilation is proven on the Linux diagnostic host. Windows runtime for H4C remains **pending user confirmation**; object-linking / runtime semantics beyond successful compiler output are not claimed.

`make -C soft386 check-ne` passed, including NLS module references, NE argv, LINK386, LIB, tiny/medium/compact/large/huge printf regressions. Existing build warnings remain; no new warnings specific to the patch were detected.

## Windows test

Rebuild from the full source (or apply `SOFT386-NE-H4C-C3.patch` to the exact H4B snapshot), update `C:\OS2\soft386_os2.exe`, ensure CL386 and its original C1/C2/C3 stages are together, and run:

```bat
set "OS2HOST32_LOADER=C:\OS2\soft386_os2.exe"
\OS2\soft386_os2.exe --trace-hc --run CL386.EXE -c void.c > cl386-h4c.log 2>&1
echo %ERRORLEVEL%
findstr /i "CreateCSAlias DevConfig FreeSeg child_exited termination unsupported malformed" cl386-h4c.log
dir void.obj
```

Expected when `void.c` contains valid syntax: 0 for all child stages and parent, with an `.OBJ` produced. `C3_386.EXE` on its own may report C1042/exit 1 because it expects `MSC_CMD_FLAGS` and `_C_FILE_INFO` supplied by CL386. No Microsoft compiler binary is included in the H4C source ZIP.

## Scope boundaries

Only `soft386/src/soft386_os2.c` changed. H4B guest environment handoff, H4A argument-block fix, H3X `DosQFileMode`, H3W named imports, and H3Y x87 fixup inventory are preserved. H4C is not a general test of every NE OSFIXUP software-emulation configuration. Older RC/RCPP integration and C3 executable output/linking remain future work.
