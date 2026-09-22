# Milestone 29O1 — nested FOR repair and trustworthy batch regression

M29O1 is a narrow correction to the M29O batch hardening pass.  Real testing
found two issues: the regression helper paths depended on the current working
directory, and the real parser reduced the nested FOR body `%%i-%%j` to the
literal `j`.  M29O1 fixes both without touching the loader/module-manager,
SESMGR, pipe/redirection, command-resolution, or VIO/KBD personalities.

## Nested FOR parser fix

Batch expansion first reduces `%%i` to `%i`.  Before M29O1, the parser's
environment-variable pass then saw a body such as:

```
for %i in (one two) do for %j in (A B) do echo %i-%j
```

and interpreted `%i-%` as an environment-variable reference.  The resulting
command ended in the stray character `j`, which is exactly what the M29O field
test exposed.

M29O1 makes the lexer identify the single-character variables declared by FOR
statements before environment expansion.  References to those active FOR
variables are preserved regardless of following punctuation, so the complete
nested statement reaches `CmdBatchFor` unchanged.

A new portable regression verifies this directly:

```
make m29o1-parser-check
```

Expected:

```
M29O1_NESTED_FOR_PARSER_OK
```

The older host-only batch regression remains:

```
make m29o-batch-check
```

Expected:

```
M29O_BATCH_HOST_TORTURE_OK
```

## Cwd-independent torture script

The M29O script accidentally embedded `examples\\...` paths.  Therefore a user
who first did `cd examples` asked CMD to run `examples\\examples\\...`.
M29O1 resolves the helper location at runtime and supports both forms:

```
C:\\2\\m29o1_work>examples\\m29o-batch-torture.cmd
```

and:

```
C:\\2\\m29o1_work>cd examples
C:\\2\\m29o1_work\\examples>m29o-batch-torture.cmd
```

The `rc-child.exe` fixture is similarly found in the tree root from either
working directory.

## Self-validating nested FOR

The real nested loop now calls a subroutine for each pair:

```
for %%i in (one two) do for %%j in (A B) do call :for_check %%i %%j
```

A correct run prints exactly these four values:

```
M29O_NESTED_FOR=one-A
M29O_NESTED_FOR=one-B
M29O_NESTED_FOR=two-A
M29O_NESTED_FOR=two-B
M29O_NESTED_FOR_OK
```

The script records each combination and will not print the final PASS marker if
one is missing.

## Batch chaining and child ERRORLEVEL

The called-child and direct-chain fixtures no longer assume a particular cwd.
The direct chain also sets observable flags so the top-level torture script can
prove that:

* the called parent started;
* the direct target ran;
* the replaced parent did **not** continue after the target;
* control finally returned to the original CALLer.

Expected chain output includes:

```
M29O_CHAIN_PARENT_BEFORE
M29O_CHAIN_TARGET_OK
M29O_CHAIN_RETURNED_TO_CALLER_OK
```

and must not include:

```
M29O_CHAIN_PARENT_AFTER_SHOULD_NOT_PRINT
```

The called ERRORLEVEL helper now receives the correct path to `rc-child.exe`
and the torture script verifies an exact result of 23 before continuing.

## Main regression

Build normally on the C/386 machine:

```
make
build-os2-shell.cmd
```

Run in the native frontend and again in the hosted C/386 frontend:

```
examples\m29o-batch-torture.cmd
```

or run it after `cd examples`, as described above.

A correct run contains:

```
M29O_CALL_ERRORLEVEL_OK
M29O_NESTED_IF_OK
M29O_NESTED_FOR=one-A
M29O_NESTED_FOR=one-B
M29O_NESTED_FOR=two-A
M29O_NESTED_FOR=two-B
M29O_NESTED_FOR_OK
M29O_GOTO_OK
M29O_CHAIN_PARENT_BEFORE
M29O_CHAIN_TARGET_OK
M29O_CHAIN_RETURNED_TO_CALLER_OK
M29O_BATCH_TORTURE_OK
```

It must not contain either:

```
M29O_GOTO_FAILED_SHOULD_NOT_PRINT
M29O_CHAIN_PARENT_AFTER_SHOULD_NOT_PRINT
```

and if any self-check fails the ending is instead:

```
M29O_BATCH_TORTURE_FAILED
```

## Ctrl+C

M29O's already-passing Ctrl+C behavior is intentionally unchanged:

```
examples\m29o-ctrlc-batch-test.cmd
```

Press Ctrl+C once while the child waits.  Expected:

```
M29O_CTRL_C_ERRORLEVEL=4
M29O_CTRL_C_RESULT_OK
M29O_CTRL_C_BATCH_CONTINUED_OK
```

## Host checks run for this bundle

The following pass in the build environment used to prepare M29O1:

```
make m29o1-parser-check
make m29o-batch-check
make parser-check
make batch-check
make env-check
make boundary-check
make pipeline-boundary-check
make console-boundary-check
```

A C89 syntax-only pass over the reconstructed CMD/parser/batch/file/environment
sources also succeeds.
