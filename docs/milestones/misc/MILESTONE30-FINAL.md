# Milestone 30 FINAL — frozen V1 checkpoint

**Frozen:** 2026-09-18

This directory is the archival end of the Milestone 30 EMX investigation and is
intended to be the source baseline for **Milestone 31: Presentation Manager SDK
examples**.

Do not read "final" as "complete EMX compatibility".  It means the V1/native
Win32 architecture has been pushed far enough to establish what works, what is
worth keeping, and what should be deferred to the future virtualized execution
architecture.

## What is proven at this checkpoint

The V1 `os2host32` path can load and directly execute flat 32-bit OS/2 LE/LX
programs and has a substantial OS/2 personality implemented through Win32
compatibility DLLs.

The pre-M30 foundation includes, among other things:

* LE/LX parsing, object mapping, imports and fixups;
* host-backed DOSCALLS, VIOCALLS, KBDCALLS, QUECALLS, SESMGR and NLS surfaces;
* real guest OS/2 DLL loading, dependency handling, runtime module handles,
  initialization and termination;
* the reconstructed C/386 CMD path, file/console/batch/process support;
* native far16 VIO/KBD migration-thunk bridging used by classic C/386 output;
* the existing PMWIN and PMGPI compatibility DLLs.

Milestone 30 proved a significant subset of EMX 0.9x runtime behavior:

* dynamic loading and initialization of the historical `emx.dll`;
* the EMX 32->16 / 16->32 transition path and the native EMX_THUNK1 dispatcher;
* TIB/PIB process information sufficient for real C runtime startup;
* OS/2 PIB command-block semantics (`argv0\0argument-tail\0\0`);
* exception/signal-registration bookkeeping;
* NLS startup calls;
* relative-max-file-handle support;
* OS/2 suballocation APIs;
* `DosDevIOCtl` startup/device probing;
* console and file I/O through the existing DOSCALLS personality;
* sidecarless experimental recovery of stripped EMX a.out absolute relocations
  from the bound LX executable.

A small GCC/EMX program has run sidecarless through C startup, stdio and clean
termination.  A real Infocom interpreter has progressed through EMX startup,
argument parsing and normal application-level file handling.  With a genuine
Planetfall story it successfully opened the file, sought it and read the first
64-byte Z-machine header before reaching the current experimental relocation /
runtime boundary.

## EMX status: useful proof, intentionally frozen

Bound EMX executables contain an embedded i386 a.out image but emxbind strips
its original text/data relocation records.  M30M introduced a bound-EXE-only
recovery heuristic.  It is good enough to run simple programs and to take a
non-trivial Infocom executable into real application work, but it is not a
claim of general correctness.

`hi.aout-oracle` is retained only as a development oracle.  The sidecarless
runtime path does not require it.  It was valuable because its genuine a.out
relocation table lets us measure inferred-site false positives/negatives.

M30M3 experimented with preserving only the small historical a.out DATA/BSS
island at 0x00020000.  On the tested Windows environment that island was not
actually available, so normal relocated DATA remained in use.  M30M4 therefore
adds diagnostic reporting rather than treating the fixed island as a solved
mapping policy.

The M30M4 source is retained in this final tree because its diagnostics are
non-invasive and useful if this work is revisited.  They are not part of the
M31/PM agenda.

## The abandoned low-address branch

M30K through M30L explored whether historical EMX preferred addresses could be
preserved on modern 32-bit Win32 under WOW64.  Important findings:

* the Win32 host image base was moved experimentally to get it out of the EMX
  arena;
* 0x00010000 is already occupied before ordinary host code can claim it;
* the running 32-bit process showed an anonymous 64-KB `MEM_MAPPED` region at
  0x00010000;
* moving the Win32 environment / PEB data does not release that mapping;
* the environment/process-parameter allocation is separate and much higher;
* suspended-child pre-reservation is already too late;
* other native process allocations also intersect the large historical EMX
  arena, so preserving the whole 1990s layout is not a durable architecture.

The experiments are documented in `RESEARCH-M30K-M30L.md` and the individual
README-M30K* / README-M30L* files.  They should be treated as research history,
not as the active loader design.

## Architectural conclusion

V1 remains the **native Win32 compatibility implementation** and is the right
place to continue Presentation Manager, GPI and ordinary 32-bit OS/2 API work.

For truly general mixed 16/32-bit OS/2 execution, historical address-space
assumptions and difficult EMX/16-bit cases, the cleaner long-term direction is
V2: a lightweight Hyper-V / Windows Hypervisor Platform guest execution layer,
conceptually closer to the WSL2 split.

A separate WHP proof-of-concept has already established the essential plumbing:
a Win64 host can create a WHP partition, run bare 32-bit protected-mode x86,
intercept an OUT-based host-API call, pass a 32-bit guest pointer to the host,
return a value to the guest, resume execution and validate guest memory before
HLT.  That V2 POC is intentionally not merged into this V1 tree.  When V2 is
resumed, the next useful experiments are 16-bit protected-mode execution and
32<->16<->32 transitions.

The goal is to reuse the V1 personalities and semantics rather than throw them
away: DOSCALLS, PMWIN, PMGPI, VIO/KBD, SESMGR, loader knowledge and the API
bridges remain valuable regardless of where guest x86 executes.

## Do not do at the start of M31

Unless a PM sample specifically exposes one of these issues, do **not** begin
M31 by:

* trying to make EMX generally correct;
* reviving M30K low-address/PEB experiments;
* removing the WOW64 low mapping;
* rebuilding the runtime around the WHP POC;
* broadening inferred a.out relocation heuristics.

Those are separate future tasks.  The next milestone should exercise PM.

## Build baseline

On the Windows MinGW/i686 build environment:

    make clean
    make tools compat

The compatibility target builds:

    DOSCALLS.dll
    KBDCALLS.dll
    VIOCALLS.dll
    QUECALLS.dll
    SESMGR.dll
    NLS.dll
    PMWIN.dll
    PMGPI.dll

The host executable remains a 32-bit Win32 process.  The Makefile intentionally
links `os2host32.exe` at 0x00400000 in this frozen V1 line; the experimental
M30K host-base changes are not the baseline.

## Historical detail worth retaining

The old EMX DLL used during M30 can emit:

    WARNING: emx 0.9d or later required

That warning did not prevent the known-good test programs from running.  It is
likely version skew between the supplied historical DLL and the toolchain /
emxbind revision rather than the principal V1 failure.

See `START-HERE-M31-PM.md` for the next conversation.
