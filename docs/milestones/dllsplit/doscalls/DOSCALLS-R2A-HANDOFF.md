# OS2HOST32 DOSCALLS R2A compile-correction handoff

## Status

`DOSCALLS_R2A_COMMON_SESSION_SOURCE_CORRECTED_MINGW_RETRY_REQUIRED`

R2A is a narrow correction to the DOSCALLS R2 split after the first real
`i686-w64-mingw32-gcc` build exposed stale Win32-backend code that the
host-side/static test environment did not compile.

No public DOSCALLS ABI, export ordinal, common semantic contract, loader,
far16 descriptor, VIO implementation, WHP core, or application behavior is
intentionally changed.

## Failure observed on the real MinGW build

The released R2 package failed while compiling
`common/win32/os2_doscalls_win32.c` in two related areas:

1. `O2TibCompat`, `O2Tib2Compat`, and `O2PibCompat` had declarations/usages
   but their complete backend-private layout definitions had been removed.
2. The old Win32 `DosSubSetMem`, `DosSubAllocMem`, `DosSubFreeMem`, and
   `DosSubUnsetMem` implementation was still compiled even though subpool
   ownership and implementation had moved to `Os2DosSession`.

The same stale-code issue also left backend-owned exception-chain and
signal-focus implementations/dispatch cases present even though those
semantics had moved to common DOSCALLS.

## R2A correction

`common/win32/os2_doscalls_win32.c` now:

- restores only the backend-private TIB/TIB2/PIB presentation structures
  required by `DosGetInfoBlocks`;
- keeps the exception-chain head owned by `Os2DosSession` and mirrors it into
  the TIB presentation through the existing `exception_head_changed` backend
  callback;
- removes the duplicate Win32 `DosSub*` allocator and its dead dispatch cases;
- removes duplicate backend exception-chain and signal-focus semantics and
  their dead dispatch cases;
- removes dead backend dispatch for common-owned `DosSetRelMaxFH` and
  `DosAcknowledgeSignalException`;
- removes the `DosEnterCritSec` dispatch temporary that produced the reported
  unused-variable warning.

The public `.def` remains unchanged.

## Regression guard added

`tools/check_doscalls_r2.py` now fails if:

- `O2SubPool`/`O2SubRange` or subpool storage reappears in the Win32 backend;
- backend-owned exception/signal state reappears;
- a fully common-owned API is routed through the transitional backend dispatch;
- any of the concrete backend-private `DosGetInfoBlocks` TIB/PIB layouts is
  incomplete/missing.

This specifically guards the source-organization mistake that caused the
first real MinGW compile failure.

## Verification in this environment

`make verify` passes after the correction, including the strengthened
DOSCALLS R2 static architecture check and all existing DOSCALLS/VIO/NLS/shared
personality regressions.

This environment still does not provide `i686-w64-mingw32-gcc`, so R2A does
**not** claim a successful Win32 DLL build.  The next authoritative action is
to rerun the exact MinGW command that exposed R2's failure.

## Recommended retry

```text
make clean
make DOSCALLS.dll
make all
make verify
```

Or rerun the exact compiler command from the failing build.  If a subsequent
compiler/linker diagnostic appears, preserve it verbatim; R2A is intended to
remove every error present in the first uploaded diagnostic, but only the real
MinGW build can expose the next stage.

The stale `SHA256SUMS-DOSCALLS-R2.txt` from the superseded archive is intentionally omitted from the R2A package; use `SHA256SUMS-DOSCALLS-R2A.txt`.
