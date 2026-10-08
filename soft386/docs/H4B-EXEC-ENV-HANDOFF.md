# Soft386 OS/2 NE-H4B — DosExecPgm guest environment transfer

Baseline: H4A (`os2host32-SOFT386-NE-H4A-exec-args.zip`). Only `soft386/src/soft386_os2.c` changes; all existing guest images and 16-bit ABI paths are preserved.

## Root cause (live H4A evidence + independent Linux replay)

Under H4A, CL386.exe correctly calls DOSCALLS.144 with `argv0="c1_386.exe"` and an EMPTY second command-tail string. This is intentional for Microsoft's C/386 toolchain. The explicit *pEnv* pointer is non-null; CL386 passes the compiler-stage control information using an environment block, not a conventional command tail. A local guest-memory trace of CL386 `-c void.c` found:

- `MSC_CMD_FLAGS` (130 bytes of value in the local replay)
- `NO87` (1 byte)
- `PATH` (13 bytes)
- `_C_FILE_INFO` (6 bytes)

H4A ignored *pEnv* and used `CreateProcessA(..., lpEnvironment=NULL, ...)` / POSIX `execv()`. Thus the newly started Soft386 process inherited the **host** environment, not the OS/2 argument's explicit **guest** environment. C1 exited with a fatal missing-compiler-input error even though CL386 passed the correct handoff information.

## H4B changes

- Copy the NUL-delimited OS/2 *pEnv* environment from validated guest far memory with 64-KiB offset bounds, terminator checks, a maximum entry count, and allocation failure detection.
- Distinguish null *pEnv* (inherit) from explicit *pEnv* (pass exact environment).
- Windows: sort environment strings case-insensitively, preserve the full name=value bytes, and pass a double-NUL-terminated ANSI environment block to `CreateProcessA`.
- POSIX diagnostic host: build an `envp` vector pointing into the copied guest block, and use `execve` for explicit environments; retain `execv` when *pEnv* is null.
- If `OS2HOST32_LOADER` is missing in an explicit environment, recursive Soft386 child launches can still resolve their own image using `GetModuleFileNameA` (Windows) or `/proc/self/exe` (Linux diagnostics). Existing explicit loader overrides remain honored.
- When `--trace-hc` is set, print environment names and VALUE LENGTHS only, never the values.
- Do not synthesize a command tail for CL386/C1; keep the H4A two-string argument parsing unchanged.

## Observed local replay

Using the original CL386, C1, C2 and C3 images supplied by the user, a Linux GCC -O0 diagnostic run of `CL386.EXE -c void.c`:

1. CL386 launches C1_386 with *pEnv* explicitly propagated. C1 exits **rc=0** (previously C1008 / rc=1).
2. CL386 launches C2_386. C2 exits **rc=0**.
3. CL386 launches C3_386. **C3 still fails the separate known NE loader-validation issue**; no successful full compilation is claimed.

On Linux test hosts, filenames must be lowercased manually to mimic Windows case-insensitive lookup. None of the proprietary compiler EXEs are included in the H4B source archive.

## Regression and limitations

- GCC source compile and `make check-ne` passed locally, including all five MS C memory-model printf examples and the NE arguments tests.
- Native Windows/MinGW runtime is NOT tested here.
- C3's NE loader rejection is not resolved by H4B.
- Host/guest environment conversion is currently an ANSI Win32 bridge; byte-for-byte testing of Windows ANSI environment values remains a future follow-up.
- Earlier RC parent-process PATH/getenv failure remains a separate investigation.

## Next Windows test

Run from the directory containing CL386.EXE, C1_386.EXE, C2_386.EXE, C3_386.EXE and void.c:

```bat
set "OS2HOST32_LOADER=C:\OS2\soft386_os2.exe"
\os2\soft386_os2.exe --trace-hc --run CL386.EXE -c void.c > cl386-h4b.log 2>&1
findstr /i "NE EXEC environment MSC_CMD_FLAGS _C_FILE_INFO child_exited malformed termination" cl386-h4b.log
```

Expected diagnostic feature: `NE EXEC environment=explicit entries=...` and names `MSC_CMD_FLAGS`, `_C_FILE_INFO`. C1 and C2 should now receive the correct guest environment; C3 may remain the next boundary.
