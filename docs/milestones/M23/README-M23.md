# Milestone 23 - `clex.c` / `cparse.c` semantic lift

M23 replaces the M22 one-command splitter with a C89 parser reconstructed from
Microsoft's Dec-1991 NT `CMD.EXE` COFF symbols and machine code.

This is a **semantic reconstruction**, not a claim that Microsoft's original C
source was recovered verbatim.  The binary does, however, preserve the exact
source-module/function names and enough implementation structure to recover the
parser architecture with high confidence.

## Recovered source contributions

The original executable identifies:

```text
C:\nt\private\windows\cmd\clex.c    RVA 1680C .. 176B8
C:\nt\private\windows\cmd\cparse.c  RVA 19C3C .. 1AD84
```

M23 ships the disassembly of both regions in `analysis/` and a function index in
`analysis/m23_clex_cparse_functions.tsv`.

## Directly recovered parser precedence

The `ParseS0..ParseS3` routines each call the common `BinaryOperator` routine
with a literal operator string.  Those literals are still present in `.data`:

```text
ParseS0   &
ParseS1   ||
ParseS2   &&
ParseS3   |
ParseS4   redirection
ParseS5   command / special statement / (...) / @
```

This order is preserved in `cmdparse.c`.

## New files

- `cmdparse.c` / `cmdparse.h` - portable C89 lexer/parser layer.
- `parse-test.c` - host-side regression harness requiring no Windows SDK.
- `analysis/CLEX_CPARSE_RECOVERY.md` - recovered architecture and evidence.
- `analysis/NT1991_CLEX_DISASM.txt` - the original `clex.c` machine-code range.
- `analysis/NT1991_CPARSE_DISASM.txt` - the original `cparse.c` machine-code range.

## What now works in `cmd32os2`

Existing M22 built-ins and external LX execution remain unchanged, but command
lines now pass through the reconstructed lexer/parser first.

The following parser/execution forms are live:

```text
echo one & echo two
hi && args one two three
missing.exe || hi
(echo one & hi) && args 1 2 3
@echo quiet-prefix-recognized
set FOO=bar & echo %FOO%
```

Important details:

- metacharacters inside double quotes are not treated as operators;
- `%NAME%` environment expansion is implemented at the recovered
  `SubVar`/`MSEnvVar` layer;
- `REM` consumes the remainder as comment data;
- `@` is represented as the recovered command-prefix property;
- syntax errors include a parser offset.

## Parsed but deliberately not executed yet

M23 also constructs nodes for:

```text
command > file
command >> file
command < file
left | right
```

The actual host-handle redirection/pipeline execution is intentionally deferred
rather than faked.  `cmd32os2` reports that these constructs were parsed but are
not yet executable.

Likewise, `FOR`, `IF`, and `DETACH` are recognized at the recovered `ParseS5`
boundary, but their statement semantics remain reserved for the upcoming
`cbatch.c` lift.

## Parser archaeology mode

The Windows build can show the reconstructed AST without loading `DOSCALLS.dll`:

```cmd
cmd32os2 --parse "echo one & hi && args one two"
```

A portable host regression is also available:

```sh
make parser-check
```

Example:

```text
INPUT: echo one & echo two || echo three && hi | args 1 2 3
&
  CMD [echo one]
  ||
    CMD [echo two]
    &&
      CMD [echo three]
      |
        CMD [hi]
        CMD [args 1 2 3]
```

That shape follows the precedence recovered directly from the 1991 binary.

## Build

Normal MinGW build remains:

```sh
make
```

`cmd32os2.exe` now links `cmd32os2.c` + `cmdparse.c`.  `build-msvc.cmd` was
updated the same way for the Microsoft C/C++ 8 / Visual C++ 1.x direction.

The parser itself passes:

```text
-std=c89 -Wall -Wextra -pedantic
```

under the available host compiler.

## Next natural boundary

The recovered `cparse.c` hands special statements into `cbatch.c`.  The next
useful lift is therefore the batch/control family:

```text
FOR
IF
GOTO
CALL
SHIFT
SETLOCAL
ENDLOCAL
```

Redirection/pipeline handle plumbing can then be attached to the already-built
M23 AST without changing the grammar.
