# M29N2b.2 — 32-bit guest DLL initialization and termination lifecycle

M29N2b.1 put the runtime OS/2 module-manager API on top of the same guest
LE/LX module registry used for static imports. M29N2b.2 adds the missing
lifecycle layer for **32-bit guest DLLs**: initialization after dependencies
are ready, termination before dependencies are released, failed-init rollback,
and process-exit termination in reverse successful-init order.

This remains deliberately narrower than a complete OS/2 loader. The milestone
proves the lifecycle ABI and ordering with CRT-free C/386 DLLs before we let a
large runtime such as EMX exercise it.

## What changed

For an ordinary guest DLL with an LE/LX library entry point, OS2HOST32 now calls
that entry as:

```text
ULONG DllInitTerm(HMODULE hmod, ULONG flag)

flag = 0   initialization
flag = 1   termination
```

The bridge supplies the process-local guest `HMODULE`. A nonzero return from
INIT means success; zero means initialization failure and is surfaced through
`DosLoadModule` as error 295 (`ERROR_INIT_ROUTINE_FAILED`). A failed module is
removed from the registry and references it acquired on dependencies are
released.

Initialization is dependency-first. For this graph:

```text
M29N2C.DLL
    |
    `--> M29N2D.DLL
```

D initializes before C. On a final `DosFreeModule(C)`, C terminates while D is
still mapped and callable, then C releases its dependency and D terminates.

For process exit, DOSCALLS' `DosExit` asks OS2HOST32 to terminate every
successfully initialized guest DLL in reverse initialization order while all
module images are still mapped.

## Lifecycle fixture DLLs

The test DLLs are intentionally CRT-free.

`M29N2D.DLL` is the leaf. Its init routine sets private state and prints:

```text
M29N2B2_INIT_D
```

Its exported value is valid only while initialized. Its term routine prints:

```text
M29N2B2_TERM_D
```

`M29N2C.DLL` imports `M29N2DValue` and proves dependency ordering. During C's
INIT it calls D and expects initialized state:

```text
M29N2B2_INIT_C_AFTER_D_OK
```

During C's TERM it deliberately calls D again. The required result is:

```text
M29N2B2_TERM_C_SEES_D_OK
```

That catches the subtle but important case where a loader unmaps/releases
child dependencies before running the parent's termination routine.

`M29N2F.DLL` is the negative fixture. Its initialization routine prints:

```text
M29N2B2_FAIL_INIT_CALLED
```

and returns zero. `DosLoadModule` must return 295 and `DosQueryModuleHandle`
must subsequently report that M29N2F is absent. Its TERM routine must never
run.

## Why there is a small SETENTRY build helper

The recovered prerelease Microsoft C/386 kit in this project does not contain
the tiny SETENTRY/DLLINIT startup object used by Microsoft's custom DLL-startup
recipe. The lifecycle fixtures nevertheless need a real LE/LX module entry
point rather than a private OS2HOST32 convention.

`m29n2b2-setentry.exe` is therefore a **build-time fixture helper only**. After
LINK386 creates each test DLL, it finds exported ordinal 2 (the fixture's
`...InitTerm` function), writes that object/offset into the LE/LX library-entry
fields, and sets the per-process initialization/termination module flags.
OS2HOST32 never patches guest binaries at runtime.

This is intentionally limited to the regression DLLs; it is not intended to
replace a future recovered historical DLL startup object.

## Build

M29N2b.2 changes both OS2HOST32.EXE and DOSCALLS.dll, and adds the build helper.
Build the host first, then the C/386 lifecycle fixtures:

```cmd
cd C:\2\m29n2b2_work
make
build-m29n2b2-lifecycle.cmd
```

The script creates:

```text
m29n2b2-dll\M29N2C.DLL
m29n2b2-dll\M29N2D.DLL
m29n2b2-dll\M29N2F.DLL
m29n2b2-dynamic.exe
m29n2b2-static.exe
```

## Scan the DLLs first

```cmd
os2host32 --scan m29n2b2-dll\M29N2C.DLL
os2host32 --scan m29n2b2-dll\M29N2D.DLL
os2host32 --scan m29n2b2-dll\M29N2F.DLL
```

After the SETENTRY helper, each should show a nonzero DLL entry object/offset.
C should import M29N2D.1 and DOSCALLS.282; D and F should import DOSCALLS.282.

## Dynamic lifecycle regression

```cmd
set OS2LIBPATH=m29n2b2-dll;.
set OS2_TRACE_MODULES=1
os2host32 --run m29n2b2-dynamic.exe
```

The important ordering should be:

```text
GUESTMOD INITCALL: M29N2D ...
M29N2B2_INIT_D
GUESTMOD INITRET: M29N2D ... result=00000001

GUESTMOD INITCALL: M29N2C ...
M29N2B2_INIT_C_AFTER_D_OK
GUESTMOD INITRET: M29N2C ... result=00000001

LIFECYCLE_C_VALUE=29B20043
```

A repeated `DosLoadModule("M29N2C")` must acquire the same instance and must
**not** call either INIT again. The first `DosFreeModule` leaves C resident.
The final free should then show:

```text
GUESTMOD TERMCALL: M29N2C ...
M29N2B2_TERM_C_SEES_D_OK
GUESTMOD TERMRET: M29N2C ...

GUESTMOD TERMCALL: M29N2D ...
M29N2B2_TERM_D
GUESTMOD TERMRET: M29N2D ...
```

followed by both modules disappearing from the registry.

The failed-init portion should show:

```text
M29N2B2_FAIL_INIT_CALLED
FAILED_INIT_RC=295
FAILED_INIT_QUERY_RC=126
```

and must **not** print `M29N2B2_FAIL_TERM_SHOULD_NOT_RUN`.

A successful dynamic run ends with:

```text
M29N2B2_DYNAMIC_LIFECYCLE_OK
```

## Static-load + process-exit regression

`m29n2b2-static.exe` imports M29N2C at link time. This checks that the same
lifecycle applies during the initial dependency walk, before the process entry
point is transferred to the EXE.

```cmd
os2host32 --run m29n2b2-static.exe
```

Before the program body, D then C should initialize. The program prints:

```text
M29N2B2_STATIC_VALUE=29B20043
M29N2B2_PROCESS_EXIT_BODY_OK
```

When the C/386 runtime reaches `DosExit`, process-exit termination should run C
then D, with C still able to call D from its TERM routine.

## Shell regression

From either native CMD32OS2 or the C/386-hosted CMD shell:

```cmd
examples\m29n2b2-lifecycle-test.cmd
```

A successful run ends with:

```text
M29N2B2_DYNAMIC_LIFECYCLE_OK
M29N2B2_PROCESS_EXIT_BODY_OK
M29N2B2_LIFECYCLE_SCRIPT_OK
```

The lifecycle trace lines described above should appear when
`OS2_TRACE_MODULES=1` is present in the environment.

## Deliberate boundaries

M29N2b.2 only invokes **32-bit flat guest DLL entry points**. It still fails
closed on 16-bit DLL lifecycle entry points and the unproven loader/fixup forms
already excluded by N2a/N2b.1. Thread-level DLL attach/detach semantics are not
implemented. Personality modules remain host-backed rather than guest DLLs.

Termination return values are traced but not used to veto unload, matching the
fact that termination is cleanup rather than acquisition.

With this milestone the loader has both static and dynamic module discovery,
real guest module handles/refcounts, and a usable process-level DLL lifecycle.
That is enough foundation to begin experimenting with less artificial runtime
DLLs in a later milestone without conflating failures in discovery, lookup,
reference management, and initialization.
