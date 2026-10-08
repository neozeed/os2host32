# Soft386 NE-H4F-RC: genuine RC trace, DosDelete ABI correction, and level-2 EA setting

Date: 2026-10-08
Exact source baseline: `os2host32-SOFT386-NE-H4E-RC-source.zip`
Original H4D and H4E archives remain frozen. H4F only changes the Soft386 NE DOSCALLS personality and its RC synthetic fixture.

## User's real Windows H4E trace: decisive evidence

RCPP now completes with rc=0 and no preprocessed source dumped on the outer console, supporting the H4E inherited-stdio fix. RC still leaves `simcity.RES` zero bytes and `simcity.$$$` unremoved, exiting rc=1.

The `rc-h4e.err` trace reveals:

- RC calls `DOS16.60` repeatedly, but NOT ONE call reaches H4E's `DOS16 Delete path=...` trace.
- RC calls `DOS16.83 SetFileInfo h=3 fd=3 level=2 cb=12 ... rc=124`.
- `DosClose` and `DosOpen` otherwise show ordinary file-handle operations; no evidence warrants reverting H4E's stdio fix.

These reveal two independent implementation bugs, not generic Resource Compiler misbehavior.

## H4F fixes

### DOSCALLS.60 DosDelete: actual 8-byte 16-bit Pascal frame

The exact SDK prototype is `APIRET APIENTRY DosDelete(PSZ pszFName, ULONG ulReserved);`.
The frame **after the 16:16 far return** contains:

- `SS:SP+4`: reserved ULONG, must be zero.
- `SS:SP+8`: far pointer to filename (offset word, selector word).

H4E wrongly used `SS:SP+4` as the filename pointer and cleaned only 4 argument bytes. H4F uses `SS:SP+8`, rejects a nonzero reserved DWORD (87), performs `_unlink`/`unlink`, and cleans **8 bytes**. Early invalid arguments now receive an explicit `--trace-hc` diagnostic, rather than an unexplained `DOS16.60` entry with no path.

### DOSCALLS.83 DosSetFileInfo: level 2 is 12-byte EAOP

OS/2 1.x `DosSetFileInfo` level 1 accepts a 22-byte `FILESTATUS`; that H4E implementation is retained unchanged.

**Level 2 SET is not 16-bit FILESTATUS2**: it accepts a 12-byte `EAOP` with a far pointer to a packed, variable-length `FEALIST`. The 12-byte request in the user's genuine RC trace is therefore entirely reasonable. H4E rejected it with ERROR_INVALID_LEVEL 124.

H4F validates the EAOP pointer, FEALIST length, far-pointer range, all individual FEA fields, name lengths and terminating NUL bytes; reports malformed-list errors (255), invalid EA names (254), and writes `EAOP.oError` on entry errors. An empty FEALIST does nothing and succeeds.

For nonempty lists H4F persists real EA name/value pairs:

- Windows/NTFS: dynamically calls `NtSetEaFile` on the actual CRT HFILE's underlying Win32 HANDLE, converting OS/2's packed 16-bit FEAs to native DWORD-aligned FILE_FULL_EA_INFORMATION records. Returns host EA errors rather than reporting unsupported operations as successful. The file must have the necessary EA write access and live on an EA-capable filesystem.
- Linux diagnostic host: writes `user.os2.<name>` xattrs via `fsetxattr`. (Critical-EA flag semantics are not persisted separately on POSIX; never claim that a POSIX diagnostic host is a complete OS/2 EA personality.)
- Other diagnostic hosts: report ERROR_EAS_NOT_SUPPORTED (282) for nonempty lists.

The `--trace-hc` diagnostic now shows `fea=selector:offset list=<bytes> ea_error=<offset> rc=<result>`, plus the first eight EA entry names and value lengths. The attribute field is displayed only for level 1; H4E incorrectly read garbage at +20 for a 12-byte EAOP.

### No changes to frozen paths

- H4E's standard handle replacement, child inheritance, 16-bit open/read/write/dup/close remain unchanged.
- H4D keyboard handling and C/386 compiler loading are unchanged.
- Soft386's LE and PM guest engine, API catalog, native DLLs, and common DOSCALLS are untouched.
- Version banner now says **NE-H4F-RC**. This is an NE personality milestone, not a claim that the 32-bit LE engine was upgraded.

## Local verification (Linux)

- H4F Linux unoptimized build: PASS (pre-existing Tiny386 warnings only).
- The expanded synthetic genuine-NE regression exercises correct 8-byte DosDelete, nonzero-reserved refusal, duplicate delete returning file-not-found, exact Pascal stack cleanup, valid nonempty EAOP/FEALIST xattr persistence, invalid/truncated EA data, level-1 FILESTATUS, stdout save/redirect/restore, and synchronous child inherited output: PASS.
- `make check-ne`: PASS. H4D keyboard; NE LINK386/LIB; ARGS; TINY/MEDIUM/COMPACT/LARGE/HUGE CRT fixtures.
- `make check-quick`: PASS on Linux (existing LE runtime fixtures and DOSCALLS/system bridge checks).
- Native 32-bit MinGW build and real Windows RC/CL386 tests: **NOT RUN** here; Windows acceptance is still required.

## Build and actual RC acceptance

From the extracted H4F source tree:

```bat
cd soft386
make WINCC=gcc win32
copy /y soft386_os2.exe C:\OS2\soft386_os2.exe
```

Use RosBE/MinGW i686, not the 64-bit MinGW compiler. Then:

```bat
cd C:\c386\proj\simcity
copy /y simcity.exe simcity.before-h4f.exe
set "OS2HOST32_LOADER=C:\OS2\soft386_os2.exe"
C:\OS2\soft386_os2.exe --trace-hc RC.EXE -i include -i include\os2h simcity.rc simcity.exe > rc-h4f.out 2> rc-h4f.err
echo %ERRORLEVEL%
findstr /i "Delete SetFileInfo DOS16 EA entry child_exited unsupported" rc-h4f.err
dir simcity.res simcity.$$$ simcity.exe
```

**Desired trace differences** from H4E: actual filenames and return codes appear after DOS16.60, and level-2 DOS16.83 prints an EAOP list and performs bounded host EA operations rather than immediately returning 124. A working resource compiler still requires `simcity.RES` to become nonempty and `simcity.exe` to grow/change; none of that is yet claimed.

Keep the known Microsoft compiler check as a mandatory independent regression:

```bat
cd C:\cl386-research\os2_2.0\x\SDK20\C386\BIN
C:\OS2\soft386_os2.exe CL386.EXE -c void.c
echo %ERRORLEVEL%
dir void.OBJ
```

If Windows RC still fails, capture `rc-h4f.err` and preferably the resource file plus `RCPP.ERR`; the added EA diagnostics will distinguish invalid-EA data, unsupported host EA operations, file-sharing failures, and later RC failures.
