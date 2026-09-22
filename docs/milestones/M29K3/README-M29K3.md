# Milestone 29K3 - Ctrl+C-safe native and hosted CMD

M29K2 proved that native and direct-C/386 CMD now agree on child exit codes,
ERRORLEVEL, environment inheritance and ordinary command execution.  User
interactive testing exposed the next shell/session boundary: pressing Ctrl+C
terminated the whole CMD process on both frontends.

M29K3 fixes the prompt/session side without changing the LE/LX loader, the
seven C/386 far16 migration thunks, or DosExecPgm's ABI.

## Keyboard personality

`KBDCALLS.dll` lazily installs a Win32 console-control handler on the first
historical KBD read.  When Ctrl+C or Ctrl+Break arrives while KbdCharIn/
KbdStringIn is blocked on a real console, the handler keeps the process alive
and enqueues a synthetic OS/2-style Ctrl+C key record (ASCII 3, Ctrl state).
This wakes the existing ReadConsoleInput path instead of inventing a parallel
input mechanism.

When no KBD read is active -- notably while CMD is synchronously waiting for a
foreground child -- the parent process simply consumes its own control event.
The child process receives the real console-control event independently, so a
normal foreground child can still terminate or handle Ctrl+C without killing
the parent shell.

## CMD line editor

`read_console_line()` recognizes ASCII 3 as line cancellation.  It prints:

    ^C

and returns to a fresh prompt without executing the partial command.  The
previous ERRORLEVEL is intentionally preserved because no command was run.

## Tests

Rebuild the Win32 personality DLL set and the C/386 shell:

    make
    build-os2-shell.cmd

Native frontend:

    cmd32os2.exe

Hosted C/386 frontend:

    os2host32 --run cmd32os2_os2.exe

At either prompt, type a partial command and press Ctrl+C.  Expected shape:

    [C:\2\m29k3>]echo this must not run^C
    [C:\2\m29k3>]

The shell must remain alive and the partial command must not execute.

For foreground-child handling, build-os2-shell.cmd also produces
`m29k3-break-child.exe`:

    m29k3-break-child

It waits for ten seconds.  Press Ctrl+C while it is waiting.  The parent CMD
should survive and return to a prompt rather than being terminated with the
child.  Child termination/errorlevel normalization is deliberately left as a
separate semantic check if Windows reports its native CTRL_C exit status.

DETACH remains isolated from console control by its existing detached process
mode.
