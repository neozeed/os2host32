# Next-chat handoff — WHP OS/2 V2 Milestone 2

## Resume instruction

This is the known-working checkpoint for Jason Stevens / neozeed's WHP OS/2 runtime. Read MILESTONE2.md, this file, README-M2-PROCESS-R2.md, and README-M2-CMD-R1.md. Inspect the source before changing behavior. Preserve this baseline and make subsequent changes in a new release directory. Do not restart from the older os2host32 native host; it is a reference implementation, not the current execution backend.

The user asked to wrap up and freeze this pass, not implement another feature. Future phase selection is still open. Suggested next work is a focused CMD process/pipeline regression pass, then SESMGR/START and PM launch integration. Do not interpret suggestions as already implemented or tested.

## Project goal and execution model

Run OS/2 binaries on modern 64-bit Windows through a Win64 Windows Hypervisor Platform host. Guest x86 instructions run inside WHP. OS/2 APIs are serviced in the host using synthetic imports: MOV EAX, encoded API / OUT 0xF0 / RET. This is an OS/2 personality, not a booted OS/2 kernel.

Main source: whp_os2_v2_hi.c. One virtual processor per guest process; cooperative guest thread scheduler. Child processes run independent instances of the same Win64 host. Guest RAM is 64 MiB with flat protected-mode addressing; no guest paging. Guest pointers must always be range-checked and translated through guest RAM, never cast directly to host pointers.

Milestone1 includes LE/LX loading, guest DLLs, thread/process information, file I/O and directory search, event/mutex/queue waits, thread-slot/stack recycling, host-to-guest callbacks, PM windows/input/painting, GPI bitmaps and Sarien support. Read the historical release notes for their precise scope; it is not complete OS/2 compatibility.

## Milestone2 source map

- v2_far16.h: recognizes the C/386 migration-thunk patterns in the supplied CMD32. Installs guest veneers for seven VIO/KBD imports. This is not general 16-bit execution. The original executable file is never rewritten.
- v2_console.h: keyboard, cursor, TTY, mode/scroll support; guest-aware WAIT_KBD and console mode restoration.
- v2_cmdfs.h: CMD filesystem/current-directory/standard-handle duplication/pipe/app-type subset.
- v2_process_boot.h: private parent-to-child shared mapping, bounded startup packet validation, guest arguments/environment transfer, diagnostic/guest-stderr separation. The final warning fix initializes guest_err=NULL and guards the final SetStdHandle with that saved handle.
- v2_process.h: DosExecPgm (DOSCALLS.283), DosWaitChild (.280), child tracking, completion polling and collection.
- whp_os2_v2_hi.c: dispatch and scheduler integration; THREAD_WAIT_PROCESS; child startup switch --whp-child; PID/PPID and child completion publication.
- cmd32-source/: recovered CMD32 source and bridge probe. The supplied cmd32os2_os2.exe is the binary exercised by the user.
- process1.c / execchild.c: C/386 guest process regression fixtures.
- tests/process-check.inc and tests/process-mock.h: native process behavior checks with mocked Windows process APIs.

## Process semantics and limits

EXEC_SYNC=0 waits cooperatively; EXEC_ASYNCRESULT=2 returns a waitable PID after child startup acknowledgment. RESULTCODES for async launch carries PID in codeTerminate and zero codeResult. DosWaitChild supports DCWA_PROCESS, WAIT/NOWAIT, PID-specific or any direct child (PID zero). It returns 129 when a matching child is still running and NOWAIT was requested, 128 when no eligible child exists. Completed results are collected once. A signaled process is checked before reading its exit code, preserving legitimate exit code 259.

Up to 32 tracked children; slots recycle on collection. Async children are not killed when their parent closes tracking handles. Program paths, the original argument block, environment block, current directory and standard handles 0/1/2 are conveyed to the child host. CRT diagnostics use a separate inherited handle so guest redirection stays clean. Host PID and parent PID are exposed in the guest PIB.

