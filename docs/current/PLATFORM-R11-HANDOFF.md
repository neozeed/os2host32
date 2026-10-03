# R11: native platform expansion

Date: 2026-10-03. Integration baseline: the complete R10 source package.

## Milestone

R11 adds ten DOSCALLS exports, standard modal file dialogs, rectangle tracking,
and external IBM message-file lookup. Four of TELNETPM's seven remaining R10
imports now have implementations: DOSCALLS.110, PMWIN.890, PMCTLS.4 and MSG.6.
DOSCALLS.111/.300/.356 remain unavailable and retain the loader's fail-stop
behavior. These are useful subsets, not claims of complete OS/2 conformance.

The loader, exact-image TELNETPM profile, normal `--run` path, queue-lifetime
fix, font/geometry work, SimCity merge and Winsock sources are retained.
The original TELNETPM executable and IBM message files are not redistributed.

## DOSCALLS additions and layering

| Ordinal | API | Supported behavior / limits |
| --- | --- | --- |
| 110 | DosForceDelete | Permanent file deletion; preserves read-only/access-denied failures |
| 209 | DosSetMaxFH | Increases the guest handle limit; existing table capacity is 256; decreases fail |
| 221 | DosSetFHState | Changes inheritance; rejects changes to unsupported I/O policy bits |
| 254 | DosResetBuffer | Flushes a file or all handles; read-only handles need no flush; flush-all skips pipes to avoid waiting for readers |
| 258 | DosCopy | Copy with overwrite/no-overwrite; append and fail-on-EA options return error 50 |
| 276 | DosQueryFHState | Reports recorded access, sharing and open-mode flags; unknown externally adopted modes return 50 |
| 278 | DosQueryFSInfo | Allocation information (18 bytes) and volume information (17 bytes); levels 1/2 only |
| 362 | DosTmrQueryFreq | Host performance-counter frequency, in the OS/2 32-bit output |
| 363 | DosTmrQueryTime | Matching counter, serialized as low/high 32-bit words |
| 572 | DosQueryResourceSize | Size of an existing loader-published resource, without taking ownership |

`dlls/doscalls/doscalls.c` contains thin ordinal veneers. Validation, OS/2
structures, limits, handle-mode state and policy live in
`common/doscalls/os2_doscalls_platform.c` and the existing session layer.
`Os2DosPlatformOps`, declared in `common/include/os2_doscalls_backend.h`, carries
neutral integers, strings and structures across the boundary.
`common/win32/os2_dos_platform_win32.c` implements deletion/copying, flush,
inheritance, disk queries, counters and the existing resource bridge.
There are no Windows headers or Windows API calls in the new policy source.
The optional platform operations are initialized by the native session;
new catalog entries do not claim WHP support. WHP/ReactOS work stays frozen.

`DosOpen` now records its mode and honors no-inherit, write-through and
sequential/random locality flags in the Win32 backend. Pipe creation and handle
duplication update shared mode metadata; duplication onto itself preserves it.
This does not add interprocess guest handle-table reconstruction or complete
support for every OS/2 open flag. `DosSetFHState` cannot retroactively change
Win32 caching flags. DOSCALLS retains the project's existing ANSI path/codepage
behavior. Volume labels are capped at 11 bytes; UNC current directories are not
supported by the drive-number FS query.

## PM and message subsets

`PMWIN.890 WinTrackRect` uses the 76-byte GA TRACKINFO layout, bottom-up OS/2
coordinates, mouse capture, XOR borders, mouse or arrow-key movement, grid,
size/boundary limits, Enter/left-release acceptance and Esc/right-click
cancellation. The portable coordinate policy is `common/pm/os2_track.c`;
Win32 drawing and event handling are in `dlls/pmwin/pm_track.h`. Cancellation
leaves the input rectangle unchanged. Validation callbacks and partial-boundary
resizing are explicitly unsupported. This is a basic tracking loop; full PM
tracking-message/custom-procedure behavior and exact TF_STANDARD interaction
remain outside this pass. Test actual TELNETPM selection/dragging on Windows.

