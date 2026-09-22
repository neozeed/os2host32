# Milestone 25 - pipes and redirection

M25 turns the redirection and pipeline nodes already produced by the recovered
`cparse`-shaped parser into executable shell semantics.

Implemented operators:

    command < input.txt
    command > output.txt
    command >> output.txt
    left | right

The operator precedence remains the M23 recovered hierarchy:

    &
    ||
    &&
    |
    redirection
    command/group

## Standard-handle model

Redirection is applied to both the native Win32 standard handle and the C
runtime file descriptor.  This is deliberate: CMD32 built-ins use normal C
stdio, while original LX children ultimately reach `DosRead`/`DosWrite`, whose
OS/2 handles 0, 1, and 2 are backed by Win32 standard handles.

`DOSCALLS.283` now launches `os2host32` with inherited `stdin`, `stdout`, and
`stderr` through `STARTF_USESTDHANDLES`.  Therefore an LX child sees the same
redirected handle state as its CMD32 parent.

## Pipe execution

A pipeline creates one inheritable anonymous Win32 pipe and starts each side in
its own short-lived `cmd32os2.exe -c ...` process.  The left process receives
the pipe write handle as stdout; the right process receives the read handle as
stdin.  The unused pipe end is deliberately made non-inheritable before each
CreateProcess call so EOF can propagate correctly.

The pipeline result is the exit status of the rightmost command.

This design avoids running two command trees concurrently inside the resident
CMD32 process, where CRT/Win32 standard handles are process-global and would
race each other.  It also provides a natural stepping stone toward the eventual
OS2SS process/session server.

## Suggested regression

First test built-in redirection:

    echo hello > m25.txt
    echo world >> m25.txt
    type m25.txt

Then prove that an original LX child inherits stdout:

    args one two three > args.txt
    type args.txt

Build `upper-test.c` with Microsoft C/386 and test input/output inheritance:

    echo hello from a pipe | upper-test
    upper-test < m25.txt > upper.txt
    type upper.txt

Build `emit-test.c` and test an LX-to-LX pipeline:

    emit-test | upper-test

Expected output:

    ONE
    TWO
    THREE

A useful conditional composition is:

    status0 && emit-test | upper-test > result.txt

## Current limits

M25 intentionally does not yet implement numeric handle redirection (`2>`,
`2>&1`, etc.), here-documents, or OS/2 `DosMakePipe` inside the resident CMD32
process.  The first goal is correct inherited standard-handle behavior across
CMD32 -> DOSCALLS.283 -> OS2HOST32 -> LX guest.
