WHP OS/2 V2 R8 - scheduling, mutexes, queues, thread recycling
=============================================================
Cumulative source release based on R7. Build the Windows host and the new
C/386 guests locally. No prebuilt R8 host or new guest EXEs are included.

QUICK START
  Windows x64 MSVC developer prompt: build-v2.cmd
  C/386 toolchain prompt:            build-c386-sync.cmd
  Copy the new guest EXEs beside the host if you built them elsewhere.
  Then:                             run-sync.cmd

Expected output, in order:
  sleep1 PASS
  mutex1 PASS
  queue1 PASS
  recycle1 PASS (1000 threads in one process)
  R8 scheduling and synchronisation suite PASS

The script runs each test once and stops on a nonzero exit. Each test has its
own <test>-trace.txt stderr log. No 50-run batch is needed for initial testing.
For optional longer stack/slot stress:
  whp_os2_v2_hi.exe recycle1.exe 10000 2>recycle1-trace.txt
recycle1 accepts 1..1000000; the default is 1000.

API COVERAGE
  DOSCALLS.229: DosSleep
  DOSCALLS.331..336: Create/Open/Close/Request/Release/QueryMutexSem
  QUECALLS.9:  DosReadQueue
  QUECALLS.10: DosPurgeQueue
  QUECALLS.11: DosCloseQueue
  QUECALLS.12: DosQueryQueue
  QUECALLS.13: DosPeekQueue
  QUECALLS.14: DosWriteQueue
  QUECALLS.15: DosOpenQueue
  QUECALLS.16: DosCreateQueue
  DOSCALLS.348: adds QSV_MS_COUNT (14), for elapsed-time measurement.

DOSCALLS and QUECALLS import veneers now have separate hypercall IDs. Queue
ordinals cannot accidentally resolve to DOSCALLS with the same ordinal.
Sarien's QUECALLS 9/14/15/16 and mutex calls now have implementations. Sarien
still imports PMWIN/PMGPI, so it remains blocked until the PM phase.

SCHEDULER
DosSleep(0) requests a round-robin yield; with no runnable peer it returns to
the same thread. Positive sleep registers a deadline and lets other guests
run. FFFFFFFF means indefinite sleep. Event, mutex and sleep deadlines are
checked at hypercall boundaries and while selecting the next runnable guest.
The host sleeps only when there is no runnable guest, until the nearest
finite deadline. This is still a cooperative single-VP scheduler: a thread
that never yields or blocks can starve others. No instruction preemption or
SMP execution is added. Synchronous console/file ReadFile can still block the
host; guest-aware blocking in this release concerns sleep/semaphores/queues.

MUTEXES
Ownership and recursion belong to guest TIDs, never to the Win64 host thread.
Zero-time request returns 121 when unavailable; finite and indefinite waits
block only the caller. A final release reserves ownership for the oldest
waiting guest before waking it, preventing the releaser from stealing it back.
Owner death discards the recursion count and the next owner gets 105
(ERROR_SEM_OWNER_DIED), with ownership granted so it can repair/release.
Nonowner release returns 288. Final close of an owned/waited-on mutex returns
301; nonfinal reference closes are allowed. Query returns PID/TID/count,
with zeroes for an unowned mutex. Named open/create is process-local and
case-insensitive; names are at most 127 bytes. Attribute bit 1 is accepted,
but does not create interprocess sharing. There are 64 mutex slots.
Handles carry type and generation; a closed/recreated object rejects old
handles. Generation exhaustion retires the slot instead of aliasing old IDs.
Mutex/event name spaces are separate in this implementation.

QUEUES
16 process-local named queues, 256 queued entries each, names at most 127
bytes and beginning with \QUEUES\. FIFO, LIFO and priority (15 highest) are
supported. Equal-priority entries remain FIFO. QUE_CONVERT_ADDRESS is accepted
for 32-bit writers, whose addresses require no conversion; no 16-bit queue
API is implemented. Queue entries store a request, byte count, opaque guest
address/value and priority. The host never dereferences or copies the data
payload: this is required for Sarien's tone values passed as pointer values.
The application must keep actual pointed-to buffers alive until consumed.

An empty blocking ReadQueue/PeekQueue saves guest output addresses and parks
the calling thread. A writer services waiting guests in arrival order, with
one consuming reader per element. A peek leaves the element in place. Peek
returns a token usable by ReadQueue; passing it back to Peek advances to the
next element. Tokens do not wrap/reuse within a queue lifetime. A missing
explicit token returns 333; end of enumeration returns 342. Blocking is
supported for the first element (token zero), not for a missing old token.

