# Soft386 OS/2 R2 handoff

## Objective

Expand the live-proven R1 jar/native split from DOSCALLS to the character-mode
system module set required to intake and begin executing reconstructed C/386
programs such as CMD32 and NLSINFO, while remaining a 32-bit-only Tiny386
execution milestone.

## Added in R2

1. Multi-module LE/LX import resolver for DOSCALLS, VIOCALLS, KBDCALLS, SESMGR
   and direct NLS imports.
2. Separate hostcall module IDs instead of treating every import as DOSCALLS.
3. Win32 ordinal loaders/marshallers for the existing VIOCALLS.dll,
   KBDCALLS.dll and SESMGR.dll.
4. Bounded copy-in/copy-out for VIO/KBD buffers and deep STARTDATA marshalling
   for DosStartSession.
5. Jar/common NLS state for DOSCALLS 289/291/395/396/397 and NLS.5/.6/.7.
6. DOSCALLS.425 DosFlatToSel token handling.
7. WHP-derived C/386 migration-helper validation/lowering for the frozen
   VIO/KBD descriptor surface. The known helper is replaced by a 32-bit
   hostcall plus RET imm16; no 16-bit guest code is executed.
8. R2 module/NLS/marshalling regressions plus inherited R1 tests.
9. Root `soft386-system-win32` target that builds DOSCALLS/VIOCALLS/KBDCALLS/
   SESMGR plus the Soft386 executable without changing those DLL sources.

## Explicitly unchanged

R2 does not modify:

* dlls/doscalls/*
* dlls/viocalls/*
* dlls/kbdcalls/*
* dlls/sesmgr/*
* their .def files or ordinals
* loader/os2host32.c
* the existing common VIO/KBD/SESMGR implementations

The root Makefile/README/layout documentation changes only expose the new
optional backend build target.

## Validation here

* GCC `make check`: PASS
* Clang `make check`: PASS
* inherited LE/LX/internal-fixup/hi-surface/thread/sync/memory tests: PASS
* multi-module import dispatcher fixture: PASS
* DOSCALLS NLS alias fixture: PASS
* direct NLS module fixture: PASS
* fake DOSCALLS native marshaller: PASS
* fake VIO/KBD/SESMGR marshaller, including packed C/386 dispatch: PASS

The exact C/386 helper scanner/lowering code is adapted from the already
live-proven WHP path. The final R2 live acceptance is to run `--check` and then
execute the user's real CMD32/NLSINFO binaries on Windows.

## First Windows targets

Prefer NLSINFO first: its NLS state and current VIO/KBD surface are covered.
Then CMD32 with a builtin (`-c "echo ..."`). CMD32 imports DosExecPgm and
DosWaitChild but external-command orchestration is intentionally not an R2
claim.

## Scheduler caveat

The native KBD DLL currently performs blocking host input. A wait-mode
KbdCharIn therefore blocks the sole host thread and all virtual Tiny386
contexts. This does not invalidate simple single-thread interactive testing,
but the next KBD milestone should use the common nonblocking KBD seam and
turn IO_WAIT into virtual-thread block/poll/wakeup.
