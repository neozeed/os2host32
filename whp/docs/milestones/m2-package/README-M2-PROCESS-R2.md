# WHP OS/2 V2 — M2 process R2

Cumulative source release following M2 CMD R1. Milestone1 and R1 remain separate checkpoints.

## Build and try

Extract into a fresh directory. From your x64 Visual Studio command prompt:

    build-v2.cmd

From your configured Microsoft C/386 / LINK386 environment:

    build-c386-process.cmd

Back in Windows, from the release directory:

    run-process.cmd
    run-cmd32.cmd -c "hi.exe"
    run-cmd32.cmd -c "execchild.exe 37"
    run-cmd32.cmd

Inside the interactive guest command prompt, try:

    execchild.exe 37
    echo %ERRORLEVEL%
    execchild.exe capture > child-output.txt
    type child-output.txt

Expected process test: `process1 PASS (sync, arguments/environment, async wait, reap, stdout)`.
The C/386 child and test EXEs must be built locally; only the existing CMD32 and hello binaries are supplied.
`process1-output.tmp` is created exclusively and removed; an existing file of that name makes the test fail without overwriting it.
`process-trace.txt` and `cmd32-trace.txt` contain host diagnostics. Guest redirected output should contain no host trace.

## Implementation

DOSCALLS.283 DosExecPgm supports EXEC_SYNC (0) and EXEC_ASYNCRESULT (2).
Each guest child gets an independent instance of this WHP host, address space, and virtual processor.
The host executable is launched directly, without cmd.exe. A private inherited mapping transports the original guest argument and environment blocks, with bounded validation.
The current directory and guest standard handles 0/1/2 are inherited at launch.
The child host keeps its diagnostic stream separate from guest stderr and stdout.
Guest PIB PID/PPID report the corresponding host process IDs.

DosExecPgm waits cooperatively for startup acknowledgment. Synchronous execution then waits cooperatively for completion, allowing other guest threads to run.
Async-result returns the child PID in RESULTCODES.codeTerminate and zero in codeResult.
DOSCALLS.280 DosWaitChild supports DCWA_PROCESS, WAIT/NOWAIT, a particular PID or PID zero for any direct child.
Completion returns termination reason and exit code, optionally the collected PID; results are collected once.
NOWAIT returns 129 while a matching child is running; no eligible child returns 128.
The helper checks process signaling before reading exit status, so guest exit code 259 is valid.
Failure before guest entry is an execution error; host/runtime failure after entry is reported as abnormal termination.
There are 32 tracked child slots, reused after collection. Closing parent tracking handles does not kill asynchronous children.

## Scope and limitations

This is an initial process implementation, not full OS/2 process/session emulation.
EXEC_ASYNC, background/trace/load modes, process-tree waits, kill/signal support, SESMGR START, cross-process shared memory, and inheritance of nonstandard guest file handles are not implemented.
Programs must be OS/2 LE/LX images supported by this loader and runtime; this does not launch arbitrary Win32 or DOS programs.
Pipelines are not yet certified end-to-end. Standard-handle inheritance provides the plumbing, but full CMD pipeline behavior still needs live testing.
Paths use existing ANSI/MAX_PATH host handling. Executable search remains the caller's responsibility.

## Evidence

User validated R1 `echo WHP_CMD_OK` and reported the console tests working. Supplied R1 traces are preserved under validation.
R2 Linux checks passed: production process helpers with mocked Win32 handles/processes, address/undefined sanitizers, cooperative sync wait, async startup/PID, wait/nowait, exactly-once collection, argument/environment packet transfer, exit 259, pre-entry failure and invalid pointers.
Existing scheduler, console, PM/GPI and loader checks also passed in the native harness.
The two new C/386 test sources passed GNU89 32-bit syntax checking against the supplied historical OS/2 SDK headers, with warnings treated as errors (SDK comment/pragma warnings suppressed).
Windows validation is now user-confirmed: process1 passed, CMD32 launched hi.exe and execchild.exe, and interactive CMD32 launched phoon after changing directories and returned to the prompt. See validation/m2-process-windows-results.txt. The reported guest_err warnings are fixed in this revision; the user subsequently confirmed a clean rebuild and successful hello, execchild and interactive phoon runs.

See v2_process.h, v2_process_boot.h and tests/process-check.inc for implementation and harness checks.
