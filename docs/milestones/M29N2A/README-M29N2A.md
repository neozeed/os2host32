# M29N2a — real recursive LE/LX guest DLL loading

M29N1 separated the OS/2 command namespace from the Win32 host and reserved
`OS2LIBPATH`.  M29N2a makes that module namespace real: an import which is not
one of the built-in OS2HOST32 personality modules is now searched on
`OS2LIBPATH` and loaded as a genuine OS/2 LE/LX library module inside the same
host process.

This is the first milestone where OS2HOST32 can host a **graph of OS/2
modules**, rather than one guest executable surrounded entirely by Win32
personality DLLs.

## Resolution model

Known personality module names remain host-backed:

```text
DOSCALLS   KBDCALLS   VIOCALLS   QUECALLS
SESMGR     PMWIN      PMGPI
```

Every other imported module is treated as an OS/2 user DLL:

```text
APP.EXE
   |
   +--> DOSCALLS       -> Win32 OS2HOST32 personality
   |
   `--> MYAPP.DLL      -> search OS2LIBPATH
           |
           +--> DOSCALLS   -> personality
           `--> HELPER.DLL -> search OS2LIBPATH recursively
```

The default module path is `.` if `OS2LIBPATH` is absent.  Quoted path elements
are accepted, and `env.cmd` now seeds:

```text
OS2LIBPATH=.;C:\OS2\DLL
```

independently from `OS2PATH` / guest `PATH`.

## Loader foundation added in N2a

For a guest DLL, OS2HOST32 now:

1. locates the module on `OS2LIBPATH`;
2. parses the LE/LX image as a library module (including DLLs with no process
   entry object or stack);
3. registers the module before following dependencies, so recursive dependency
   graphs do not repeatedly map the same library;
4. maps and relocates the DLL's objects independently from the executable;
5. recursively resolves its own import modules;
6. resolves exports by ordinal or by resident/non-resident export name;
7. applies the currently proven flat-32 internal and external fixups; and
8. protects the mapped objects with their final access permissions.

The module registry is process-local.  Two imports of the same module name
(with or without a `.DLL` suffix) reuse the same mapped guest module.

### Deliberate N2a boundary

M29N2a does **not** yet invoke a DLL initialization/termination entry point and
does not expose full runtime `DosLoadModule` / `DosFreeModule` reference-count
semantics.  Those belong to M29N2b.

For that reason the supplied test DLLs are deliberately tiny and CRT-free.
They contain callable 32-bit code but require no C runtime DLL startup.

16-bit DLL exports, 286 call-gate exports, entry-table forwarders, chained
fixups, and other unproven fixup forms still fail closed rather than being
silently approximated.

## Regression graph

The new fixture intentionally uses two real Microsoft C/386 OS/2 DLLs:

```text
m29n2a-import.exe
       |
       | imported by NAME
       v
   M29N2A.DLL
       |
       | imported by ORDINAL 1
       v
   M29N2B.DLL
```

`M29N2B.DLL` exports ordinal 1, `M29N2BValue()`, which returns
`0x29A20042`.  `M29N2A.DLL` calls it and returns one more.  The executable must
therefore print:

```text
M29N2A_CHAIN_VALUE=29A20043
M29N2A_GUEST_DLL_CHAIN_OK
```

The executable imports `M29N2AValue` **by name**, while M29N2A imports M29N2B
**by ordinal**.  One run therefore tests both export lookup paths as well as
recursive DLL loading.

## Build on the C/386 machine

First rebuild the host personalities/loader, then build the three guest
fixtures:

```cmd
cd C:\2\m29n2a_work
make
build-m29n2a-dll.cmd
```

The DLL build deliberately uses `/Zl` and no libraries for the two DLLs.  The
normal test EXE is linked against the usual `LIBC.LIB` and `OS2386.LIB`.

If the C/386 compiler/linker in a particular kit disagrees with the fixture DEF
syntax, keep the linker output: the first real-machine link is intentionally
part of this loader probe.

## Inspect the library images first

```cmd
os2host32 --scan m29n2a-dll\M29N2A.DLL
os2host32 --scan m29n2a-dll\M29N2B.DLL
os2host32 --scan m29n2a-import.exe
```

Useful things to look for:

* M29N2A is a library module and imports M29N2B;
* M29N2B is a library module with its exported entry;
* the executable imports M29N2A by name;
* the ordinary DOSCALLS/VIO/KBD machinery used by the executable remains
  unchanged.

## Direct loader regression

```cmd
set OS2LIBPATH=m29n2a-dll;.
set OS2_TRACE_MODULES=1
os2host32 --run m29n2a-import.exe
```

The trace should have this general shape (addresses and exact scan counts will
vary):

```text
GUESTMOD LOAD: M29N2A -> m29n2a-dll\M29N2A.dll
GUESTMOD LOAD: M29N2B -> m29n2a-dll\M29N2B.dll
GUESTMOD READY: M29N2B ...
GUESTMOD EXPORT: M29N2B.1 -> ...
GUESTMOD READY: M29N2A ...
GUESTMOD EXPORT: M29N2A.M29N2AValue -> ...
M29N2A_CHAIN_VALUE=29A20043
M29N2A_GUEST_DLL_CHAIN_OK
```

If LINK386 emits a DLL entry object even for these CRT-free fixtures, N2a will
also print `GUESTMOD INIT: ...` explaining that the entry has been mapped but
is intentionally not invoked yet.

## Shell regression

The same loader is reached naturally from either CMD32OS2 frontend.  Run:

```cmd
examples\m29n2a-guest-dll-test.cmd
```

It sets `OS2LIBPATH=m29n2a-dll;.` and enables module tracing.  A successful run
finishes with:

```text
M29N2A_GUEST_DLL_CHAIN_OK
M29N2A_GUEST_DLL_CHAIN_SCRIPT_OK
```

## Why this milestone is intentionally small

This is only the static-import foundation.  Once this graph is proven on the
real C/386 output, M29N2b can put the dynamic module APIs and lifecycle on top
of the **same** registry rather than inventing a second loader:

```text
DosLoadModule
DosGetModHandle
DosGetProcAddr
DosFreeModule
DLL init/term
reference counts
```

That is also the foundation needed before trying substantially larger DLL-based
OS/2 software such as EMX applications.
