# Shared OS/2 personality architecture

## Purpose

The native OS2HOST32 path and the Win64/WHP path originally implemented OS/2
system calls in separate worlds:

```text
transformed PE -> 32-bit compatibility DLL -> native implementation
untouched LE   -> WHP hypercall dispatcher -> private WHP implementation
```

That was useful for proving WHP quickly, but every corrected flag, error path,
structure layout, or ordinal name then had to be changed twice.  This revision
introduces a common personality layer used by both execution backends.

## Resulting call paths

```text
Native transformed program
    -> DOSCALLS.DLL export veneer
    -> common DOSCALLS implementation
    -> native adapter operations

Untouched WHP guest
    -> synthetic ordinal veneer / OUT 0xF0
    -> WHP ordinal dispatcher
    -> the same common DOSCALLS implementation
    -> guest-safe WHP adapter operations
```

The common implementation is source code, not a DLL binary.  A Win64 WHP process
cannot load the project's 32-bit `DOSCALLS.DLL`, and compiling the old DLL source
unchanged as Win64 would be unsafe because its OS/2 pointers are direct process
pointers.  Both products therefore compile the same common source with different
address-space adapters.

## Pointer contract

All application addresses crossing the shared boundary are represented as
`os2_addr32_t`, never as host pointers.

- The native 32-bit adapter maps the address to the transformed process directly.
- The WHP adapter bounds-checks the address against guest RAM and then maps or
  copies it through `Runtime.ram`.

The common code performs null, length, overflow, and output-buffer validation
before invoking backend operations.  This keeps guest-pointer rules consistent
without allowing a malformed guest address to be dereferenced by Win64 code.

## Canonical ordinal catalogue

`common/api/os2_api_catalog.inc` is the project-wide module/ordinal/name catalogue.
It currently contains 247 entries:

- 245 entries represented by native compatibility DLL DEF exports;
- `DOSCALLS.336` (`DosQueryMutexSem`) and `DOSCALLS.349`
  (`DosWaitThread`), which are present in the WHP execution path.

Each entry has one route:

- `SHARED` — one common semantic implementation is compiled into both products.
- `BACKEND` — the API is catalogued but still uses subsystem/backend-specific
  implementation code.
- `INTRINSIC` — the operation controls the loader or scheduler and must remain
  execution-engine-specific.
- `NATIVE_ONLY` — currently exported only by the native compatibility DLL set.

An argument-byte value of zero means that the ABI has not yet been canonicalised
or has known historical variants.  `DosOpen`, for example, appears in both
8-argument and pre-release 9-argument forms and is deliberately not assigned a
misleading fixed byte count.

The catalogue is used by:

- `le2pe386`, when reporting ordinal imports;
- WHP import resolution and runtime diagnostics;
- validation tools which compare catalogue entries against DLL DEF exports.

## Initial shared DOSCALLS set

| Ordinal | API | Common responsibility | Backend responsibility |
|---:|---|---|---|
| 224 | `DosQueryHType` | pointer/result contract | translate OS/2 HFILE and classify native handle |
| 230 | `DosGetDateTime` | exact 12-byte OS/2 DATETIME layout | acquire local Win32 time/time-zone data |
| 256 | `DosSetFilePtr` | origin validation and result writeback | translate HFILE and seek the host handle |
| 282 | `DosWrite` | buffer validation/mapping and count writeback | translate HFILE and perform host write |
| 299 | `DosAllocMem` | request validation and pointer writeback/rollback | allocate native or guest address space |
| 304 | `DosFreeMem` | common null contract | release native allocation or WHP allocation record |
| 305 | `DosSetMem` | common range contract | change native protection/commit or WHP guest state |
| 348 | `DosQuerySysInfo` | QSV values and packed output | monotonic millisecond clock |

The Win32 time service used by ordinals 230 and 348 is itself shared under
`common/win32/`.

## Deliberately loader-specific operations

The following classes remain loader intrinsics:

- process and thread exit;
- guest thread creation, waiting, suspension, and scheduling;
- event/mutex semaphore waits and wakeups;
- process creation and child waiting;
- callback transitions between guest and host execution;
- `DosSleep` and the WHP blocking portion of `DosBeep`;
- 16-bit/far-pointer transitions.

Keeping these local is not accidental duplication: the native path delegates to
Windows threads/processes, while WHP must save guest CPU state and schedule
multiple OS/2 contexts on one virtual processor.

## Build integration

The root build compiles:

```text
common/doscalls/os2_doscalls_core.c
common/win32/os2_win32_services.c
    -> DOSCALLS.DLL
```

The WHP build compiles:

```text
common/doscalls/os2_doscalls_core.c
common/api/os2_api_catalog.c
common/win32/os2_win32_services.c
    -> whp/whp_os2_v2_hi.exe
```

The WHP Makefile still has one production output only.

## Verification

`make verify` checks three independent properties:

1. common API behaviour using a bounded mock address space;
2. catalogue uniqueness and coverage of all native ordinal exports;
3. wiring: every API marked `SHARED` must call the common implementation from
   both the native DLL export and the WHP ordinal dispatcher.

The common test is C89-clean under GCC and Clang and has also been exercised with
AddressSanitizer/UndefinedBehaviorSanitizer.  Windows runtime regression remains
required after building with MinGW and the x64 Visual Studio/WHP toolchain.

## Next migration rule

A new API should not be marked `SHARED` merely because both loaders implement it.
First define a pointer-safe backend contract, move the common validation and OS/2
semantics into `common/`, make both paths call it, and then change its catalogue
route.  The wiring checker will thereafter prevent either path from silently
forking it again.
