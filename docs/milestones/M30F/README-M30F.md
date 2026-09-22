# Milestone 30F - EMX signal-exception focus

M30E advances the real EMX-bound `hi.exe` through the exception-registration
step and then stops at the next actually-used deferred import:

    DOSCALLS.378

OS/2 ordinal 378 is `DosSetSignalExceptionFocus`.

## M30F behavior

M30F implements the process-level bookkeeping EMX expects when it asks OS/2
to receive Ctrl-C and Ctrl-Break as signal exceptions.

The 32-bit API is:

    APIRET DosSetSignalExceptionFocus(ULONG enable, PULONG pulTimes)

where `enable=1` acquires signal-exception focus, `enable=0` releases it, and
`*pulTimes` receives the resulting acquire-minus-release nesting count.

For the current single-console/single-thread M30 execution model, M30F keeps a
process-local focus count and returns `NO_ERROR`.  It does not yet translate
Win32 `CTRL_C_EVENT` / `CTRL_BREAK_EVENT` into the guest OS/2 exception chain;
that is deliberately deferred until a program actually requires asynchronous
signal delivery.

M30F also implements ordinal 418, `DosAcknowledgeSignalException`, as the
matching normal-path acknowledgement operation.  EMX-style runtimes commonly
acknowledge Ctrl-C / Ctrl-Break after acquiring focus.  Since M30F does not yet
queue host signal exceptions, acknowledgement currently has no pending state
to clear and returns `NO_ERROR`.

With `OS2_TRACE_EMX_SELF=1`, look for:

    M30F SIGNAL: DosSetSignalExceptionFocus enable=1 times=1

and, if EMX acknowledges signals during this startup path:

    M30F SIGNAL: DosAcknowledgeSignalException signal=........

Success means the deferred-import diagnostic for DOSCALLS.378 disappears.  If
the program reaches another deferred ordinal, that ordinal is the next narrow
M30 compatibility target.

## Build and test

Use the same i686 MinGW environment as M30E:

    make clean
    make tools compat

Place beside the resulting binaries:

    hi.exe     known-good EMX-bound executable
    emx.dll    EMX runtime DLL
    hi         unbound ZMAGIC a.out sidecar (already included)

Then run:

    m30f-emx-hi-test.cmd
