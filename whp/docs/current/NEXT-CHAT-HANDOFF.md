# WHP Milestone 3 handoff

## Proven before this source migration

The supplied WHP milestone already proved untouched OS/2 LE execution, DOSCALLS
hypercall veneers, guest threads, event/mutex synchronization, guest DLL loading,
file I/O, callbacks, PM/GPI work, and the single-WHP-VP/multiple-saved-context
scheduler architecture.  The previous handoff remains under
`whp/docs/milestones/m2-package/` and revision notes remain under
`whp/docs/milestones/revisions/`.

## New architecture to preserve

Do not put ordinary DOSCALLS semantics back into the main WHP switch when an API
is marked `OS2_API_ROUTE_SHARED`.

For a new shared API:

1. Define the pointer-safe backend operation in `common/include/`.
2. Implement validation and OS/2 semantics in `common/<subsystem>/`.
3. Make the native DLL export a thin veneer over that implementation.
4. Make WHP call the same implementation through its guest adapter.
5. Change the ordinal catalogue route to `SHARED`.
6. Extend `tests/common/` and `tools/check_shared_personality.py`.
7. Run `make verify` and both Windows regression paths.

Do not force thread/process/scheduler operations through the common layer.  Those
remain loader intrinsics.

## Immediate Windows validation

Build native artifacts with 32-bit MinGW and run at least:

```text
hi.exe
thread1.exe
memory torture
CMD32 file-I/O smoke tests
phoon/Sarien regressions as applicable
```

Build WHP from an x64 Visual Studio Developer Command Prompt and run untouched:

```text
whp_os2_v2_hi.exe hi.exe
whp_os2_v2_hi.exe thread1.exe
whp_os2_v2_hi.exe <event/mutex torture executable>
whp_os2_v2_hi.exe <memory torture executable>
whp_os2_v2_hi.exe --check <representative LE/LX image>
```

Expected import output now includes names and routes, for example:

```text
DOSCALLS .282 DosWrite [shared]
DOSCALLS .311 DosCreateThread [intrinsic]
```

## Recommended next migration

The next useful common cut is the nonblocking file-handle layer: `DosRead`,
`DosClose`, `DosSetFileSize`, and file-status packing.  Treat `DosOpen` carefully:
the tree contains both the common eight-argument ABI and a pre-release
nine-argument C/386 form, so its catalogue argument size is intentionally zero
until the ABI adapter is explicit.

`DosSleep`, process APIs, thread APIs, and blocking semaphore calls should stay
intrinsic because WHP must deschedule and later restore a guest CPU context.
