# QUECALLS backend design

## Purpose

QUECALLS is split so OS/2 queue semantics are not synonymous with Win32 kernel
objects.  The common layer owns the queue as an OS/2 personality object; a backend
only provides mechanics required by the execution environment.

## Ownership boundary

### Common queue layer

`common/queue/os2_queue.c` owns:

- queue identity/name;
- OS/2 queue handles;
- owner process ID;
- ordering discipline;
- queue entries and count;
- request code;
- byte count;
- opaque 32-bit data address/value;
- element priority;
- writer PID;
- validation and OS/2 return codes.

### Backend

`Os2QueueBackendOps` supplies only:

```text
lock
unlock
current_pid
create_availability
destroy_availability
set_available
wait_available
```

No Win32 type is exposed in the common headers.

## Why availability is an event, not mirrored queue state

The common `queue->count` is authoritative.  A backend availability object is only
used to wake a native caller that must sleep.

For Win32 this is a manual-reset event:

```text
queue transitions 0 -> 1 entries : SetEvent
queue transitions 1 -> 0 entries : ResetEvent
```

This deliberately avoids keeping a second count in a Win32 semaphore.  Multiple
waiting readers may wake from the event; they re-check common state under the
session lock.  Only readers that find an element consume one.

That design also maps more cleanly to execution engines which do not block a host
thread for each OS/2 thread.

## Scheduler-facing primitive

`os2_queue_try_read()` never blocks.  It performs the same common handle,
ownership, element, and dequeue semantics and returns `ERROR_QUE_EMPTY` if no
entry exists.

A WHP backend can therefore use:

```text
try_read
    -> element available: complete guest API
    -> ERROR_QUE_EMPTY + WAIT: save guest call state and park guest thread
    -> later DosWriteQueue: wake appropriate guest queue waiter
```

An OS2SS server can use an analogous request/deferred-reply model.

The existing native exported `DosReadQueue` uses the higher-level common wrapper,
which waits through `wait_available()` and retries the common state.

## Opaque data-address rule

Queue data is represented in common code as:

```text
uint32_t data_value
```

It is not a C pointer.

For native 32-bit OS2HOST32 the ABI veneer performs:

```text
void * -> uintptr_t -> uint32_t
uint32_t -> uintptr_t -> void *
```

For WHP it can remain an untouched guest linear address/value.  For OS2SS it can
remain an address/value in the originating client address space.

The common implementation never dereferences or copies this value.

## Ordering

The common layer implements the three historical disciplines:

- `QUE_FIFO` (0): append;
- `QUE_LIFO` (1): insert at the front;
- `QUE_PRIORITY` (2): higher numeric priority first, with equal-priority FIFO.

`QUE_CONVERT_ADDRESS` (4) is accepted.  In a 32-bit-only personality no 16:16
address conversion is required.

## Current native lifetime model

Only the four exports already present in OS2HOST32 are implemented.  Because
`DosCloseQueue` is not yet exported, a queue lives for the lifetime of the native
QUECALLS process instance, as it did before the split.

Do not invent reference-count or destruction semantics in a backend before
`DosCloseQueue` is implemented in common code.

## Future notification semantics

The existing native implementation ignored `DosReadQueue`'s `hev` argument and
this split preserves that behavior.  WHP already contains richer queue-event
experiments, but those semantics should be promoted deliberately into the common
contract together with Peek/Close/Query/Purge rather than being smuggled into the
four-call refactor.
