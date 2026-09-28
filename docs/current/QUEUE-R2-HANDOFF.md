# OS2HOST32 QUECALLS R2 handoff

Date: 2026-09-27

Status: **QUEUE_R2_COMMON_STATE_WIN32_BACKEND_STATIC_VERIFIED_RUNTIME_PENDING**

Baseline package:

- `os2host32-DOSCALLS-R2B.zip`
- SHA-256 `bcd9fe7e85910e1bebbd431a13fd84b394f82e30bc8bce703d42bfb068a5150f`
- DOSCALLS/VIO R2B was live-tested successfully by the user before this QUECALLS work.

## Objective

Split the existing native Win32 `QUECALLS.DLL` along the same personality/backend
boundary already used for VIO and DOSCALLS, without expanding the exported queue
surface.

The four historical exports remain frozen:

| Ordinal | Export |
|---:|---|
| 9 | `DosReadQueue` |
| 14 | `DosWriteQueue` |
| 15 | `DosOpenQueue` |
| 16 | `DosCreateQueue` |

`dlls/quecalls/quecalls.def` is byte-for-byte unchanged from the R2B baseline,
SHA-256 `edccc8b6d80c1076dfdf637230f3575d3ed8a98791ad5617933001ebfafcbd75`.

## Resulting architecture

```text
OS/2 application
      |
      v
QUECALLS.DLL ABI veneer
  dlls/quecalls/quecalls.c
      |
      v
backend-neutral queue personality
  common/queue/os2_queue.c
  common/include/os2_queue.h
      |
      v
backend operations
  common/include/os2_queue_backend.h
      |
      +---- native Win32 backend
            common/win32/os2_queue_win32.c
```

The exported DLL source is now a thin adapter.  It contains no Win32 handles,
critical sections, events, queue arrays, or queue ordering implementation.

## Common-owned OS/2 state and semantics

`Os2QueueSession` / `Os2QueueObject` now own:

- queue handle namespace;
- case-insensitive queue names;
- creating/owner PID;
- queue discipline;
- queued entry metadata;
- FIFO ordering;
- LIFO ordering;
- priority ordering, priority 15 highest;
- stable FIFO order among equal-priority entries;
- queue depth/count;
- validation of `\\QUEUES\\...` names;
- validation of queue-order flags;
- validation of element priorities 0..15;
- owner-only `DosReadQueue` semantics;
- NOWAIT empty semantics;
- opaque 32-bit queue data address/value semantics.

`QUE_CONVERT_ADDRESS` is accepted for the native 32-bit personality.  The common
layer never dereferences or copies the data payload.

This is important for the existing Sarien sound path, where values such as 440 Hz
are intentionally passed through the queue data-address field as pointer values.

## Win32 backend responsibilities

The native backend now supplies only execution mechanics:

- a `CRITICAL_SECTION` protecting the common queue session;
- `GetCurrentProcessId` for the caller identity;
- one manual-reset Win32 event per queue;
- Set/ResetEvent availability notification;
- WaitForSingleObject for native blocking reads.

The common queue count remains authoritative.  The Win32 event is only a wakeup
indicator; it is not a mirrored element count.

## WHP / OS2SS seam

`os2_queue_try_read()` is deliberately exposed as a nonblocking common semantic
primitive.  It lets an execution engine with its own scheduler avoid inheriting
the native blocking model:

- WHP can attempt the common read, park a guest thread on `ERROR_QUE_EMPTY`, and
  wake it through the guest scheduler when a write arrives.
- OS2SS can perform the same common operation while using its own server/client
  scheduling and LPC request lifetime.

The queue payload is already represented as a 32-bit OS/2 value in common code,
so neither backend needs to reinterpret a guest/client address as a host pointer.

No WHP or ReactOS/OS2SS source was modified in this milestone.

## Existing behavior intentionally retained

The native implementation remains process-local, as before.  The four supported
exports are unchanged.  The `LE2PE_TRACE_AUDIO` QUECALLS diagnostics remain
available from the ABI veneer.

`DosReadQueue`'s `hev` argument remains ignored by the native four-call subset,
matching the previous Win32 implementation.  Notification-semaphore behavior can
be added when Peek/Query/Close/Purge and wider queue semantics are promoted; it
was not mixed into this architectural split.

## Deliberately out of scope

No new exports were added.  In particular this milestone does **not** add:

- `DosCloseQueue`;
- `DosPeekQueue`;
- `DosPurgeQueue`;
- `DosQueryQueue`;
- interprocess native Win32 queue sharing;
- OS2SS backend code;
- WHP backend conversion;
- 16-bit queue execution.

## Verification

Host-portable tests added:

- `tests/queue/queue-core-check.c`
- `tests/queue/queue-veneer-check.c`
- `tests/queue/queue-win32-shim-check.c`
- `tools/check_queue_r2.py`

The tests cover:

- frozen ordinals;
- case-insensitive names;
- queue-name validation;
- duplicate detection;
- owner PID returned by OpenQueue;
- owner-only reads;
- FIFO;
- LIFO;
- priority ordering;
- FIFO ordering among equal priorities;
- priority range validation;
- `QUE_CONVERT_ADDRESS` acceptance;
- NOWAIT empty behavior;
- invalid element/wait behavior;
- opaque `0xDEADBEEF` payload preservation;
- the Sarien/queuebeep pointer-as-value `440` case through the actual veneer;
- Win32 event backend wiring;
- strict C89 compilation.

Results:

- GCC 14 strict C89 queue tests: PASS
- Clang 17 strict C89 queue tests: PASS
- full existing `make verify`: PASS
- DOSCALLS R2 checks: PASS
- VIO R2B checks: PASS
- QUECALLS R2 static architecture check: PASS
- actual MinGW QUECALLS build: **not run in this environment**
- native runtime Sarien/queuebeep regression: **pending user test**

Full transcript:

`regression-evidence/queue-r2/STATIC-VERIFICATION-QUEUE.txt`

## Native build / live regression

On the normal Win32 build host:

```text
make clean
make QUECALLS.dll
make all
make verify
```

Then run the existing queue users, especially:

1. the historical `queuebeep-test` path if its built specimen is available;
2. Sarien / the sound path that uses QUECALLS;
3. the normal demo set used to validate DOSCALLS/VIO R2B.

If those pass, promote the milestone to:

`QUEUE_R2_COMMON_STATE_WIN32_BACKEND_LIVE_PASS`

## Recommended next QUECALLS work

Do not add another architectural layer.  The next queue work should be semantic
surface expansion on this foundation, probably in this order:

1. `DosCloseQueue`
2. `DosQueryQueue`
3. `DosPurgeQueue`
4. `DosPeekQueue`
5. NOWAIT event/semaphore notification semantics
6. WHP adapter to the common state/ordering primitives
7. OS2SS backend

Close/Peek are the important pieces before attempting true cross-process queue
lifetime and notification semantics.
