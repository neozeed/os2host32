# WHP Milestone 3 — shared OS/2 personality core

## Milestone statement

The WHP loader no longer owns private implementations of every OS/2 system call.
For the first migrated API set, untouched guest code and native transformed code
execute the same loader-neutral DOSCALLS semantics.

## Shared API set

| Ordinal | API | WHP adapter responsibility |
|---:|---|---|
| 224 | `DosQueryHType` | guest HFILE to Win32 HANDLE |
| 230 | `DosGetDateTime` | safe guest write; common Win32 clock service |
| 256 | `DosSetFilePtr` | guest HFILE seek |
| 282 | `DosWrite` | map bounded guest buffer and write host handle |
| 299 | `DosAllocMem` | allocate tracked guest linear range |
| 304 | `DosFreeMem` | release guest allocation record |
| 305 | `DosSetMem` | validate guest range; backend commit/protection policy |
| 348 | `DosQuerySysInfo` | safe result write and shared monotonic clock |

The same `os2_core_*` functions are called from the native `DOSCALLS.DLL`
exports.  `make wiring-check` enforces both paths.

## Ordinal catalogue

`../common/api/os2_api_catalog.inc` is linked into WHP.  Import resolution,
`--check`, startup import summaries, and unsupported-call diagnostics can now
report the API name and its route rather than a bare ordinal.

Catalogue routes are:

- `shared` — common implementation currently active;
- `backend` — catalogued but still subsystem-specific;
- `intrinsic` — must control loader/scheduler state;
- `native-only` — not presently exposed by WHP.

## Memory safety boundary

Every common API receives 32-bit OS/2 addresses.  WHP validates each range
against the 64 MiB guest RAM mapping before converting it to `Runtime.ram + va`.
The common implementation cannot receive or dereference a fabricated Win64 host
pointer from guest code.

## What remains local

WHP still owns process and thread creation/exit, virtual-thread scheduling,
semaphore wait/wakeup logic, callbacks, far/16-bit transitions, guest DLL mapping,
`DosSleep`, and the blocking/scheduling portion of `DosBeep`.

Most file/path APIs remain marked `backend` for this milestone.  The catalogue
makes that remaining migration work visible; they must not be relabelled shared
until native and WHP actually call one common implementation.

## Build composition

The WHP executable is built from its active loader source plus:

```text
../common/doscalls/os2_doscalls_core.c
../common/api/os2_api_catalog.c
../common/win32/os2_win32_services.c
```

No x64 clone of the native system DLLs is produced, and no 32-bit DLL is loaded
into the Win64 process.

## Verification completed in the package workspace

The following toolchain-independent checks pass:

```text
make verify
PASS: shared DOSCALLS personality core and ordinal catalogue
PASS: 247 catalogue entries cover 245 ordinal DLL exports
PASS: native DOSCALLS and WHP share 8 catalogued DOSCALLS implementations
```

The common test also passes under GCC and Clang with strict C89 warnings, and
under AddressSanitizer/UndefinedBehaviorSanitizer.  `le2pe386` plus the catalogue
compiles cleanly with both host compilers.  Root and WHP production builds pass
complete dry runs.

This packaging environment does not contain MinGW, Visual C++, Windows headers,
or Windows Hypervisor Platform, so actual Windows binaries and guest runtime
results are not claimed here.  Those are the first required Windows validation
steps in the handoff.
