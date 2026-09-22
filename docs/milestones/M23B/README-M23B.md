# Milestone 23b - quiet LX launches and conditional status regression

M23's `&&` / `||` executor was behaving correctly.  The original `hi.exe`
regression program does not explicitly return zero from `main`: its generated
main calls the print routine and immediately returns, leaving that routine's
return value in EAX.  Printing `"hi\n"` therefore leaves a non-zero process
result (normally 3).  Since DOSCALLS.283 correctly propagates the LX child's
result code, `hi && args ...` must suppress `args`.

Use the explicit regression guests in this bundle to test conditionals:

    status0 && args one two three
    status7 && args one two three
    status7 || args one two three

`status0.c` returns 0; `status7.c` returns 7.

## Quiet nested guest execution

A shell should not print the LX loader inventory every time it starts an OS/2
program.  `os2host32` therefore now accepts:

    os2host32 --run-quiet program.exe [arguments ...]

This follows exactly the same mapping/fixup/import/startup path as `--run`, but
suppresses normal informational diagnostics.  Errors and guest exceptions are
still printed.

DOSCALLS.283 uses `--run-quiet` by default for DosExecPgm children, so normal
CMD32OS2 use now looks roughly like:

    [C:\2>]hi
    hi
    [C:\2>]args 1 3 9
    hello from OS/2
    argc c is 4
    argv 0 is [args.exe]
    argv 1 is [1]
    argv 2 is [3]
    argv 3 is [9]

For loader debugging, direct invocation remains verbose:

    os2host32 --run args.exe one two three

or restore verbose DosExecPgm children temporarily with:

    set OS2HOST32_VERBOSE=1

## Validation

The portable parser regression still passes strict C89 compilation, and the
host build of os2host32 also passes `-std=c89 -Wall -Wextra -pedantic`.