No SESMGR START implementation yet. No general EXEC_ASYNC/background/trace/load modes, process-tree waits, signals/kill, cross-process shared memory or nonstandard guest-file-handle inheritance. Pipes have plumbing, but full CMD pipelines have not been certified on this WHP backend. The CMD welcome banner advertises historical native-host features and still mentions OS2HOST32; it does not establish WHP feature coverage.

## ABI/toolchain rules

Guest compiler is Microsoft C/386 6.00.081 with old OS/2 includes/libs. Use INCL_DOSPROCESS / INCL_DOSFILEMGR and official os2.h declarations; don't hand-declare Dos APIs (wrong decoration/calling convention previously caused unresolved symbols). M_I386 is predefined by the real compiler. Keep tests C89-compatible. Link through the user's LINK386 shim (run286), libc.lib and os2386.lib; C386LIB defaults to \c386\lib. Both old LE and newer LX linkers have been used successfully.

Host uses modern x64 MSVC and Windows SDK, linking WinHvPlatform, User32 and Gdi32. It does not build with the guest compiler. This workspace's Linux native harness is not a full MSVC or WHP execution substitute.

## Compatibility details worth preserving

CMD uses GA directory-find layout via run-cmd32.cmd (`WHP_OS2_FIND_LAYOUT=GA`). Sarien uses the beta compatibility path and launcher-specific options; do not globally switch structure layout to fix one program.

The seven recognized migration imports are KBDCALLS.4 KbdCharIn, .13 KbdFlushBuffer; VIOCALLS.7 VioScrollUp, .9 VioGetCurPos, .15 VioSetCurPos, .19 VioWrtTTY, .21 VioGetMode. DOSCALLS.425 is the recognized C/386 pointer-token helper with register ABI. Don't replace its behavior with an ordinary stack-argument API stub.

Console cleanup restores cooked mode before child launch so child DosRead can work. Child CRT stderr descriptor replacement must preserve a distinct guest handle 2, including restoring the Win32 standard handle after _dup2. The warning-fixed code was built cleanly on Windows and rechecked with phoon.

## Verification and known evidence

MILESTONE2.md records exactly what Jason validated. Full Windows build/testing remains on his machine; don't claim local WHP tests. Native harness checks cover process packet transfer, cooperative waiting, PID/wait/nowait, single collection, exit259, startup failure and bounds, plus console, callbacks, scheduler, PM and GPI helpers. Loader checks exercise supplied CMD32/hello LE/LX and reject a corrupted recognized thunk. These are unit/harness checks, not full-system certification.

Useful Linux commands from the extracted release root:

    ASAN_OPTIONS=detect_leaks=0 python3 tests/run-sync-check.py
    ASAN_OPTIONS=detect_leaks=0 python3 tests/run-loader-check.py cmd32os2_os2.exe hi.exe hi-lx.exe

Windows commands:

    build-v2.cmd
    build-c386-process.cmd
    run-process.cmd
    run-cmd32.cmd -c "hi.exe"
    run-cmd32.cmd -c "execchild.exe 37"
    run-cmd32.cmd

Use the appropriate host/guest compiler environment for each build command. To explicitly validate shell status next time, type execchild.exe 37 followed by echo %ERRORLEVEL% inside the guest CMD, avoiding accidental expansion by an outer host command shell.

## Suggested next bounded task

Add focused tests for stdin/stdout/stderr inheritance, a small producer | consumer pipeline with EOF/handle-close behavior, and interactive ERRORLEVEL. Address observed failures before broadening process semantics. Then design SESMGR/START support and PM launch routing for Sarien; be explicit about console/session ownership and whether parents wait. Full 16-bit execution and a software CPU fallback are separate future projects.

Use short practical test suites; Jason already ran many regressions and stopped a long 50-run torture loop after repeated 30-worker/1000-round passes. Preserve existing milestone archives and avoid requiring needless full torture reruns for small changes.
