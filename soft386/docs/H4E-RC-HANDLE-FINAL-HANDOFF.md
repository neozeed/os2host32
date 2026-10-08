# Soft386 OS/2 NE-H4E-RC — 16-bit file handles and RC/RCPP bridge

Date: 2026-10-08
Source baseline: user-supplied `os2host32-SOFT386-NE-H4D-kbdflush-final(1).zip`
The H4D ZIP bytes remain unmodified; all changes below are additive H4E.

## Problem and source-confirmed diagnosis

The original Microsoft RC.EXE creates a zero-byte `simcity.RES`, exposes RCPP's
preprocessed source on outer stdout, then reports it cannot delete `simcity.$$$`.
The H4D runtime had **two separate stdout redirection defects**:

1. `Dos16DupHandle` explicitly rejected targets 0/1/2, although the OS/2 ABI
   permits replacing standard guest handles.
2. `ne_exec_sync_child` passed `GetStdHandle` values from the Soft386 host,
   ignoring guest HFILE 0/1/2 replacements made by RC.

Additionally, `Dos16Write` bypassed the guest HFILE table for standard output
and went directly to common DOSCALLS, losing output redirected by the 16-bit
parent. The H4E path consistently uses guest HFILE -> native descriptor.

These explain why preprocessing output could go to the outer stdout capture.
The real Windows RC failure and the separate temp-file deletion issue have
**not yet been re-tested** on H4E.

## H4E implementation

- Guest-local HFILE table includes independent slots 0/1/2, with ownership
  tracking so guest DosClose(1) never closes Soft386's host stderr/stdout.
- `Dos16DupHandle` accepts requested target 0, 1 or 2; target 0xFFFF allocates
  a new guest slot; source and destination share the native underlying open-file
  description and file offset. It handles same-source/target safely.
- `Dos16Read` already used guest HFILE lookup. `Dos16Write` now does also,
  even on handles 0/1/2. A redirected guest stdout receives actual guest bytes.
- Windows synchronous child launch duplicates selected native handles to
  inheritable Win32 HANDLEs and passes them via `STARTF_USESTDHANDLES`;
  temporary HANDLEs are closed in the parent after `CreateProcessA`.
- POSIX diagnostic child launch snapshots selected descriptors before dup2,
  so stream swaps and closed descriptors are not corrupted.
- DOSCALLS.83 `DosSetFileInfo(HFILE,USHORT,PVOID,USHORT)` added for 16-bit NE:
  10-byte Pascal callee cleanup, valid level-1 22-byte 16-bit `FILESTATUS`,
  packed DOS date/time conversion and writable attributes. File size/allocation
  bytes remain query-only. On Win32 this uses `_get_osfhandle`, `SetFileTime`,
  and pathname-resolved `SetFileAttributesA`; on POSIX, `futimens/fchmod` cover
  timestamps and read-only mode (DOS hidden/system/archive have no POSIX
  equivalents). Unsupported levels and short buffers return explicit errors.
- Traces under `--trace-hc`: open, duplicate (old/wanted/native), write,
  close, delete (path/errno/APIRET), SetFileInfo (level/size/attr/APIRET), and
  child's inherited 0/1/2 descriptors.
- Updated NE banner `NE-H4E-RC` (32-bit LE banner unchanged).

## Validation completed

- Linux unoptimized Soft386 build: PASS.
- H4E synthetic 16-bit NE regression: PASS. Constructs genuine NE far imports
  for DosOpen, DosDupHandle, DosWrite, DosExecPgm, DosClose, DosSetFileInfo,
  DosDelete, and DosExit. Checks stdout restore, shared file position across
  child process, content isolation from host stdout, 16-bit Pascal cleanup,
  metadata/timestamps, invalid level/length/handle, and deletion.
- `make check-ne`: PASS (existing H4D keyboard plus Microsoft CRT TINY,
  MEDIUM, LARGE, COMPACT, HUGE, LINK386/LIB/argv regressions).
- `make check-quick`: PASS (LE + bridge regression suite).
- Full `make check` started but stopped by execution-time limit while
  building PM runtime checks; **not claimed as passing**.
- Windows/RosBE build, native RC/RCPP run, and Windows CL386/LINK386
  regression remain user acceptance items. No binary RC or CL386 supplied.

## Windows build and acceptance

Build from the new archive's root using your known RosBE/i686 compiler:

```bat
cd soft386
make WINCC=gcc win32
copy /y soft386_os2.exe C:\OS2\soft386_os2.exe
```

If your shell uses cross compiler instead of the native RosBE compiler,
use `make WINCC=i686-w64-mingw32-gcc win32` as appropriate.

First preserve the compiler regression with source and known toolchain paths:

```bat
cd C:\cl386-research\os2_2.0\x\SDK20\C386\BIN
set OS2HOST32_LOADER=C:\OS2\soft386_os2.exe
C:\OS2\soft386_os2.exe --trace-hc CL386.EXE -c void.c > cl386-h4e.out 2> cl386-h4e.err
echo %ERRORLEVEL%
dir void.OBJ
```

Now use the actual RC sample directory, back up SimCity before modification:

```bat
cd C:\c386\proj\simcity
copy /y simcity.exe simcity.before-h4e.exe
set OS2HOST32_LOADER=C:\OS2\soft386_os2.exe
C:\OS2\soft386_os2.exe --trace-hc RC.EXE -i include -i include\os2h simcity.rc simcity.exe > rc-h4e.out 2> rc-h4e.err
echo %ERRORLEVEL%
findstr /i "DupHandle SetFileInfo Delete Close EXEC inherit open unsupported" rc-h4e.err
for %%F in (simcity.res simcity.$$$ simcity.exe) do @dir %%F
```

An actual RC result will determine what remains broken. In particular,
`simcity.$$$` deletion might still fail if an HFILE is left open (Windows
sharing semantics) or the file retains read-only attributes. Inspect close
and delete traces, including the OS/2 APIRET, before introducing further
changes. `simcity.RES` must become nonempty and the output LE must actually
change before claiming RC success.

## Scope boundaries

This is a 16-bit NE-only change. It does not change 32-bit DOSCALLS exported
ordinals, the 32-bit loader/CPU core, H4D KBDCALLS.13, the common DOSCALLS
backend, or any original Microsoft executables. The original H4D was proven
live with C1/C2/C3/CL386 on Windows; **H4E Windows live status is pending**.
