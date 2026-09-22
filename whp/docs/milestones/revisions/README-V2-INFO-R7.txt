WHP OS/2 V2 R7 - thread/process information
==========================================
Cumulative source release based on the user-verified R6 find1 PASS.
New guest binaries must be built with your C/386 toolchain; this package does
not contain prebuilt info1.exe / infotort.exe or a Windows host executable.

BUILD
  Windows x64 MSVC developer prompt: build-v2.cmd
  C/386 environment:                build-c386-info.cmd
  C386LIB defaults to \c386\lib, as in the earlier test scripts.
  Copy the two guest EXEs back beside whp_os2_v2_hi.exe if built elsewhere.

RUN
  run-info1.cmd
    Expected: info1 PASS (1 workers, 3 rounds)
    Diagnostics: info1-trace.txt

  run-infotort.cmd
    Runs 10 fresh guest processes; each has 10 workers, 100 handshake rounds.
    Stops immediately on failure. Diagnostics: infotort-trace.txt, overwritten
    each run, retaining the failed run (or the final successful run).

  run-infotort.cmd 50 30 1000
    50 process runs, 30 workers per process, 1000 rounds per worker.
    Arguments: [runs=10] [workers=10] [rounds=100].
    Workers: 1..30; runs and rounds: 1..1000000.
    Large counts generate substantial stderr traces and may run a long time.

  whp_os2_v2_hi.exe infotort.exe 30 1000 2>infotort-trace.txt
    Direct single-process run: [workers=10] [rounds=100].

IMPLEMENTATION
- DOSCALLS.312 DosGetInfoBlocks returns guest pointers to a private TIB/TIB2
  and the process-shared PIB. NULL omits either output (both may be NULL).
  Out-of-RAM output pointers return 87, validating both before writing either.
  This is address-range checking, not a general guest memory protection layer.
- TIB +0: guest-owned exception-chain head, initialized to FFFFFFFF.
  +4: stack low address; +8: stack high address (exclusive).
  +12: TIB2 pointer; +16: version 20; +20: thread ordinal (currently TID).
  TIB2: TID, priority 0200 (regular class, delta zero), version 20,
  16-bit must-complete/force counters initially zero.
- Main stack uses the existing loader's stack-object base through initial
  stack top, matching the early OS/2 convention for LINK386 executables.
  Child stack bounds cover the actual page-rounded allocation.
- Each thread receives a distinct GDT data selector and FS base. Both the
  saved WHP segment state and a real GDT entry are initialized. Reloading FS
  from its selector works. Existing context save/restore already includes FS.
- PIB: virtual PID 1, parent 0 (no emulated parent), EXE module handle 1,
  pointers to the existing argv0-NUL-tail-NUL-NUL and environment blocks,
  initial status 0, process type derived from the executable windowing flags
  (fullscreen 0, window-compatible 2, Presentation Manager 3).
- Info area E20000..E22FFF is reserved after the startup strings. Main-image
  overlap checking also covers the enlarged GDT and new information area.
- Queries do not reconstruct or erase information blocks. Guest changes to
  the exception-chain head and PIB survive API calls and context switches.

BETA2 ABI DIFFERENCE
Your bsetib.h defines the old flat TIB (TID at offset 0), and bsedos.h declares
DosGetThreadInfo. That is NOT the later TIB/TIB2 layout used here for ordinal
312. info_test.h deliberately uses separately named GA structures and a
private cdecl InfoBlocks declaration. Each test's .DEF explicitly binds
_InfoBlocks=DOSCALLS.312; this bypasses legacy import-library name ambiguity.
It uses the already supported caller-cleanup convention. All other APIs use
os2.h declarations. No user SDK files are modified. No claim is made that
the Beta2 DosGetThreadInfo structure ABI is implemented by this release.

TEST COVERAGE
info1 is a small one-worker regression. infotort runs the same assertions with
configurable concurrent workers and rounds. Workers have individual gate
semaphores; main acknowledges each step through a shared acknowledgment event.
Workers block and resume repeatedly, retaining a private stack-resident
exception-chain marker. Every iteration checks:
  stable TIB and common PIB pointers; correct TID and stack bounds;
  distinct worker TIB/TIB2 pointers and non-overlapping worker stacks;
  FS reads matching TIB fields after reloading the selector;
  private exception-chain markers and main-thread identity surviving switches;
  exact completion counts, event reset counts, successful waits and closes.
Even workers use DosExit(EXIT_THREAD); odd workers return normally.
The initial regression also checks structure sizes, NULL outputs, invalid
outputs without partial writes, and command-tail arguments when supplied.
Worker code uses no CRT thread-local facilities on successful paths.

The FS probe is a 12-byte leaf x86 routine stored in a byte array, accessed
through a pointer union: no assembler or inline-assembler dialect dependency.
It reloads FS and reads FS:[offset], touches EAX/ECX only, and returns normally.
Its disassembly is in validation/fs-probe.txt. It relies on this runtime's
executable guest-data mapping; these tests are intended for this WHP runtime,
not a hardened native OS/2 process with non-executable data.

LIMITATIONS
One virtual processor, cooperative scheduling. These tests check interleaved
thread state and blocking/wakeup correctness, not parallel SMP races or
preemption. Exception dispatch, priority scheduling/mutation, must-complete
APIs, TLS allocation, process creation, PM and queues remain unimplemented.
The existing scheduler still retains dead threads/stacks until process exit;
this release does not add thread-slot recycling. Repetition uses fresh host
processes; per-process rounds reuse the same live workers.
Each event wait has a 10-second timeout. There is no host watchdog for a guest
that never reaches an API; interrupt a truly hung run manually.

VALIDATION HERE
- GCC 32-bit C89 syntax/type check of both guests against the supplied Beta2
  SDK; legacy keywords removed for GCC, old SDK comment/pragma warnings
  suppressed. This is NOT a Microsoft C/386 compiler/linker run.
- Production v2_info.h plus extracted production thread-creation and context
  save/load functions exercised with native WHP register test doubles under
  AddressSanitizer/UndefinedBehaviorSanitizer. 32 thread contexts, 32000
  save/restores, full slot capacity, FS descriptors, shared PIB/private TIB,
  NULL outputs and invalid-pointer no-partial-write checks passed.
  LeakSanitizer disabled because this execution environment uses ptrace.
  Reproduce: ASAN_OPTIONS=detect_leaks=0 python3 tests/run-info-check.py
- x86 FS probe independently disassembled with objdump.
- No Windows host compilation or live WHP execution was available here.
  Older validation logs in this cumulative package describe earlier releases.

REFERENCE NOTES
SDK supplied by user: os2h/bsetib.h and os2h/bsedos.h (Beta2 ABI).
Supplied V1 doscalls.c: ordinal 312 and GA compatibility layouts.
IBM Toolkit documentation mirror, optional NULL outputs:
https://www.os2.kr/komh/os2books/os2tk45/cp1/620_L2H_DosGetInfoBlocksPara.html
https://www.os2.kr/komh/os2books/os2tk45/cp1/621_L2H_DosGetInfoBlocksPara.html

Regression suggestion after the new tests: rerun your existing thread/event,
DLL/thread, fileio1 and find1 tests, because the initial FS segment changed.
