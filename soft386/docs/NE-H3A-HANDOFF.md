# Soft386 NE-H3A handoff

## Milestone

NE-H3A adds the first OS/2 1.x 16-bit NE executable path to Soft386 without changing the existing system support DLLs or replacing the existing 32-bit LE/LX loader.

The first frozen target is `tests/fixtures/void-ne16.exe`, SHA256:

`abd4f42e8f185bb96078282d39074e5ac825cb580b2c16cb6e6d0b630041ebc9`

Its source is simply `int main(){return 0;}`.  Despite that trivial source, the Microsoft 16-bit C runtime exercises enough OS/2 startup machinery to make it a useful loader/ABI proof.

## What is implemented

All implementation changes are under `soft386/`.

* Detect MZ+NE OS/2 executables before the existing LE/LX path.
* Materialize separate 16-bit NE segments into Soft386 guest RAM.
* Install 16-bit protected-mode GDT descriptors/selectors for NE code/data.
* Build OS/2-style initial CS:IP, SS:SP, DS, environment and info-segment state.
* Parse NE module/import tables.
* Apply internal NE selector/far-pointer relocations.
* Apply **chained** NE relocation source sites (required by this Microsoft image).
* Resolve 16-bit `DOSCALLS` ordinal imports to a Soft386-owned 16-bit stub segment.
* Bridge those stubs inside Soft386 to jar-owned semantics / the existing common personality; no 16-bit system DLL is introduced.
* Preserve the existing 32-bit LE/LX path and its system bridge architecture.

## DOSCALLS surface required by VOID.EXE

The image imports the original 16-bit DOSCALLS ordinal ABI, not the later 32-bit ordinal ABI.  NE-H3A currently implements the eleven ordinals reached by the target runtime:

* 5 `Dos16Exit`
* 8 `Dos16GetInfoSeg`
* 34 `Dos16AllocSeg`
* 38 `Dos16ReallocSeg`
* 77 `Dos16QHandType`
* 85 `Dos16SetMaxFH`
* 89 `Dos16SetVec`
* 138 `Dos16Write`
* 140 `Dos16SemRequest`
* 141 `Dos16SemClear`
* 142 `Dos16SemWait`

The 16-bit bridge is intentionally narrow.  It is the place to add historical 16-bit ABI marshalling as later NE programs require it.  The existing 32-bit support DLL ABI should remain unchanged.

## Live host-side proof

Running:

```
./soft386_os2 --trace-hc --max-cycles 500000 tests/fixtures/void-ne16.exe
```

reaches the untouched Microsoft runtime, crosses the DOS16 bridge repeatedly, and finishes with:

```
soft386: DOS16.5 SS:SP=0118:0E18
---------------- guest ended -----------------
soft386: termination=Dos16Exit(EXIT_PROCESS) rc=0 cycles=219650
```

Observed DOS16 sequence:

`8, 140, 34, 141, 140, 38, 141, 140, 141, 85, 77, 77, 77, 89, 140, 38, 141, 140, 141, 5`

## Regression status

`make check-quick` passes after NE-H3A, including the new NE target plus the existing 32-bit LE hello/thread/sync/memory tests and DOSCALLS/system bridge checks.

The larger `make check` suite was started but takes longer than the available build-command execution window because it recompiles several Tiny386 runtime-check binaries; no test failure was observed before the external timeout.  `check-quick` completed cleanly.

A Win32 cross compiler was not present in the build environment, so the final MinGW/RosBE Windows executable remains a user-side build/runtime check.

## Scope deliberately not claimed yet

* General NE/DLL module loading.
* 16-bit VIO/KBD/MOU/SESMGR thunk surfaces beyond what a later target proves necessary.
* Mixed 16/32 modules or call gates in arbitrary applications.
* Full OS/2 info-segment semantics.
* Async signal/vector delivery (the current `Dos16SetVec` support is only sufficient for this CRT startup path).
* General shared-memory / interprocess semaphore semantics.
* Arbitrary NE relocation types/import-by-name forms.

## Next target strategy

Keep the same architecture: choose a small real 16-bit text executable, inventory its imported historical ordinals, add only the missing ABI adapters in Soft386, and leave the common/system DLLs unchanged.  A program that actually calls `Dos16Write` is a useful immediate next step because the VOID runtime imports it but does not need to print user text.
