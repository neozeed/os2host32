# Soft386 NE-H4G-RC: RC stdout reopening, Pascal ABI audit, FEALIST bounds

Date: 2026-10-09
Exact baseline: uploaded `os2host32-SOFT386-NE-H4F-RC-source.zip` (unchanged/frozen).
Scope: NE personality only; no changes to 32-bit LE runtime or native DLL exports.

## Decisive H4F Windows evidence

The real `RC.EXE` (`Resource Compiler Version 2.01.02`) executed `RCPP.EXE` and the child returned rc=0, but `simcity.RES` remained zero bytes. The trace exposes:

```
DOS16 DupHandle old=1 fd=1 wanted=4 newfd=4 rc=0
DOS16 Close h=1 fd=1 rc=0
DOS16 open file=C:\Users\...\Temp\rba00001 h=5 flags=0012 mode=0042
NE EXEC inherit std1 guest_fd=-1 native=00000000
NE EXEC child_exited exit_code=0
```

RC uses the historical pattern **save stdout -> close stdout -> DosOpen temporary output file -> execute RCPP -> restore stdout**. Before H4G, the `DosOpen` allocator in `dispatch_dos16` started at guest slot **3**, never reclaiming closed slots 0/1/2. Thus the newly opened temporary file took HFILE 5 and child stdout was NULL. The previous H4E DosDupHandle implementation was not enough to repair this particular strategy.

Other H4F evidence:
- `DosDelete("simcity.$$$") rc=0`: corrected 8-byte Pascal frame is now proven by the real trace; don't keep changing it.
- `DOS16.83 SetFileInfo ... level=2 ... list=32 ea_error=30 rc=255`: the single `.TYPE` entry has length `4+4+5+1+16=30`, leaving two padding bytes in a list whose `cbList` is 32.
- H4F shows significant copying/rewriting of executable pages later, but with no compiled resources. It does not prove resource compilation success.

## H4G changes

1. **Lowest-free guest HFILE, including 0/1/2**: both `DosOpen` and `DosDupHandle(..., 0xffff)` now look from guest slot 0, not 3. `n->hfile` tracks guest handles and native CRT descriptors separately. `DosClose` does not close the underlying host stdout/stderr when it does not own those descriptors. The child launcher already snapshots the guest 0/1/2 mappings; the corrected guest allocation now gives it the file RC intended.
2. **File object versus handle number**: `DosQHandType` now checks the actual underlying handle with Win32 `GetFileType` or POSIX `fstat`. A reopened HFILE=1 naming a disk file is `HANDTYPE_FILE` rather than erroneously `HANDTYPE_DEVICE`.
3. **Conservative 16-bit FEALIST padding**: accept a 1–3 byte **zero-filled trailing** pad only when it follows at least one complete packed FEA and the declared total length is DWORD-aligned. Never skip gaps between entries; nonzero trailer remains ERROR_EA_LIST_INCONSISTENT (255). A valid 30-byte `.TYPE` FEA inside 32 allocated bytes now reaches the real host EA adapter; on Windows that can still fail independently if the filesystem, handle access, or NT kernel EA call refuses it.
4. **Pascal cleanup safety**: audited 34 implemented DOSCALLS 16-bit signature widths against `sdk/os2h/bsedos16.h`. They are listed and automatically checked in `soft386/tests/check-ne-pascal-abi.py`. The cleanup dispatcher is explicitly module-scoped: DOSCALLS, NLS.4, KBDCALLS.13, MSG.2. Unknown imports now fail instead of silently assuming zero Pascal argument cleanup or borrowing a DOSCALLS width for another module. No arbitrary blanket cleanup patches.
5. Version banner says **`Soft386 OS/2 NE-H4G-RC`**.

## Live proof status

Linux diagnostics on actual H4G sources (unoptimized `-std=gnu99 -O0` build):

- `tests/check-ne-pascal-abi.py`: PASS, 34 DOSCALLS and 3 module-specific signatures.
- H4G synthetic NE fixture: PASS; closes HFILE=1, reopens a file, verifies **guest HFILE=1**, `DosQHandType=FILE`, parent output, inherited child output, stdout restoration, 8-byte DosDelete and level-2 EAs including padded `.TYPE`, negative nonzero padding, and exact Pascal cleanup.
- Full `make check-ne`: PASS, including H4D KBDCALLS, NE args, LINK386, LIB, and C runtime tiny/medium/compact/large/huge fixtures.
- Full `make check-quick`: PASS, including LE, sync/thread, DOSCALLS and system bridge tests.
- Windows build and real RC/CL386: **NOT RUN here**. User is the Windows proxy; do not call H4G Windows-live-proven yet.

## Windows build and acceptance

Extract the H4G source ZIP and rebuild in your normal 32-bit RosBE/MinGW shell:

```
cd soft386
make WINCC=gcc win32
copy /y soft386_os2.exe C:\OS2\soft386_os2.exe
```

In the SimCity working directory (keep executable backup):

```
copy /y simcity.exe simcity.before-h4g.exe
set "OS2HOST32_LOADER=C:\OS2\soft386_os2.exe"
C:\OS2\soft386_os2.exe --trace-hc RC.EXE -i include -i include\os2h simcity.rc simcity.exe > rc-h4g.out 2> rc-h4g.err
echo %ERRORLEVEL%
findstr /i "open.file DupHandle Close inherit SetFileInfo EA.error Delete child_exited unsupported" rc-h4g.err
dir simcity.RES simcity.exe simcity.$$$
```

**First acceptance criterion**: the temporary `rba00001` opened immediately after `DosClose(1)` is now `h=1`, and `NE EXEC inherit std1 guest_fd` is nonnegative. Next, look for `simcity.RES` becoming nonempty and RC successfully writing resources into its target executable. RC might reveal a separate next-stage issue, and `DosSetFileInfo` on Windows might still report a legitimate host error.

**Compiler regression** (retain the live-proven Microsoft C/386 chain):

```
cd C:\cl386-research\os2_2.0\x\SDK20\C386\BIN
C:\OS2\soft386_os2.exe CL386.EXE -c void.c
echo %ERRORLEVEL%
dir void.OBJ
```

The earlier C1_386/C2_386/C3_386 successful execution is the regression oracle. No historical proprietary toolchain binaries were bundled.

## Known boundaries

A static width audit does **not** prove every supported API's complete semantics or validate every argument offset dynamically. Existing NE synthetic fixtures check real far returns and SP for the most important paths. Outstanding compatibility work includes real RC acceptance, full file sharing/delete-pending semantics, advanced EA handling, and Windows-native MinGW runtime validation.
