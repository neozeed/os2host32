# Soft386 OS/2 R0 handoff

Date: 2026-10-03

Repository baseline:

```
7cb53dd91b1bfa6fb61bc028866167c6b8401290
```

## Result

R0 proves that the Tiny386 Brain-in-a-Jar CPU can replace the WHP virtual CPU
for the core 32-bit OS/2 process-vessel architecture.

Live host-side validation completed in this build environment:

1. 32-bit LE parse/map/import-resolution/execution: PASS
2. 32-bit LX parse/map/import-resolution/execution: PASS
3. LE internal OFF32 fixup + execution: PASS
4. LX internal OFF32 fixup + execution: PASS
5. historical hi.exe eight-ordinal DOSCALLS surface fixture: PASS
6. virtual-thread create/wait/switch/return/wake/resume fixture: PASS
7. GCC build/run: PASS
8. Clang build/run: PASS

The thread fixture executed 43 Tiny386 instruction steps/cycles in this
bounded test and demonstrated a switch from TID 1 / FS=0018 to TID 2 /
FS=0020, then restoration of TID 1 after the worker returned through the
runtime gate.

## Architectural correspondence with WHP

WHP concept | Soft386 R0
---|---
WHP guest RAM | `Runtime.ram` byte array
WHP virtual processor | one Tiny386 `CPUI386`
WHP run-until-exit | one-instruction Tiny386 stepping until hostcall/exit
I/O-port VM exit | Tiny386 I/O callback for `OUT 0xF0,EAX`
WHP register query/set | `cpui386_get_state` / `cpui386_set_state`
virtual threads | saved `CPUI386_State` contexts
per-thread TIB | guest TIB + per-thread FS GDT descriptor
synthetic imports | identical `mov eax,id; out 0xF0,eax; ret` shape
shared DOSCALLS | `common/doscalls/os2_doscalls_core.c`

This is intentionally software scheduling, not 386 hardware TSS task switching.
The OS/2 application does not require or observe a TSS for this R0 model.

## Tiny386 delta

The Brain-in-a-Jar checkpoint already provided protected-mode reset and a
read-only CPU state snapshot.  R0 adds `cpui386_set_state` and exposes segment
indices.  The state snapshot also records `next_ip` so a thread blocked after
an `OUT` resumes at the veneer `RET` rather than re-executing the host call.

State restore reloads ES/SS/DS/FS/GS/CS through Tiny386's existing descriptor
loader.  This is important: each virtual thread's FS selector must rebuild the
hidden segment cache with that thread's TIB base.

## Guest memory map

```
00007000   GDT
000F0000   synthetic DOSCALLS veneers
000F7FF0   runtime thread-return veneer
00E00000   argv/environment/startup area
00E20000   PIB/TIB area
01000000   guest heap / thread stacks
03000000   future module/DLL allocation region
```

The test LE/LX images use historical-style low preferred object addresses
(00010000 code, 00020000 data/stack).

## Known limitations

* 32-bit flat protected-mode application code only.
* Main executable only; no guest DLL loading in R0.
* DOSCALLS-only ordinal imports.
* Named imports rejected.
* No VIO/KBD migration helpers yet.
* No 16-bit guest code/fixups yet.
* No PM.
* No asynchronous pre-emption/timer interrupt.
* Host file surface is still intentionally tiny.
* Windows branch is implemented but this build environment has no MinGW-w64
  cross compiler, so the produced R0 evidence is Linux GCC/Clang.  The source
  uses Win32 APIs directly under `_WIN32` and is intended for a 32-bit MinGW
  build with `make win32`.

## Historical hi.exe next test

The prior WHP/OS2SS research established an `hi.exe` with SHA256:

```
3e0383860d8e76262b3c954bb95ae8e8e3c7887deddfbfd7705c4b8c580441c7
```

Its known DOSCALLS ordinal surface is 224, 234, 256, 282, 299, 304, 305, 348.
R0 implements and fixture-tests that entire set.  The binary itself was not
present in the supplied 7cb53dd checkout, so R0 does not claim a historical
`hi.exe` runtime pass yet.
