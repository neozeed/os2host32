# Soft386 R3 process vessels

## Goal

R3 makes `DOSCALLS.283 DosExecPgm` and `DOSCALLS.280 DosWaitChild` usable from
an OS/2 program running inside Tiny386.  The process boundary is deliberately
not another guest address space inside the same 64 MiB RAM image.

**One host `soft386_os2.exe` is one OS/2 process vessel.**

Each child started by DosExecPgm therefore gets:

* a fresh Tiny386 CPU object;
* private guest RAM and LE/LX mappings;
* a fresh PIB and per-thread TIB/FS state;
* its own jar-owned thread/memory/semaphore state;
* fresh native DOS/VIO/KBD/SESMGR DLL instances as required by that image.

This keeps identical OS/2 linear addresses valid in every process and avoids
inventing paging/CR3 address-space switching merely to support process
isolation.

## Why 280/283 are marshalled to DOSCALLS.dll

Process creation is a hybrid boundary.  OS/2 guest memory, threads and private
kernel objects remain inside each jar, but creating/waiting for a host process
is inherently host-facing.  The existing DOSCALLS Win32 backend already has a
mature process layer which:

* normalizes the OS/2 NUL-separated environment block;
* understands `EXEC_SYNC`, `EXEC_ASYNC`, `EXEC_ASYNCRESULT` and background;
* snapshots the authoritative OS/2 HFILE 0/1/2 state;
* launches with inheritable stdin/stdout/stderr;
* retains process handles for `DosWaitChild`;
* translates child exit status into OS/2 RESULTCODES.

R3 reuses that code by adding explicit guest-pointer marshalling for ordinals
280 and 283.  No guest pointer is passed to the native DLL.

## Re-entering Soft386 instead of OS2HOST32

At process-vessel startup R3 publishes the absolute current executable path as
`OS2HOST32_LOADER`.  The existing DOSCALLS process backend therefore constructs
children like:

```
soft386_os2.exe --run-quiet --argv0 <guest argv0> <program> <tail...>
```

R3 accepts the historical loader switches `--run`, `--run-quiet` and `--argv0`.
`--run-quiet` suppresses normal loader/import/scheduler diagnostics so child
stderr is not contaminated by compatibility-layer chatter.

The explicit native DLL paths supplied to the parent are canonicalized and
published as inherited `SOFT386_*_DLL` variables.  This matters after CMD32
changes its current directory: descendants still reopen exactly the same
DOSCALLS/VIO/KBD/SESMGR binaries rather than relying on CWD DLL search.

## Pipes do not share guest RAM

CMD32's pipeline implementation already uses:

```
DosCreatePipe
DosDupHandle
DosExecPgm(EXEC_ASYNCRESULT)
DosWaitChild
```

DOSCALLS.dll owns the HFILE namespace and its Win32 pipe handles.  When CMD
redirects HFILE 1 or 0, `DosDupHandle` also updates the authoritative Win32
standard handle.  `DosExecPgm` snapshots those exact handles into STARTUPINFO,
so the child Soft386 vessel inherits the pipe endpoint.

Conceptually:

```
Tiny386 A -- HFILE 1 -- DOSCALLS.dll -- Win32 pipe -- DOSCALLS.dll -- HFILE 0 -- Tiny386 B
```

No guest page is shared.  The pipe is a cross-process kernel object underneath
two independent jars.

A CMD pipeline is even stronger: CMD starts two asynchronous CMD32 worker jars;
each worker may synchronously start its external program in another jar.  Pipe
handles survive down that process tree through standard-handle inheritance.

## What R3 does not yet provide

There is no global `soft386srv` broker yet.  The Win32 process tree and each
parent DOSCALLS common session are sufficient for ordinary parent/child
execution and anonymous pipelines.

A future top-level broker becomes useful for OS/2 objects whose namespace must
span unrelated jars, notably:

* named semaphores;
* shared memory;
* named pipes/queues beyond ordinary inherited anonymous handles;
* system-wide PID/session enumeration and signals.

R3 intentionally does not move private memory, thread scheduling or private
sync objects out of the jar.
