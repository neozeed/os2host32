# Soft386 OS/2 NE-H4D — KBDCALLS.13 bridge and C/386 toolchain checkpoint

Date: 2026-10-08
Baseline: `os2host32-SOFT386-NE-H4C-c3-compiler.zip` (H4C Windows-live-proven by user)
Scope: small, module-scoped 16-bit NE keyboard bridge; H4C compiler ABI and loader are frozen.

## Windows proof received for the frozen H4C baseline

The user built the genuine Microsoft C/386 V1.00.075 `CL386.EXE -c void.c` under Win32 Soft386. Its untouched C1_386, C2_386 and C3_386 children all exited 0; parent CL386 exited 0 and generated 234-byte `void.OBJ`. The original Microsoft `LINK386.EXE` V1.01.015 then linked it to a 32-bit OS/2 LE `void.exe`. Soft386 executed the linked LE with process rc=0.

A second source program whose main() returns 42 was compiled and linked. User's Win32 Soft386 run of `42.exe` reported `DosExit(EXIT_PROCESS) rc=42` and the Win32 command shell `echo %ERRORLEVEL%` returned **42**. This is a verified live end-to-end source -> three genuine 16-bit NE compiler stages -> Microsoft OMF -> genuine NE LINK386 -> 32-bit LE guest execution milestone. User described performance as snappy.

## H4D change

`KBDCALLS.13` = `KbdFlushBuffer(HKBD)` uses one 16-bit word (Pascal far-call frame `SS:SP+4`), a 16-bit APIRET in AX, and **two bytes of Pascal callee cleanup**. NE module-index dispatch is by imported module *name*; KBDCALLS.13 must not be confused with a DOSCALLS ordinal 13.

H4D adds `dispatch_kbd16` and a module-scoped cleanup rule, preserving H4C's NE loader, other DOSCALLS/NLS/MSG handling, x87 OSFIXUP behavior, child argument parsing and explicit child environments.

On Win32 the new host adapter calls the existing shared R2 `os2_kbd_KbdFlushBuffer(os2_kbd_win32_session(), hkbd)` implementation. The common keyboard core and the actual Win32 backend sources are linked into Soft386, so no second, guessed keyboard state machine and no KBDCALLS.DLL installation are required for this NE call. The existing exported KBDCALLS.DLL is **unchanged** and already exports ordinal 13; 32-bit system bridge handling is unchanged. The common R2 keyboard implementation treats HKBD as the default keyboard (ignores nonzero HKBD, pre-existing approximation). On a non-Win32 diagnostic host the terminal input queue is flushed if it is a TTY; redirected standard input is not consumed.

`--trace-hc` now emits `soft386: KBD16.13 KbdFlushBuffer hkbd=0000 rc=0` on success. The `KBD16` prefix prevents accidentally diagnosing this as a `DOS16.13` API.

## Files changed from H4C

- `soft386/src/soft386_os2.c` — dispatch for KBDCALLS and module-local NE stack cleanup.
- `soft386/src/soft386_ne_kbd.c` (new) — portable scalar adapter to shared R2 keyboard semantics / POSIX diagnostic terminal flush.
- `soft386/src/soft386_ne_kbd.h` (new) — host adapter interface.
- `soft386/Makefile` — link shared keyboard core and Windows backend; add regression target.
- `soft386/tests/check-ne-kbdflush.py` (new) — construct original-format synthetic OS/2 1.x NE image with real far relocations to KBDCALLS.13 and DOSCALLS.5; call with HKBD 0 and 1, validate APIRET AX=0, exact 2-byte Pascal stack cleanup, correct module mapping, guest exit 0.

## Verification performed on this H4D snapshot

1. GCC syntax check of the touched C sources completed (pre-existing warnings only).
2. Local Linux **unoptimized** Soft386 build completed; the optimized core build did not complete within the execution time limit and is not claimed as passing.
3. New synthetic `KBDCALLS.13` 16-bit NE regression passed, including two real guest far calls and exact Pascal stack cleanup.
4. Complete `make check-ne` suite passed, covering LINK386 import-table startup, NE argument handling, LIB cases, tiny/medium/compact/large/huge Microsoft C runtime cases, and the new KBDCALLS test.
5. The unmodified Microsoft CL386/C1/C2/C3 compiler chain was rerun with H4D Linux diagnostic executable, all three stages and parent exited 0, producing a real Microsoft 8086 relocatable OMF `fortytwo.OBJ` of 242 bytes. On Linux the historical compiler output extension is uppercase `.OBJ` and exact filename case matters. Historical Microsoft compiler executables and generated OMF files are not bundled.
6. ZIP integrity and patch-to-H4C application checks are part of packaging verification.

**H4D's new 16-bit keyboard bridge has not yet been live-tested on Win32.** Do not label it Windows-live-proven until the user runs it. H4C Windows proof remains independent and frozen.

## Windows rebuild and test

Rebuild Soft386 from the included H4D source (or apply the H4D patch to exact H4C), replacing `C:\OS2\soft386_os2.exe`:

```bat
set "OS2HOST32_LOADER=C:\OS2\soft386_os2.exe"
\OS2\soft386_os2.exe --trace-hc --run CL386.EXE -c void.c > cl386-h4d.log 2>&1
echo %ERRORLEVEL%
findstr /i "KBD16 child_exited termination unsupported" cl386-h4d.log
```

Test `LINK386.EXE` as previously proven, and optionally a tiny call to `KbdFlushBuffer(0)` linked from the 16-bit OS/2 SDK. The new synthetic fixture test runs automatically via `make check-ne`. LINK386's import table includes KBDCALLS.13 but its interactive link path may not actually invoke it; do not claim the binary test covers the call unless `KBD16.13` is seen.

## Scope boundaries and next chapter

This checkpoint completes the Microsoft C/386 source -> NE compiler pipeline -> NE linker -> 32-bit LE end-to-end toolchain **demonstration** and adds the requested 16-bit keyboard ordinal to the NE personality. It does **not** mean every 16-bit OS/2 program or keyboard API is implemented. In particular, the older Resource Compiler RC.EXE/RCPP.EXE integration had an independent `getenv("PATH")` / environment lookup issue; direct standalone RCPP preprocessing exited 0 at H3V. RC's full child launch remains an outstanding separate task, not a regression or proof claim for H4D.
