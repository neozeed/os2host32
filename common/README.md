# Shared OS/2 personality core

This directory contains implementation code deliberately shared by the native
OS2HOST32 compatibility DLLs and the Win64 Windows Hypervisor Platform loader.

## Layout

- `include/` — public contracts for 32-bit OS/2 addresses, backend operations,
  shared DOSCALLS implementations, catalogue access, and shared Win32 services.
- `api/` — canonical module/ordinal/name catalogue and lookup implementation.
- `doscalls/` — loader-neutral DOSCALLS validation and OS/2 result semantics.
- `win32/` — small Win32 services genuinely identical in both products, such as
  local DATETIME acquisition and the monotonic millisecond clock.

## Address rule

The native loader and WHP must never exchange naked host pointers through this
layer.  Every application pointer is an `os2_addr32_t`.

- The native 32-bit DLL adapter treats that value as an address in its process.
- The Win64 WHP adapter validates it against guest RAM before mapping or writing.

Common routines own pointer/range validation, fixed OS/2 structure layouts, and
result writeback.  Backends own handle translation, actual host I/O, allocation,
and execution-engine operations.

## API routing

`api/os2_api_catalog.inc` labels each known ordinal as shared, backend-specific,
loader-intrinsic, or native-only.  Do not mark an API `SHARED` until both call
paths actually invoke the common implementation; `make wiring-check` enforces
that rule.

Calls which alter an execution engine—`DosExit`, process/thread creation and
waiting, semaphore scheduling, callbacks, and guest sleeps—remain loader
intrinsics.  Ordinary API semantics should move here as pointer-safe backend
contracts mature.

## Verification

From the repository root:

    make verify

The common behavioural test is host-portable and does not require Windows or WHP.