`PMCTLS.4 WinFileDlg` is a new DLL using Windows Open/Save common dialogs and
the 328-byte GA FILEDLG. It handles modal single selection, title/OK caption,
centering, wildcard filtering, initial directory and OK/Cancel results. The
dialog returns a nonzero dismissed-window value on ordinary cancellation;
`lReturn` distinguishes OK (1) from Cancel (2). It preserves the process CWD.
Custom callbacks/templates, modeless operation, EA/type filters and multiple
selection are rejected. Native appearance and filesystem behavior apply;
OS/2 help/apply buttons and exact dialog positioning are not implemented.
Both file-dialog calls inspected in this TELNETPM use the supported 0x101
flags with a zero callback and a 328-byte structure.

`MSG.6 DosTrueGetMessage` and companion `MSG.4 DosInsertMessage` are provided
by a new MSG.dll. The host-independent decoder in `common/msg/os2_msg.c`
supports MKMSGF v0/v2, 16/32-bit indexes, literal byte strings, %1..%9 inserts,
E/W component-number prefixes and bounded output. Returned lengths count
copied bytes; there is no implicit terminating NUL. Missing message IDs and
unused `?` slots return 317; truncation returns 316; malformed files return 319.
Lookup tries the supplied filename, then DPATH for bare filenames. Files are
limited to 32 MiB. Output uses message bytes as stored; alternate-language
selection and codepage conversion are not implemented.

The true-message entry includes the extra first bound-message-segment argument.
TELNETPM's wrapper supplies the verified empty MSGSEG32 table, which falls
back to an external file. Nonempty bound-message tables return error 50 rather
than pretending to have searched embedded text. This is not a text-file shim.

## Remainder of DOSCALLS

`DOSCALLS-R11-AUDIT.tsv` inventories 157 entries from the bundled documented
32-bit SDK ordinal set plus later native exports and the discussed additions:
80 existing exports, 10 new exports, and 67 still absent. Eight existing
entries are explicitly flagged as partial; an export alone is not a
conformance result. Reproduce the inventory with:

```sh
python3 tools/audit_doscalls.py
```

| Priority | Candidates | Required work |
| --- | --- | --- |
| Small next batch | DosErrClass, DosEditName, DosQueryProcType | Error/name rules and module metadata; mostly portable policy |
| Moderate | File locking | LockFileEx/UnlockFileEx plus OS/2 ranges, timeout, cancellation and rollback |
| Moderate | Named pipes | Native pipe primitives plus OS/2 naming, message modes, handle state and lifecycle |
| Moderate | Async/start/stop timers | Timer objects that post guest semaphores; synchronize stop and release |
| Moderate | Mutex queries / muxwait | Track ownership/counts and implement wait-any/all with close-race handling |
| Moderate | Filesystem/device queries and verify policy | Wire layouts and real behavior, including effect on writes |
| Infrastructure | Thread/process control and critical sections | Retained thread objects, guest-thread lifecycle and scheduler semantics |
| Infrastructure | Shared memory | Names, ownership, address agreement and cross-process mapping lifetime |
| Infrastructure | Raise/unwind exception and signals | OS/2 context records, guest callbacks and reliable unwind/continuation |

In particular, `DosEnterCritSec` is an existing success stub. Adding a normal
Windows critical section for `DosExitCritSec` would not supply OS/2's suspension
of other process threads. `DosExitList` is also validation-only, and priority
control is limited to a current-thread approximation. Those existing gaps are
documented rather than expanded with more success stubs. `DosKillThread` is
not mapped to TerminateThread, and shared memory is not substituted with
private VirtualAlloc. The TSV excludes legacy 16-bit and undocumented kernel
surfaces; it is not a count of every historical DOSCALLS export.

## Verification and Windows acceptance

