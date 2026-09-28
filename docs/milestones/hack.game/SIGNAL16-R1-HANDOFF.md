# OS2HOST32 — SIGNAL16 R1 HANDOFF

## Purpose

Add the two historical Microsoft C/386 LIBC `signal.asm` far16 dependencies
without introducing real 16-bit CPU execution or asynchronous 16-bit handler
delivery.

Observed unresolved CRT symbols:

- `SYSSETSIGHANDLER`
- `SYSSETVEC`

These are CRT-local import names for documented 16-bit DOSCALLS family APIs:

- `SYSSETSIGHANDLER=DOSCALLS.14` -> `DosSetSigHandler`
- `SYSSETVEC=DOSCALLS.89` -> `DosSetVec`

Authoritative project/reference material used:

- `sdk/os2h/bsedos16.h`
- `sdk/os2h/bseord.h`
- `docs/reference/microsoft-programmers-library/os2/prgmr3.txt`
- `docs/reference/microsoft-programmers-library/os2/os2smpl.txt`

## Historical signatures

`DosSetSigHandler`:

```c
USHORT DosSetSigHandler(PFNSIGHANDLER pfnSigHandler,
                        PFNSIGHANDLER FAR *pfnPrev,
                        PUSHORT pfAction,
                        USHORT fAction,
                        USHORT usSigNumber);
```

Historical ordinal: `DOSCALLS.14`.

`DosSetVec`:

```c
USHORT DosSetVec(USHORT usVecNum, PFN pfnFunction, PPFN ppfnPrev);
```

Historical ordinal: `DOSCALLS.89`.

The Microsoft Programmer's Reference describes `DosSetVec` as installing or
removing a process exception-vector handler and returning the previous handler.
A zero `pfnFunction` removes the current handler.

## C/386 bridge descriptors

The existing descriptor engine uses packed Pascal-frame order from low address
to high address (reverse source-argument order when widened onto the native
cdecl stack).

Added to the Microsoft C/386 descriptor table:

```text
DOSCALLS.14 DosSetSigHandler { 2, 2, 4, 4, 4 }
DOSCALLS.89 DosSetVec        { 4, 4, 2 }
```

`DOSCALLS.14` already existed in the EMX generic-far16 table; R1 adds it to the
Microsoft C/386 table as well.

## Common process state

`Os2DosSession` now owns:

- seven historical signal dispositions (`SIG_*` 1..7); and
- a small sparse table of historical exception-vector registrations.

Handler addresses are kept as opaque 32-bit far-pointer tokens. Common DOSCALLS
does not dereference or execute them.

### DosSetSigHandler semantics implemented

- validates historical signal numbers 1..7;
- returns previous handler token and previous action;
- `SIGA_ACCEPT` stores the supplied handler;
- `SIGA_KILL`, `SIGA_IGNORE`, and `SIGA_ERROR` store their disposition without
  a callable handler;
- `SIGA_ACKNOWLEDGE` succeeds without changing the persistent disposition;
- invalid signal number returns historical `ERROR_INVALID_SIGNAL_NUMBER` 209.

Actual asynchronous signal delivery remains outside this milestone.

### DosSetVec semantics implemented

- stores a per-process opaque handler token keyed by vector number;
- returns the previous handler token;
- a zero handler removes the registration;
- removal of an unregistered vector succeeds and reports previous handler 0;
- no handler is executed by this milestone.

The common layer is intentionally permissive about vector-number storage; the
historical reference documents the supported exception classes, while actual
exception delivery/validation belongs with the eventual execution backend.

## Exact DEF fragment for Microsoft C/386 LIBC

Add these local import aliases to the application's LINK386 definition file:

```text
IMPORTS
    SYSSETSIGHANDLER=DOSCALLS.14
    SYSSETVEC=DOSCALLS.89
```

A copy is provided as `tests/doscalls/c386-signal-imports.def`.

## Files changed

- `common/include/os2_doscalls.h`
- `common/doscalls/os2_doscalls.c`
- `dlls/doscalls/doscalls.c`
- `dlls/doscalls/doscalls.def`
- `common/api/os2_api_catalog.inc`
- `loader/os2host32.c`
- `tests/doscalls/doscalls-core-check.c`
- `tests/doscalls/doscalls-veneer-check.c`
- `tests/doscalls/c386-signal-imports.def`
- `tools/check_doscalls_r2.py`
- `tools/check_sesmgr_r2.py` (allows the later, intentional loader descriptor change)

## Verification

Host verification passes under both GCC and Clang in strict C89 mode, including
the complete existing `make verify` suite.

Tests cover:

- signal disposition install/replace/previous-state behavior;
- acknowledge and invalid-signal behavior;
- vector install/replace/remove/previous-state behavior;
- actual DOSCALLS veneer routing;
- exact C/386 far16 descriptor widths for ordinals 14 and 89;
- exact CRT-local `SYS*` import aliases;
- all pre-existing VIO/DOSCALLS/QUEUE/KBD/SESMGR/NLS/MOU regressions.

Real MinGW build and the user's C/386 hack link/runtime remain pending live test.

## Explicit non-goals

- no real 16-bit CPU execution;
- no asynchronous far16 signal-handler invocation;
- no IRET exception-frame synthesis;
- no Win32 SEH-to-DosSetVec translation;
- no new signal APIs;
- no changes to WHP or ReactOS OS2SS.

## Suggested live test

After rebuilding `os2host32.exe` and `DOSCALLS.dll`, add the two import aliases
to the hack's `.DEF`, relink it, and run it. If the CRT merely initializes its
signal machinery, registration should now succeed. A program that deliberately
raises a processor exception and expects its 16-bit `DosSetVec` handler to run
is beyond this milestone.

Suggested milestone status after native build but before the hack runs:

`SIGNAL16_R1_STATIC_VERIFIED_RUNTIME_PENDING`
