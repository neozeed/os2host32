# Milestone 29K1 - C/386 nested-command stack hardening

M29K proved the child-lifecycle path itself: EXEC_ASYNCRESULT + DosWaitChild,
synchronous child result propagation into ERRORLEVEL, and the DETACH execution
mode all reached the real backend.  The full C/386 shell then exposed a second
historical-stack problem when `IF ERRORLEVEL ... command` recursively reparsed
the selected command and raised runtime error R6000.

The cause was not DosWaitChild.  Several reconstruction-era convenience buffers
were automatic arrays and therefore accumulated on the small OS/2 C/386 stack
across nested parse/execute calls:

* CmdParserEx: ParserState contains roughly 12 KB of lexer/token state.
* InitLex: an additional 8 KB expansion buffer.
* SubVar: an additional 8 KB environment-value scratch buffer.
* CmdBatchIf / cb_run_conditional: multiple 4 KB command buffers.
* run_line / execute_node_core: another 4 KB each.

M29K1 keeps the explicit 61440-byte shell STACKSIZE from M29J1, but removes the
need to consume most of it merely to parse an IF command:

* ParserState is heap-resident for the duration of CmdParserEx.
* variable expansion writes directly into LexState::buffer;
* SubVar's large value scratch is heap-resident;
* run_line allocates the batch-expansion buffer only when a batch is active;
* execute_node_core duplicates a simple command at its actual size;
* CmdBatchIf operands and conditional command copies are heap-resident;
* CmdBatchFor and batch-frame line copies no longer reserve 4-8 KB frames.

No loader, LE/LX fixup, far16 bridge, DOSCALLS ordinal, or VIO/KBD behavior is
changed by this milestone.

## Recommended test

Build as usual:

    make
    build-os2-shell.cmd

Then run the hosted shell:

    os2host32 --run cmd32os2_os2.exe

and exercise:

    rc-child 37
    echo %errorlevel%
    if errorlevel 37 echo IF_ERRORLEVEL_OK
    if not errorlevel 38 echo IF_NOT_ERRORLEVEL_OK
    if exist rc-child.exe echo IF_EXIST_OK
    if "same"=="same" echo IF_COMPARE_OK
    for %i in (one two three) do echo FOR=%i

The first two M29K lifecycle checks remain useful:

    os2host32 --run cmdos2_os2_wait_test.exe

## About cmd32os2.exe and rc=87

`cmd32os2.exe` is the separate native Win32 bootstrap frontend; it is not the
C/386 LE/LX shell loaded by OS2HOST32.  The hosted M29K trace showing
`rc-child 37` followed by `%ERRORLEVEL%=37` proves that the hosted
DosExecPgm path remains alive.  If the native bootstrap frontend returns 87,
run it with:

    set OS2_TRACE_EXEC=1
    cmd32os2.exe

and reproduce the child launch.  M29J2/M29K will then print whether 87 came
from environment validation or CreateProcessA, plus the nested command line
and handle types.  Do not conflate that native-bootstrap diagnostic with the
hosted C/386 shell path.
