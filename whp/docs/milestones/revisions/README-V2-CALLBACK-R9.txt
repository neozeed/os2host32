WHP OS/2 V2 R9 - host-to-guest callback foundation
================================================
Cumulative source package based on the user-verified R8 suite PASS.
No Windows host executable or new guest binaries are prebuilt here.

BUILD AND RUN
  build-v2.cmd                 (x64 MSVC developer prompt)
  build-c386-callback.cmd       (your C/386 environment)
  run-callback.cmd              (Windows, with guest binaries beside host)

Expected:
  callback1 PASS
  callbackdll PASS
  R9 callback suite PASS

Keep CBDLL.DLL beside the host/guests. Diagnostics go to callback1-trace.txt
and callbackdll-trace.txt. No long batch run is needed. After these pass,
run-sync.cmd and run-info1.cmd are useful regressions for scheduler/FS changes.

WHAT THIS IMPLEMENTS
The host can enter a flat 32-bit guest function on the current guest thread,
pass stack arguments and collect its 32-bit integer return value. Each thread
has its own linked stack of host continuation frames, limited to 16 levels.
Frames live on the host heap, avoiding a large per-thread fixed array and
avoiding recursive native run_guest loops.

begin_guest_callback saves the current integer/segment register context,
records the host API continuation RIP, reserves argument/return words below
the current guest ESP, and redirects the VP to the guest function. The guest
function runs in the ordinary execution loop, so its DOSCALLS, sleeps, event
waits and thread switches use the existing scheduler. Other threads may have
independent pending callbacks. Nested calls push another continuation frame.

The return sentinel at F7FE0 pushes EAX, loads a dedicated runtime trap ID,
and exits via OUT F0. The host checks the expected sentinel RIP and stack
position, retrieves the saved guest result, restores the caller's registers
and segments, writes the result, and resumes the suspended host API stub at
its RET. Restoring this continuation must bypass the usual hypercall RIP
advance, otherwise the callback entry or its caller would be skipped.

Both normal and DosExit thread termination discard all pending continuation
frames before recycling the stack/slot. Process cleanup also discards frames
for all threads, including an interrupted/faulted callback run.

TEST INTERFACE
WHPTEST.1 is a synthetic import supplied by the host, not an OS/2 DLL/API:
  ULONG cdecl HostInvoke(CALLBACK2 fn, ULONG a, ULONG b, ULONG *result)
  CALLBACK2: ULONG cdecl fn(ULONG a, ULONG b)
Return code 0 means the callback returned; *result holds its EAX value.
Invalid pointer/range/stack inputs return 87, depth/allocation exhaustion 8,
and register access failures 1. A thread exiting inside a callback never
returns to its HostInvoke caller. A malformed return sentinel stops execution
with a diagnostic rather than popping the wrong continuation.

The general internal entry helper supports 0..8 DWORD arguments; WHPTEST.1
exercises two. Stack arguments use caller cleanup. Callee-cleanup RET n,
floating-point/structure returns and 16-bit/far callbacks are not supported.
All saved integer registers, EFLAGS and CS/SS/DS/ES/FS/GS are restored, except
EAX, which carries the fixture API's return code. DF is cleared at guest C
callback entry and the original flags are restored on return. Guest memory
side effects, including TIB contents, are retained.

This release supplies same-thread asynchronous continuations within the host
execution loop. It does not yet connect a native Windows message pump,
implement PM/GPI, inject arbitrary callbacks into another guest thread, or
resume arbitrary native C stack frames. Future PM dispatch needs a completion
action appropriate to that API, instead of this fixture's result-pointer
completion. No host window procedures are called through 32-bit guest pointers.
Function pointers are range-checked, not checked against executable objects;
invalid code within RAM will produce the existing guest-fault diagnostic.
Only the existing integer/segment context is saved: FPU/SIMD remains deferred.

GUEST TESTS
callback1:
- zero/invalid arguments and exact 32-bit return values;
- deliberate EBX/ESI/EDI/EBP clobber and DF set inside a machine-code callback;
- depth 16 followed by graceful depth-limit error and complete unwinding;
- four worker threads each making 20 nested calls while yielding, plus main
  making a callback concurrently; FS/TIB2 identity and stack canaries checked;
- event-blocked main callback, resumed by a separate sleeping/posting thread;
- 40 worker exits from inside callbacks, exercising abandoned frame cleanup
  and slot/stack reuse; a subsequent ordinary callback must still succeed.
callbackdll:
- host -> guest DLL callback -> nested host -> guest EXE callback;
- DLL-side DosSleep(0), EXE-side DosSleep(1), return propagation and shared
  DLL call count; caller FS/TIB identity checked after each call.

CBDLL is CRT-free and uses explicit import/export .DEF entries like the
previous V2DLL fixture. Its address getter returns an internally relocated
function pointer, avoiding any new external OFF32 import requirement.
The test probes stored in byte arrays rely on the runtime's executable guest
RAM mapping and avoid an additional assembler dependency. These are WHP
fixtures; WHPTEST is intentionally not a native OS/2 API.

VALIDATION HERE
- GCC 32-bit C89 syntax/type checks against the supplied Beta2 SDK passed for
  callback1.c, callbackdll.c and cbdll.c. Legacy keywords are removed for GCC,
  existing SDK comment/pragma warnings suppressed. This is not a C/386 build.
- Production callback helpers compiled/executed with WHP-register/clock test
  doubles under AddressSanitizer and UndefinedBehaviorSanitizer. Checks cover
  stack layout, high-bit results, full register/segment restoration, nested
  calls, independently pending thread callbacks, blocked callback resumption,
  invalid/spoofed returns, depth limit, stack exhaustion and cancellation.
- The same native suite also passes R8 scheduling/mutex/queue tests and 5000
  recycled threads, plus R7's 32000 context save/restores.
  ASAN_OPTIONS=detect_leaks=0 python3 tests/run-sync-check.py
  LeakSanitizer is disabled for this ptrace-based execution environment.
- Real Windows WHP execution and Microsoft C/386 linking remain user tests.
  Earlier validation logs are historical. R8 live suite PASS was reported by
  the user before this release; no R9 Windows success is claimed here.
