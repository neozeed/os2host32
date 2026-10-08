# Soft386 NE-H3J: LINK386 first-loader boundary

Baseline: NE-H3I. Scope: only `soft386/`.

## New binary
`link386.exe` is a genuine OS/2 1.x NE image: file length 133102 bytes; NE header at 0x4E88; five segments; four import modules in order `DOSCALLS`, `KBDCALLS`, `NLS`, `MSG`. Entry segment 2 offset 0x514E; automatic data segment 5.

## Previous failure
The H3I loader rejected NE relocation source type 5 (16-bit offset), present 23 times across the linker code segments. H3I handled source types 2 (selector) and 3 (far pointer) only.

## H3J implementation
- Apply internal type-5 offset relocations (target offset) across chained fixup sites.
- Apply imported type-5 offsets to Soft386's synthetic import stub offset.
- Key imported stubs by `(module index, ordinal)` rather than ordinal alone, avoiding cross-module aliasing.
- Dispatch only imports from module index 1 (`DOSCALLS`) through the existing 16-bit DOSCALLS ABI adapter. Other module indices fail explicitly; never masquerade as DOSCALLS.
- Stubs encode import slot index in the hypercall, and runtime resolves module/ordinal via guest process import table.
- Test the observed stage as a *negative* boundary: LINK386 executes to `NLS.4`, which is NOT YET SUPPORTED. Exit remains nonzero.

## Observed
`soft386: input=... type=NE entry=0108:514E stack=0120:90A0 DS=0120 env=0188 imports=29`
`soft386: NE unsupported imported module 3 ordinal 4`
`soft386: termination=NE runtime failure rc=1 cycles=6876`

## Next actions
Identify the historical 16-bit NLS ordinal 4 function, signature, stack cleanup and return behavior; implement a module-aware bridge; continue the linker through KBDCALLS/MSG and file APIs. Also harden NE parser against invalid chained cycles/offsets, and verify on native Windows. This milestone does **not** claim linker functionality, object linking or runnable output.

Regression: Linux-hosted `make check-quick` at `-O0` passes, including all prior NE fixtures and 32-bit quick tests. No system DLL changes, no CPU core changes. Default -O2 and native Windows runtime not retested here.
