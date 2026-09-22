# Milestone 29J1 — explicit C/386 shell stack

M29J compiled the complete reconstructed CMD32OS2 shell cleanly, but the
first entry into the program failed with Microsoft C run-time error R6000
(stack overflow) before any shell output.

The full shell differs from the previous probes in an important way: `main()`
has a 4096-byte automatic command-line buffer, and the parser adds roughly
12 KB of automatic lexer/token state on deeper paths.  Historical LINK386
applications should not rely on the linker's small default stack for this.

M29J1 changes only `cmd32os2_os2.def`:

    STACKSIZE 61440

No loader, DOSCALLS, VIO/KBD bridge, backend, parser, or shell source code is
changed.  The 60 KB value stays within the traditional 64 KB stack limit of
these linker generations while leaving ample room for the normal parser and
built-in call chains.

Build:

    build-os2-shell.cmd

Then test:

    os2host32 --run cmd32os2_os2.exe -c "echo M29J1_STACK_OK"
    os2host32 --run cmd32os2_os2.exe -c "dir"
    os2host32 --run cmd32os2_os2.exe

If this removes the immediate R6000, the next hardening step is to move the
very large automatic workspaces in COPY off the stack; that command contains
arrays much larger than a historical 64 KB OS/2 thread stack.
