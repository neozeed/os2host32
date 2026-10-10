# PMWIN 867 / VIOFETCH R1

Based on the supplied os2host32-main.zip and viofetch.zip, 10 October 2026.

## Changes

- PMWIN.867 is WinSetPointerPos, confirmed by sdk/os2h/bseord.h. The export
  calls SetCursorPos with the same desktop pixel conversion used by
  WinQueryPointerPos: native Y = screen height - 1 - OS/2 Y. Its ordinal, API
  catalogue entry and Soft386 typed-handle/scalar ABI descriptor are included.
- VIOCALLS.48 already existed with the correct C/386 far16 argument widths.
  The Win32 renderer used absolute backing-buffer coordinates and dimensions.
  Writes to small row numbers could therefore land in hidden scrollback. The
  renderer now maps positioned writes, reads, cursor access, clears and upward
  scrolling to the visible window. Linear spans split at the visible width,
  including horizontal panning. Scroll operations carry an explicit clip.
- Viewport bookkeeping stays in the Win32 backend. Explicit mode queries
  invalidate the shared cell cache when the origin moves. An unchanged mode
  retains the original OS/2 attributes, including blink. No whole-screen read
  has been added to the TTY hot path. TTY retains its existing WriteFile stream
  semantics; its newline/horizontal-pan behavior remains Win32 stream behavior.
- VioGetMode respects an eight-byte requested prefix, preserving adjacent bytes.
- Shared DosQuerySysInfo adds memory fields 17..21 via an optional host memory
  primitive. Win32 DOSCALLS and Win32 Soft386 use GlobalMemoryStatusEx. Physical
  total, physical used, remaining commit budget, and process virtual address
  budget are serialized as OS/2 ULONGs with saturation rather than overflow.
  Resident/private/shared values are compatibility approximations of the host
  budgets, not a measurement of historical OS/2 memory pools. Backends without
  this primitive retain their existing zero fallback. The WHP source is unchanged.
- proj/viofetch restores version, uptime, memory, codepage, country, drive and
  disk queries. Its C/386 build keeps DOS and NLS flat and uses explicit far16
  Pascal VIO declarations. NLS.5 owns the country import. It checks failures,
  initializes every output, removes the unsafe information-segment dereference,
  clips text, and handles disks larger than 4 GiB without a byte-product overflow.

## Separation

OS/2 VIO state remains in common/vio; console operations remain in common/win32;
VIOCALLS is a veneer. DOS serialization and bounds remain in the shared core;
GlobalMemoryStatusEx remains in common/win32. NLS still delegates to the existing
process NLS service. MSG is unchanged. PM remains the direct native UI interface.

The compiled DLL import tables confirm:

| DLL | Direct native imports relevant to separation |
| --- | --- |
| DOSCALLS | KERNEL32, MSVCRT, WINMM |
| VIOCALLS | KERNEL32, MSVCRT |
| NLS | DOSCALLS, KERNEL32, MSVCRT |
| MSG | KERNEL32, MSVCRT |
| PMWIN | USER32, GDI32, KERNEL32, MSVCRT |

The runtime package uses static libgcc where needed and does not require an
additional GCC runtime DLL. It includes replacement PMWIN, VIOCALLS, DOSCALLS,
NLS and Soft386 binaries plus viofetch-pe.exe. Other existing compatibility DLLs
remain necessary for programs using their APIs. Back up the old binaries and
replace files while the host and its child programs are closed.

## Validation

- Full production Win32 build: PASS, i686-w64-mingw32 GCC 13.
- Active Soft386 Linux and Win32 builds: PASS.
- Shared core: memory saturation, subtraction before narrowing, unavailable
  backend fallback, backend failure and inconsistent counters: PASS.
- VIO core, Win32 renderer, far16/static surface checks: PASS. Includes an
  offset viewport, wrap, attributes, hidden-cell preservation, rectangle scroll,
  unchanged blink shadow and short mode prefix.
- Actual PMWIN pointer export bodies compiled with deterministic Win32 calls:
  bottom/top pixel origins, signed X, query/set round trip, host failures: PASS.
- Soft386 DOS, system and PM bridge regressions: PASS. PM includes forwarding
  ordinal 867 with a desktop handle and signed coordinates.
- viofetch source regression: all restored fields, exact structure sizes,
  large-disk arithmetic, failed queries and failed VIO operations: PASS.
- Native PE viofetch build: PASS. Imports checked: DOSCALLS.275/.278/.291/.348,
  NLS.5 and VIOCALLS.7/.15/.21/.48.
- Platform, DOS, NLS and other existing make verify functional checks: PASS.

`make -k verify` still reports two pre-existing frozen-hash failures:
`tools/check_doscalls_r2.py` and `tools/check_sesmgr_r2.py`. Both failures were
reproduced against the untouched uploaded snapshot. DOSCALLS.def and
loader/os2host32.c remain byte-for-byte identical to that snapshot. The checks
were not loosened to conceal these baseline mismatches.

No live Windows/OS/2 execution is claimed. The Microsoft C/386 viofetch build
also needs live compilation/linking with the user's historical tools. Captain
Blood was not supplied here, so ordinal 867 is implemented and bridge-tested,
but its full PM import list and game behavior remain for the next live run.

## Deliverables

- os2host32-PMWIN867-VIOFETCH-R1.patch: source-only patch against the upload.
  Apply from the repository root with `git apply` or `patch -p1`.
- os2host32-PMWIN867-VIOFETCH-R1-source.zip: updated snapshot, including the
  restored viofetch source, build files, tests and this handoff. Generated build
  outputs are supplied separately.
- os2host32-PMWIN867-VIOFETCH-R1-win32.zip: replacement binaries and native
  viofetch diagnostic. Run viofetch-pe.exe directly from an ordinary console.