NOWAIT returns 342 for empty queues. Polling with hsem=0 is accepted. A nonzero
NOWAIT event handle is remembered (subsequent nonzero handles must match),
retained, and posted on writes; guest event waiters are awakened normally.
WAIT ignores hsem. Queue final close releases the event reference and wakes
blocked queue readers with 337. Open increments a process-local reference
count; only the final close destroys the queue. Purge removes entries without
freeing opaque payload memory and leaves blocked readers waiting for new data.
Full queues return 346 immediately; writes do not block. Output ranges are
validated before consuming an entry. No interprocess queue ownership,
shared-memory transfer or named DOSCALLS/QUECALLS imports are implemented.
These are documented V2 subset semantics, not a claim of full OS/2 fidelity.

THREAD LIFETIME
Both DosExit(EXIT_THREAD) and normal return share cleanup. Mutex ownership
is abandoned, thread waiters are awakened, and allocator-owned stacks are
marked free immediately after guest execution ends. TID 1's original EXE
stack is not freed as a heap allocation. A successful wait consumes the dead
slot; creation can also reuse an unjoined dead slot. New thread contexts and
TIB/TIB2/FS descriptors are rebuilt, including clearing old exception heads.
When the last live thread exits, the guest process ends successfully.

TIDs are monotonically issued and never reused or wrapped. For compatibility
with earlier V2 tests, waiting for an explicit, previously issued completed
TID succeeds even after its slot has been recycled. This uses no growing
completion-record allocation. Wait-any (TID zero) consumes an available dead
slot or waits for a future exit; it has no unlimited historical completion
queue once slots have been reused. If no other live or retained dead thread
exists, it returns 309. This intentionally preserves V2 late/repeated explicit
wait behavior; it is not exact native OS/2 thread-ID lifetime behavior.
32 simultaneous slots remain the limit; sequential creation is no longer
limited to 31 workers. Stack-size overflow is rejected before allocation.

OTHER FIX
Final event-semaphore close now returns 301 while a guest is waiting, instead
of silently destroying the object under a blocked thread. Event generations
are bounded to keep handle types disjoint from the new mutex/queue handles.

GUEST TESTS
sleep1: lone-thread yield, peer progress, positive sleep measured with
QSV_MS_COUNT, and finite event timeout.
mutex1: named duplicate/open/close, recursive ownership/query, wrong-owner
release, immediate/finite timeouts, reserved handoff, four contending workers
with yields inside critical sections (200 increments), owner death, stale
handle after recreation.
queue1: empty polling, named open/refclose, opaque values, peek/read token,
count/purge, event notification, three simultaneously blocked readers,
close while a reader waits, stale handle, FIFO/LIFO/stable priority ordering.
recycle1: 1000 sequential workers in ONE guest process, 64KiB stacks, yields,
stack canaries, monotonic IDs, both termination paths, late explicit wait.
Without recycling, the old 32-thread or allocation limits would fail this.

All four tests use the supplied Beta2 os2.h declarations. They do not need
the private GA information-block import used by R7 info1/infotort.

VALIDATION HERE
- Both new headers and production thread-creation, allocator, register
  save/load, scheduling, waiting, and veneer-emission helpers compiled and
  executed with clock/WHP-register test doubles under GCC -Wall -Wextra
  -Werror and AddressSanitizer/UndefinedBehaviorSanitizer.
- Sleep/yield/deadlines; mutex recursion, nonowner, timeout, abandonment with
  and without a blocked waiter; queue full/empty, priority/LIFO, tokens,
  references, stale handles, invalid output without consumption, multiple
  blocked readers, final-close wakeup and event notification passed.
- 5000 stack/slot reuse cycles, failed-register-read allocation rollback,
  oversized stack rejection, wait-any consumption, waiting-thread wakeup,
  last-thread process exit, and 2000 queue round trips passed.
- R7 checks also passed: 32 contexts and 32000 save/restores, private TIB
  markers, FS bases, common PIB, NULL output and invalid output validation.
- New guest sources passed GCC 32-bit C89 syntax/type checking against the
  supplied Beta2 headers (legacy keywords removed for GCC; existing SDK
  comment/pragma warnings suppressed). No Microsoft compiler/linker run here.
- Reproduce native checks from the package directory:
    ASAN_OPTIONS=detect_leaks=0 python3 tests/run-sync-check.py
  LeakSanitizer is disabled because the execution environment uses ptrace.
  tests/run-info-check.py now forwards to the combined R8 suite.
- Windows host compilation and real WHP execution remain for the user.
  Earlier validation logs are historical, not R8 Windows pass claims.

REFERENCE
Supplied c386 os2h/bsedos.h / bseerr.h: signatures, constants and error codes.
Supplied V1 quecalls.def/c and doscalls.c: Sarien subset and pointer semantics.
Queue ordinal cross-check (declarations only, no implementation copied):
https://raw.githubusercontent.com/icculus/2ine/main/native/quecalls.h

After run-sync passes, rerun info1, a single infotort run, and existing
semaphore/DLL/file/directory tests to check the cumulative release on Windows.
PM, audio, FPU context switching, priority scheduling, suspend/resume support
and preemption remain future work.
