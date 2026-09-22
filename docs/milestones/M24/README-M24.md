# Milestone 24 - `cbatch.c` semantic lift

M24 moves `cmd32os2` from an interactive parser scaffold into a usable batch
command processor based on the recovered Dec-1991 NT CMD `cbatch.c` boundary.
The original binary retains 29 named functions in that source contribution;
see `analysis/CBATCH_RECOVERY.md` and `analysis/NT1991_CBATCH_DISASM.txt`.

## Newly live commands

```
IF
FOR
GOTO
CALL
SHIFT
SETLOCAL
ENDLOCAL
```

The existing M22/M23 built-ins and the original-LX `DosExecPgm -> OS2HOST32`
path remain unchanged.

## Batch file execution

CMD32OS2 now recognizes `.CMD` and `.BAT` as command-processor inputs.  With no
extension, M24 searches the implemented personalities in this order:

```
.EXE
.CMD
.BAT
```

`.EXE` continues through DOSCALLS.283 into `os2host32 --run-quiet`; `.CMD` and
`.BAT` run inside the reconstructed cbatch layer.

Supported parameter syntax includes `%0..%9`, `%*`, `SHIFT`, labels, `GOTO`,
`GOTO :EOF`, nested batch calls, and `CALL :label` subroutines.

## IF

M24 implements the condition families visible as distinct retained functions in
1991 `cbatch.c`:

```
IF ERRORLEVEL 7 command
IF NOT ERRORLEVEL 7 command
IF EXIST file command
IF NOT EXIST file command
IF "one"=="one" command
IF NOT "one"=="two" command
```

`ERRORLEVEL n` is the historical >= test, not equality.

## FOR

Interactive form:

```
FOR %i IN (one two three) DO ECHO %i
```

Batch-file form:

```
FOR %%i IN (one two three) DO ECHO %%i
```

M24 also fixes the M23 lexer interaction that could mistake the two `%i`
occurrences for one giant `%environment variable%` reference.

## SETLOCAL / ENDLOCAL

`SETLOCAL` snapshots the process environment and `ENDLOCAL` restores it.  Any
unmatched localization created by a called batch file is unwound when that
batch invocation returns, preserving the caller's environment.

## Regression script

Copy both files from `examples/` beside `cmd32os2.exe` (and the existing
`status7.exe` test guest), then run:

```
cmd32os2.exe examples\m24-batch-test.cmd first second
```

or from inside CMD32OS2:

```
examples\m24-batch-test.cmd first second
```

The script exercises positional parameters, ERRORLEVEL, NOT EXIST, string IF,
FOR, SETLOCAL/ENDLOCAL, CALL to another batch file, CALL :label, SHIFT, GOTO,
and GOTO :EOF.

## Portable validation

The language core is host-independent and has its own regression harness:

```
make parser-check
make batch-check
```

Both are compiled with strict C89 flags by default.  `batch-check` currently
produces a successful sequence covering positional expansion, IF, FOR, CALL,
SHIFT and GOTO.

The Win32 integration source was also syntax-checked against the same strict
C89 mode.  The release remains aimed at eventual MSC 8 / VC1 compilation;
`build-msvc.cmd` now links `cmdbatch.c` into `cmd32os2.exe`.
