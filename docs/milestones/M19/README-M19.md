# Milestone 19b - DosExecPgm host-stack worker

This revision fixes the first `DosExecPgm` execution failure seen when an LX
parent called ordinal 283.

The direct LX guest thread runs on the original OS/2 stack.  Small C/386 test
programs have stacks around 10 KB.  Calling Win32 `CreateProcessA` directly
from that guest stack can consume substantially more stack than lightweight
DOSCALLS shims and terminate the parent before the call returns.

`DosExecPgm` now:

1. builds the host command line in heap memory;
2. starts a native Win32 worker thread (normal Windows stack);
3. the worker performs `CreateProcessA`, waits for the child, and captures its
   exit status;
4. the guest thread waits only on the worker handle;
5. `RESULTCODES` are copied back to the OS/2 caller.

Current scope remains `EXEC_SYNC` only.

Regression target:

    os2host32.exe --run exec-test.exe

Expected nesting:

    exec-test.exe (LX parent)
      -> DOSCALLS.283
         -> native worker thread
            -> os2host32.exe --run args.exe one two three
               -> args.exe (LX child)
      <- RESULTCODES