Host tests exercise the production shared policy, error propagation, output
guards, file-mode state, message layouts and constrained rectangle geometry.
There are 12,916 checks, mostly a grid of rectangle boundary/size cases.
The same tests pass AddressSanitizer/UBSan with leak detection disabled because
this environment prevents LeakSanitizer from inspecting process threads.
The message corpus check covers 32 intact intake files, 6,940 message bodies
and 3,609 unused slots, and rejects damaged data in two other intake files.
It checks binary format/output, not execution against a native OS/2 oracle.

Shared DOSCALLS core/veneer/static checks, NLS, API catalog and existing PM
profile/switch/text/font/queue regressions are included in the verification
logs. The monolithic `make verify` stops at an existing dependency on the
removed `whp/src/whp_os2_v2_hi.c`; its gate was not weakened or silently marked
as passed. The new native smoke executables are cross-built, not run here.
No Windows/Wine runtime is available in this environment.

From the packaged `runtime` directory, keep the working ETC setting and run:

```bat
run-platform-smoke.cmd
run-pm-platform-smoke.cmd
run-api-smoke.cmd
run-socket-smoke.cmd
run-telnetpm.cmd C:\OS2\tmp\telnetpm-phase1\telnetpm.exe
```

The first smoke uses temporary files, real ordinal calls, file states/copy/
flush/delete, disk/counter/resource queries, MSG wrapper/DPATH and insertion.
The second opens its own test window and cancels its own file dialog. Its
15-second watchdog terminates only the smoke process if that test hangs;
no production shutdown watchdog was added. Check real TELNETPM Open/Save,
Cancel, selection/dragging and normal exit after a connection. Re-run SimCity
from its usual asset directory using `run-pm-app.cmd` and the existing rendering
smokes. File-dialog acceptance/save paths still need live acceptance.

For a diagnostic log use `run-telnetpm-probe.cmd` and send
`phase2-r11-probe.txt`. A call to the three deferred DOSCALLS imports still
stops explicitly. New DLLs must be installed alongside the packaged loader.
Windows runtime use needs no Python. The display-server runner stays on port 23.

## Build and integrate

The archive contains 1,432 source files, including SO32DLL/TCP32DLL and their
shared Winsock backend; runtime binaries; checksums; this handoff; and the
verified `integration/r10-to-r11.patch` with `FILES-TO-COMMIT.txt`.
The patch changes/adds 29 files; 1,403 baseline source files are byte-identical.
The patch applies to the complete R10 package, not the original upstream HEAD.
No commit or push has been made. Use the complete tree or review/apply the patch.

```sh
make -j2 all platform-smoke.exe pm-platform-smoke.exe socket-smoke.exe telnetpm-api-smoke.exe pm-merge-smoke.exe font-render-smoke.exe window-size-smoke.exe queue-lifecycle-smoke.exe telnet-display-server.exe MINGW=i686-w64-mingw32-gcc
make platform-host-check doscalls-check doscalls-veneer-check doscalls-static-check catalog-check
```

The clean build produces 28 PE32/i386 files. Only DOSCALLS.dll and PMWIN.dll
replace existing R10 packaged runtime companions. PMCTLS.dll, MSG.dll and two
native smoke binaries are added. The R10 loader, PMGPI, PMSHAPI, socket adapters
and the other existing runtime companions (12 files total) are preserved byte-for-byte.
The runtime also includes the ten remaining clean-built native companions
(other DLLs, shell and transformer), so the package contains every active
native build output. The full source builds these with GNU make.

Reference material: bundled IBM SDK `sdk/os2h/bseord.h`/`bsedos.h`; GA toolkit
TRACKINFO/FILEDLG declarations in the supplied intake; the exact guest call
sites; the [MKMSGF author's format/compiler source](https://github.com/MikeyG/mkmsgf/tree/master/src);
Microsoft [FlushFileBuffers](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers)
and [KB Q59893, DosSetMaxFH](https://www.infania.net/misc/kbarchive/kb/059/Q59893/index.html).
The message decoder is independently written from the format; no compiler
implementation or IBM message text is copied into the package.
