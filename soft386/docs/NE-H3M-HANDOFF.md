# NE-H3M: LINK386 DosOpen frame diagnostics + bounded compatibility

Base: NE-H3L source, plus H3M GetEnv diagnostic patch.

## What the Windows trace establishes

`DOS16.91` returned env selector 0188 and command offset 0AE6. LINK386 successfully opened HUGE.OBJ, LLIBCE.LIB and OS2.LIB. Its output-open call had `mode=0043`, `action=0012`, and the 30-byte far-pointer layout, unlike the common 26-byte call. `0x0043 & 7 == 3` is not a documented SDK access mode; do not claim it is.

## Changes

- Add `Dos16GetEnv` implementation in the guest NE environment, no changes to host DOSCALLS DLL.
- Inspect both 30- and 26-byte DosOpen candidate frames before selecting; trace output pointers, path pointer, reserved word, validity and decoded arguments with --trace-hc. Prefer 30 bytes on equal score to preserve the LIB.EXE creation path, which can contain stale but valid-looking 26-byte pointers.
- Permit access value `3` ONLY for exact `mode=0043` and `action=0012` after a validated filename and output pointers. Explicitly report the workaround in the trace and use guest-local HFILE mode read/write. All other access=3 patterns continue returning error 87.
- Do not assume this compatibility mapping proves original OS/2 accepted access=3. Disassemble the LINK386 call site and validate on Windows before expanding scope.

## Verification

Linux diagnostic binary compiled with -O0. NE args, LINK386 no-input startup, librarian create and add-OBJ, VOID, tiny/medium/large/compact/huge tests passed individually in the suite. A broader make check-quick was terminated externally while running the NE fixture sequence, not by a failing assertion. No LLIBCE.LIB or OS2.LIB present here, so true output `huge.exe` creation by LINK386 cannot be validated from this workspace. Host Windows runtime is pending.

## Windows test

```
soft386_os2.exe --trace-hc --run \\cl386-research\\os2_2.0\\x\\SDK20\\TOOLKT20\\OS2BIN\\link386.exe huge.obj > link-h3m.log 2>&1
```

Look for `OPEN candidate frame=30`, `DOS16 open compatibility: undocumented mode=0043`, and `DOS16 open file=huge.exe ...`. If it creates a file, validate format and the linker exit status; do not assume a created file is a valid executable.
