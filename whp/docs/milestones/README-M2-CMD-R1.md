# WHP OS/2 V2 — M2 CMD bridge R1

Development continuation from frozen whp_os2_v2-milestone1. Goal: run the supplied cmd32os2_os2.exe through its C/386 VIO/KBD migration helpers. This is a first Windows test build, not a claimed successful live CMD session. Milestone 1 remains separately preserved.

## First run

Extract to a separate directory such as C:\proj\whp-cmd. From the same x64 MSVC developer prompt used for Milestone 1:

```bat
build-v2.cmd
whp_os2_v2_hi.exe --check cmd32os2_os2.exe
run-cmd32.cmd -c "echo WHP_CMD_OK"
run-cmd32.cmd
```

The supplied 91231-byte CMD32 executable is included unchanged. No guest rebuild is needed for those commands. The launcher sets WHP_OS2_FIND_LAYOUT=GA for CMD32's explicitly defined FILEFINDBUF3 wire structure. It does not enable Sarien's uninitialized-count or geometry workarounds. The host's generic default remains Beta-2; run-sarien.cmd explicitly selects BETA within this development build.

The launcher retains your current directory. At the interactive prompt, try ECHO, DIR, CD, CLS, Up/Down history, Left/Right editing and Tab completion, then EXIT. Confirm output/cursor placement before attempting more involved commands. Interactive CMD still reads its existing C:\OS2\ENV\STARTUP.CMD when present; any command errors there can appear before the prompt. The -c smoke test bypasses that startup script. stderr is saved to cmd32-trace.txt beside the launcher. If it stops, send that trace and stdout.

A host usage string still identifies the generic LE/LX runtime. --check maps, validates and lowers recognized helpers with diagnostic import addresses; it does not execute guest instructions or prove console API behavior.

## Scope and implementation

Seven descriptor-driven APIs are implemented: KbdCharIn, KbdFlushBuffer, VioScrollUp, VioGetCurPos, VioSetCurPos, VioWrtTTY and VioGetMode. The source and packed Pascal signatures come from the preserved M31 CMD backend and prior bridge implementation.

The loader accepts SEL16, PTR16:16 and PTR16:32 records only when they form the recognized C/386 migration graph: a 14-byte 16-bit fragment, a matching 32-bit return continuation, alias jump, SS comparison and helper prologue. Every nonflat fixup must be consumed and each 16-bit object must consist entirely of recognized fragments. It replaces the 32-bit helper entry in mapped guest memory with a jump to a WHP hypercall veneer, whose RET immediate cleans up the packed Pascal arguments. The disk executable is not patched. It does not execute arbitrary 16-bit code, install a general selector subsystem or load native Win32 VIO/KBD DLLs.

DOSCALLS.425 DosFlatToSel is a register-ABI RET veneer, preserving EAX as a flat guest pointer token. It must not use the ordinary MOV-EAX hypercall stub, which would destroy the incoming pointer. Other APIs decode WORD scalars and DWORD pointer tokens in rightmost-argument-first Pascal frame order. No guest token is cast directly into a host pointer.

VIO cursor/mode calls use coordinates relative to the native console viewport. Mode output is limited to the tested 12-byte prefix. Scroll/clear honors bounds and cell attributes. TTY writes guest bytes to host stdout. KbdCharIn writes the packed 10-byte KBDKEYINFO, supports wait/nowait, extended scan codes, shift state and repeats; empty blocking calls enter WAIT_KBD. The scheduler polls every 10 ms while letting runnable guest peers and PM progress. Input mode is restored on normal runtime cleanup. Console KBD services require an actual console; redirected input belongs to DosRead.

Additional DOS services cover cwd/default drive queries/changes, mkdir/remove/move, handle duplication, anonymous pipe creation and LE/LX application-type queries. These support shell built-ins and their groundwork. Directory enumeration can explicitly return GA FILEFINDBUF3: next-offset at 0, attributes at 24, name length at 28, name at 29. Default Beta-2 remains name-at-23. Both currently return one entry per call.

## Explicit next boundaries

DOSCALLS.283 DosExecPgm, .280 DosWaitChild and SESMGR imports resolve, but return error 1 with an explicit diagnostic. External programs, pipeline worker processes, START, PS/KILL and PM auto-session launch are NOT implemented by this build. Imported services are not silently reported as successful. The inherited CMD banner describes the earlier os2host32 implementation, so it must not be read as a capability claim for this WHP host.

Anonymous pipes exist, but pipe I/O itself retains the older synchronous DosRead path; guest-aware pipe I/O and process lifecycle need a subsequent phase. Standard handle duplication and file redirection have native helper checks, but Windows shell redirection needs live verification. No separate per-drive cwd table, UNC current-drive emulation, general VIO buffers or all KBD APIs are claimed. CLI input is cooperative polling, not compute-loop preemption.

## Recovered source and guest test

cmd32-source contains the seven shell/backend C modules, required headers/definitions and the existing complete console probe from os2host32-m31-final.zip. The source is preserved as recovered, including its provenance comments. This is the reconstructed command processor developed using OS/2/1991 NT command behavior; it is not an assertion that these files are the original Microsoft CMD source.

```bat
build-c386-cmd32.cmd
run-console.cmd
```

The builder uses cl386 /Gd and the user's LINK386 shim, defaults OS2LIB to C:\c386\lib, and produces a rebuilt shell plus console test under cmd32-source. It deliberately does not overwrite the supplied root executable. The console test exercises cursor, clear, flush, TTY and a key read; press a key when prompted and look for M29H_OS2_CONSOLE_BACKEND_OK. C/386/MSVC compilation and actual WHP execution still require the user's Windows tools.

## Completed native checks

- Production LE/LX loader + bridge installer: the exact supplied CMD32 passes, reporting 753 internal records / 1149 sites and 82 external sites, with all seven helpers recognized. LE and LX hello fixtures pass. A corrupted migration fragment is rejected.
- Packed console dispatch: pointer-token veneer, WORD/DWORD ordering, viewport/cursor mapping, mode write bounds, TTY bounds, clear, waiting/yield, extended keys, repeats/flush, unavailable console and mode restoration pass.
- Existing scheduler/callback/info/PM/GPI/Sarien helper checks pass with the new WAIT_KBD path.
- Directory checks pass for Beta-2 and explicit GA layouts, buffer guards and Sarien's existing opt-in count behavior.
- Native filesystem shims test cwd wire layout, create/move/remove, pipe/duplicate lifetimes and explicit process-call failure; earlier file-I/O checks pass.

These execute production C helpers with mocked Windows/WHP services or POSIX file handles under ASan/UBSan. They do not execute x86 guest instructions or establish actual Windows console behavior. Logs are in validation/m2-*. No old torture suite was rerun on Windows here.

To repeat on Linux: tests/run-loader-check.py takes executable paths; tests/run-sync-check.py runs console/scheduler/graphics helpers; tests/find-check.c and tests/fileio-posix-check.c compile with GCC and ASan/UBSan. Use ASAN_OPTIONS=detect_leaks=0 in the existing sandbox environment. The latter's filesystem tests must run in a disposable test environment.

Read SHA256SUMS-M2-CMD-R1.txt for this tree. All earlier milestone hashes/readmes are historical. Start next work from this development package after obtaining the first Windows trace; keep the Milestone 1 archive untouched.
