# Milestone 29O — batch-language torture and frame semantics

M29O deliberately leaves the N2b.2 LE/LX module manager frozen and returns to
CMD32OS2.  The goal is not to add another broad command set; it is to make the
existing recovered `cbatch.c` surface survive the kinds of nested batch files
that expose frame, parameter, and ERRORLEVEL mistakes.

## What changed

### Batch positional parameters

`CmdBatchFrame` now keeps the original argument tail as well as the parsed
`%0..%9` vector.  Positional parsing preserves surrounding quote characters
and accepts empty quoted arguments.  For example a CALL with:

```
alpha "beta gamma" "" delta
```

is visible to the called batch/subroutine as:

```
%1 = alpha
%2 = "beta gamma"
%3 = ""
%4 = delta
%* = alpha "beta gamma" "" delta
```

`SHIFT` advances `%1..%9`, but `%0` and the original `%*` tail remain stable.
This is intentionally different from reconstructing `%*` by joining the
already-tokenized arguments, which lost quoting and empty arguments.

### CALL :label owns a localization frame

The shell records the SETLOCAL depth before `CALL :label`.  When the
subroutine returns, any unmatched SETLOCAL scopes opened by that subroutine
are unwound automatically.  This makes the common pattern:

```
:sub
setlocal
set VALUE=private
goto :eof
```

return with the caller's environment restored even without an explicit
ENDLOCAL.

Nested batch files invoked through CALL already had this automatic unwind; M29O
extends the same rule to label/subroutine calls.

### CALL versus batch chaining

M29O distinguishes these two forms:

```
call child.cmd
child.cmd
```

The first pushes a batch invocation and returns to the next line in the caller.
The second is a transfer/chaining operation: the current batch frame is marked
complete before the new CMD/BAT file starts, so EOF in the new file does not
resume the replaced frame.

`CmdBatchEndCurrent()` is the small portable batch-layer primitive used for
that behavior.  `eCall` marks its nested execution so command resolution knows
when the caller must be preserved.

## Regression

Run in either frontend:

```
examples\m29o-batch-torture.cmd
```

The script exercises:

* CALL :label with quoted and empty parameters
* `%0..%9` and `%*`
* SHIFT while `%*` remains the original tail
* CALL's second batch-parameter pass (`%%1` -> `%1` -> value)
* nested CALL :label frames
* automatic SETLOCAL unwind from a subroutine
* automatic SETLOCAL unwind from a called child CMD file
* ERRORLEVEL propagation out of a called CMD file
* nested IF ERRORLEVEL / IF NOT ERRORLEVEL
* IF NOT EXIST and string comparison
* nested FOR loops
* GOTO and GOTO :EOF
* direct batch-file chaining versus CALL

The final line is:

```
M29O_BATCH_TORTURE_OK
```

The batch-chain portion must contain:

```
M29O_CHAIN_PARENT_BEFORE
M29O_CHAIN_TARGET_OK
M29O_CHAIN_RETURNED_TO_CALLER_OK
```

and must **not** print:

```
M29O_CHAIN_PARENT_AFTER_SHOULD_NOT_PRINT
```

### Manual Ctrl+C batch regression

Run:

```
examples\m29o-ctrlc-batch-test.cmd
```

Press Ctrl+C once while `m29k3-break-child.exe` is waiting.  The parent CMD
should survive, the batch file should continue, and the expected result is:

```
M29O_CTRL_C_ERRORLEVEL=4
M29O_CTRL_C_RESULT_OK
M29O_CTRL_C_BATCH_CONTINUED_OK
```

This does not implement an interactive "Terminate batch job?" prompt; it
verifies the existing OS/2-style child termination mapping remains coherent
inside a batch invocation.

## Portable regression

On a normal C89 host:

```
make m29o-batch-check
```

The host-only test checks quoted/empty positional parameters, SHIFT/%*, nested
subroutines, GOTO, and replacement/chaining semantics without requiring the
Win32 personality DLLs or Microsoft C/386.

Expected:

```
M29O_BATCH_HOST_TORTURE_OK
```

The previous checks remain useful:

```
make batch-check parser-check env-check boundary-check pipeline-boundary-check console-boundary-check
```

## Frozen subsystems

M29O does not alter the N2a/N2b guest DLL loader, N2b.2 DLL lifecycle, SESMGR,
pipes/redirection, command lookup, or the seven classic VIO/KBD migration
thunks.  The hosted CMD image should therefore retain the familiar four
personality imports and 0x62 mixed-mode object.
