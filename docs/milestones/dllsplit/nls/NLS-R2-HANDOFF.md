# OS2HOST32 — NLS R2 handoff
## Explicit per-process NLS state with a minimal Win32 bootstrap backend

Date: 2026-09-27

## Baseline

NLS R2 is based on the live-proven KBD/QUEUE/DOSCALLS/VIO/SESMGR split tree,
with SESMGR R2 live-tested by the user using regular START, PM START, VIO
sessions, PS/session enumeration and session termination.

The native ABI remains frozen.

### Frozen files

- `dlls/nls/nls.def`
  - SHA256 `8e1b567043be8767d6c93eee386ad21c57270eaa8421e8639315a874e998aef4`
- `dlls/doscalls/doscalls.def`
  - SHA256 `2a1d6d7aa4f9b1369f9d949ebd448ea76746f42bf23a460763c097fee6f2d13b`
- `loader/os2host32.c`
  - SHA256 `793ee7de7c6d0bd79b7d8bbdfaab6774cb2ec6af39b88d0cefc3c714430f0c55`
- `common/nls/os2_nls_tables.inc`
  - SHA256 `c215e0d84423bd6786a70d2bf7317a5ed8fbe805e79cea05b2916fccd356afbe`

## Objective

Turn the existing shared NLS algorithms into an explicitly owned per-process /
per-session personality state suitable for native Win32, WHP and future ReactOS
OS2SS backends.

The important change is ownership:

```text
before:
    common NLS algorithms
        + NLS state physically owned by Win32 DOSCALLS backend

R2:
    common NLS algorithms + common authoritative Os2NlsState
        + tiny backend used only to bootstrap host defaults
```

## Public ABI preserved

`NLS.DLL` remains:

- `DosQueryCtryInfo @5`
- `DosQueryDBCSEnv @6`
- `DosMapCase @7`

The existing DOSCALLS NLS exports remain unchanged:

- `DosSetProcessCp @289`
- `DosQueryCp @291`
- `DosQueryCtryInfo @395`
- `DosQueryDBCSEnv @396`
- `DosMapCase @397`

The private named DOSCALLS helpers imported by NLS.DLL are also retained.

No ordinal or signature changes were made.

## Common state

`struct Os2NlsState` is still the compatibility-visible type name, but R2 makes
it an explicit session object.  It now contains its backend binding plus the
OS/2-visible state:

- country;
- current process code page;
- prepared code pages.

`os2_nls_session_init()` constructs the state and optionally asks a backend for
an initial host profile.

`os2_nls_state_init()` remains as the deterministic no-backend initializer for
existing common callers/tests.

## Win32 backend

New files:

- `common/include/os2_nls_backend.h`
- `common/include/os2_nls_win32.h`
- `common/win32/os2_nls_win32.c`

The only R2 backend callback is `query_initial_profile`.

Win32 obtains country/OEM code page using `GetLocaleInfoA` and `GetOEMCP`, but
common NLS validates the result.  Unsupported host values fall back to the
implemented OS/2 defaults rather than leaking into guest-visible state.

The old `os2_win32_initialize_nls()` function has been removed from generic
`os2_win32_services`.

## Native DOSCALLS

`Os2DosSession` now embeds `struct Os2NlsState nls`.

The five NLS DOSCALLS APIs were removed from the transitional Win32 dispatch
hot path and now terminate directly in common NLS semantics.

This also means the three NLS.DLL calls, via their existing DOSCALLS private
helpers, observe exactly the same process state as `DosSetProcessCp` and
`DosQueryCp`.

## WHP

The WHP source and Makefile now bind NLS explicitly through
`os2_nls_win32_init_session()` and compile the new Win32 NLS backend source.

This is not a new WHP runtime feature.  It preserves the current Win32-hosted
bootstrap policy while making the replaceable backend boundary explicit.

## Tests added/strengthened

### Common NLS

`tests/nls/nls-core-check.c` now also verifies:

- backend bootstrap is queried exactly once per session initialization;
- supported host profile hints are accepted;
- unsupported host country/codepage hints do not leak into OS/2 state;
- NLS session instances are separately owned.

### Production Win32 NLS backend shim

New:

- `tests/nls/nls-win32-shim-check.c`
- `tests/nls/win32-stub/windows.h`
- `tests/nls/win32-stub/win32_nls_stub.c`

This compiles the actual `common/win32/os2_nls_win32.c` against deterministic
Win32 locale stubs.

### DOSCALLS ownership regression

The DOSCALLS core and actual veneer tests now verify that NLS calls execute in
common state and do not touch the fake legacy dispatch seam.

### Static architecture test

`tools/check_nls_r2.py` verifies:

- NLS session/backend binding exists;
- Win32 locale APIs do not appear in common NLS;
- generic Win32 services no longer own NLS bootstrap;
- `Os2DosSession` owns NLS state;
- the five common-owned NLS calls have no Win32 DOS dispatch cases;
- Win32 has no duplicate static `Os2NlsState`;
- WHP uses the explicit NLS backend initializer;
- NLS.DLL DEF remains byte-identical.

`tools/check_doscalls_r2.py` was also strengthened so these five NLS calls may
not silently reappear in the transitional Win32 dispatch.

## Verification

PASS:

- GCC strict-C89 full `make verify`
- Clang 17 strict-C89 full `make verify`
- NLS table reproducibility
- common NLS behavior/API test
- production Win32 NLS backend shim
- NLS R2 static architecture checker
- DOSCALLS core and actual veneer NLS ownership tests
- all existing VIO, QUECALLS, KBDCALLS, SESMGR and DOSCALLS regression suites
- production build dry-run dependency wiring for DOSCALLS and WHP

The environment does not contain `i686-w64-mingw32-gcc`, so a real native
MinGW DLL build/runtime test remains pending on the user's Windows host.
The x64 Visual Studio/WHP runtime is likewise not executed here.

## Runtime test recommendation

Build normally on the existing Win32 host:

```text
make clean
make all
make verify
```

Useful smoke tests:

1. CMD / existing demos to establish no regression.
2. `nlsinfo.exe` if desired.
3. Any specimen that calls `DosSetProcessCp(850)` followed by `DosQueryCp` and
   NLS.DLL `DosMapCase`/country queries, proving both modules share one process
   NLS state.

## Status

`NLS_R2_COMMON_PROCESS_STATE_WIN32_BOOTSTRAP_STATIC_VERIFIED_RUNTIME_PENDING`

## Next NLS work

Do not broaden NLS merely for completeness.  Good future milestones are driven
by actual callers, for example:

- `DosQueryCollate` / historical `DosGetCollate`;
- COUNTRY.SYS/resource loading;
- additional code pages/countries;
- an OS2SS-specific NLS bootstrap/resource backend.

Those should extend this common state rather than reintroducing host-owned NLS.
