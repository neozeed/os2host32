# Soft386 NE-H3D handoff — 16-bit large memory model

Baseline: NE-H3C. Only `soft386/` changed. Historical DOSCALLS/system DLLs unchanged; 32-bit LE path retained.

## Live host-side result

Untouched supplied 16-bit OS/2 NE LARGE.EXE (Microsoft C large model) runs to completion:

```
soft386: input=LARGE.EXE type=NE entry=0100:0028 stack=0110:0E8F DS=0110 env=0188 imports=9
hello from tiny memory large
soft386: termination=Dos16Exit(EXIT_PROCESS) rc=0 cycles=25349
```

## Root cause and fix

The large-model NE uses a selector-only (2-byte) relocation at segment 1 offset 0x1280, while its logical allocation is 0x1282. H3C incorrectly prechecked every relocation source as requiring four bytes, rejecting this valid fixup. H3D sizes the initial bound check according to relocation type (2-byte selector or 4-byte far address). The chain walk already correctly used type-specific bounds.

Segment 2 is uninitialized data: zero disk offset, allocation 0x200. H3D correctly represents its file payload as zero bytes rather than interpreting length-field zero as a 64-KiB file payload when no disk offset exists. Memory remains zero-filled as before.

## Regression

`make check-ne` now runs VOID, TINY, MEDIUM and LARGE. New exact-output regression checks `hello from tiny memory large\r\n`, no unsupported DOS16 APIs and process rc=0. Host-side `make check-quick` passes, including all existing 32-bit tests. Native Win32 runtime still requires user verification.
