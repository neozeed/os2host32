# M29N2b.1 — runtime module handles and dynamic loading

M29N2a proved that OS2HOST32 can map a recursive graph of genuine LE/LX guest
DLLs from `OS2LIBPATH`. M29N2b.1 puts the 32-bit OS/2 module-manager API on top
of **that same module graph** rather than creating a second loader.

The new DOSCALLS entry points are the 32-bit OS/2 ordinals:

```text
318  DosLoadModule
319  DosQueryModuleHandle
321  DosQueryProcAddr
322  DosFreeModule
```

(`DosQueryModuleHandle` / `DosQueryProcAddr` are the 32-bit successors to the
older `DosGetModHandle` / `DosGetProcAddr` names.)

## Architecture

DOSCALLS.dll is a Win32 personality DLL, while the guest LE/LX module registry
lives in OS2HOST32.EXE. M29N2b.1 therefore adds only a narrow private bridge:

```text
guest EXE
   |
   +-- DOSCALLS.318 DosLoadModule
   |       |
   |       `--> OS2HostModuleLoad  ----+
   |                                    |
   +-- DOSCALLS.319 QueryHandle         |
   +-- DOSCALLS.321 QueryProc           +--> one guest-module registry
   +-- DOSCALLS.322 FreeModule          |
                                        |
                     static imports ----+
```

The private bridge is an implementation detail. Guest programs still see the
normal OS/2 DOSCALLS ordinals.

## Reference model in this phase

Each module record now has a stable process-local `HMODULE` and reference
count. Static imports take a reference. Each successful `DosLoadModule` takes
another reference. `DosQueryModuleHandle` and `DosQueryProcAddr` do not.
`DosFreeModule` drops one reference.

When a module reaches zero references, its guest dependencies are released,
its mapped LE/LX objects are unmapped, host personality import references are
released, and the module record disappears. This means the test graph:

```text
M29N2A.DLL -> M29N2B.DLL
```

can be loaded twice with one stable A handle, freed once and remain alive, then
freed a second time and unload both A and its otherwise-unreferenced B.

M29N2b.1 still does **not** invoke DLL initialization/termination entrypoints.
That lifecycle work is deliberately M29N2b.2.

## New dynamic regression

`m29n2b1-dynamic.exe` has no static import from M29N2A. It performs:

```text
DosQueryModuleHandle("M29N2A")       -> initially not found
DosLoadModule("M29N2A")             -> handle A
DosLoadModule("M29N2A")             -> same handle A, refcount 2
DosQueryModuleHandle("M29N2B")      -> finds recursively loaded B
DosQueryProcAddr(A, name)            -> M29N2AValue
DosQueryProcAddr(B, ordinal 1)       -> M29N2BValue
call both exports
DosFreeModule(A)                     -> A remains (refcount 1)
DosFreeModule(A)                     -> A unloads and releases B
Query A/B                            -> both absent
```

The values must remain:

```text
DYNAMIC_A_VALUE=29A20043
DYNAMIC_B_VALUE=29A20042
M29N2B1_DYNAMIC_MODULE_API_OK
```

## Build

Rebuild the host because both OS2HOST32.EXE and DOSCALLS.dll changed, then
build the guest DLL chain and dynamic caller:

```cmd
cd C:\2\m29n2b1_work
make
build-m29n2b1-module-api.cmd
```

The build script first reuses the proven M29N2a CRT-free A -> B DLL fixtures,
then links the dynamic caller. Its DEF explicitly imports the four 32-bit
DOSCALLS ordinals so this test does not depend on import-library naming aliases.

## Scan first

```cmd
os2host32 --scan m29n2b1-dynamic.exe
```

The important result is that the executable should import DOSCALLS ordinals
318, 319, 321 and 322 but **must not** import M29N2A statically.

## Direct test

```cmd
set OS2LIBPATH=m29n2a-dll;.
set OS2_TRACE_MODULES=1
os2host32 --run m29n2b1-dynamic.exe
```

Expected trace shape:

```text
GUESTMOD LOAD: M29N2A ... handle=00010000 refs=1
GUESTMOD LOAD: M29N2B ... handle=00010001 refs=1
GUESTMOD READY: M29N2B ...
GUESTMOD READY: M29N2A ...
GUESTMOD ACQUIRE: M29N2A handle=00010000 refs=2
LOAD_A1_HANDLE=00010000
LOAD_A2_HANDLE=00010000
QUERY_B_HANDLE=00010001
GUESTMOD QUERYPROC: ... name=M29N2AValue ...
DYNAMIC_A_VALUE=29A20043
GUESTMOD QUERYPROC: ... ordinal=1 ...
DYNAMIC_B_VALUE=29A20042
GUESTMOD RELEASE: M29N2A ... refs=1
GUESTMOD RELEASE: M29N2A ... refs=0
GUESTMOD RELEASE: M29N2B ... refs=0
GUESTMOD UNLOAD: M29N2B ...
GUESTMOD UNLOAD: M29N2A ...
M29N2B1_DYNAMIC_MODULE_API_OK
```

Addresses and handle values are diagnostic, not contractual.

## Shell test

From either native CMD32OS2 or the C/386-hosted shell:

```cmd
examples\m29n2b1-module-api-test.cmd
```

A successful run ends with:

```text
M29N2B1_DYNAMIC_MODULE_API_OK
M29N2B1_DYNAMIC_MODULE_API_SCRIPT_OK
```

## Deliberate boundary

N2b.1 is only dynamic module discovery/handles/export lookup/refcounting. It
still fails closed on the unsupported DLL forms from N2a, and does not yet run
DLL init/term entrypoints. Personality-module handles (for example dynamically
querying DOSCALLS itself) are also not part of this first guest-module API
slice; the regression is intentionally limited to genuine LE/LX user DLLs.

M29N2b.2 can now concentrate on initialization ordering, termination ordering,
and the ABI of DLL entrypoints without mixing that problem with module-handle
semantics.
