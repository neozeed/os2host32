# Milestone 29P1 — native redirection/stdio ordering fix

M29P passed the hosted C/386 filesystem regression, but the native bootstrap
showed an apparent pause after `M29P_SELF_COPY_GUARD_OK`.  Typing `EXIT` then
caused the rest of the already-executed batch output to appear.

The batch engine was not actually suspended.  The prompt is written directly
through `CmdO2VioWrtTTY`, while many built-ins report through C `stdout`.  The
native Win32 bootstrap changes CRT fd 0/1/2 as OS/2-style redirections are
applied.  After the two redirected ECHO commands immediately following the
self-copy test, later stdout output could remain buffered, so the direct VIO
prompt overtook it on screen.  Process exit finally flushed stdout, creating
the illusion that the batch resumed only after EXIT.

M29P1 flushes stdout and stderr at every command boundary.  This occurs after
`execute_node()` has completed; redirected nodes have already flushed their
streams before restoring handles, so redirected output still goes to the
correct file.

## Regression

Build normally and run:

    cmd32os2.exe
    examples\m29p-filesystem-test.cmd

The test must now run continuously through:

    M29P_SELF_COPY_GUARD_OK
             2 file(s) copied.
    M29P_COPY_CONCAT_OK
             1 file(s) moved.
    M29P_MOVE_OK
    alpha
    M29P_TYPE_OK
    M29P_FILESYSTEM_OK

without an intervening interactive prompt or needing `EXIT`.

The hosted C/386 shell should continue to pass unchanged.
