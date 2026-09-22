# M29M — stderr, descriptor redirection, and real pipelines

M29M freezes the M29L1 session-manager work and returns to the remaining major
CMD execution boundary: standard handles and pipelines.

The older M28D work already implemented real concurrent pipelines through
`DosCreatePipe`, `DosDupHandle`, `DosExecPgm(EXEC_ASYNCRESULT)`, and
`DosWaitChild`.  M29M completes the shell-facing descriptor semantics rather
than replacing those pipes with temporary files.

## New command syntax

The reconstructed parser now understands the standard descriptors 0, 1, and 2:

```text
program < input.txt
program > output.txt
program >> output.txt

program 2> errors.txt
program 2>> errors.txt

program > both.txt 2>&1
program 2>&1 > stdout-only.txt

program 1>&2
```

Redirections are applied **left-to-right**.  Therefore these deliberately mean
different things:

```text
program > both.txt 2>&1
```

redirects stdout to `both.txt` and then duplicates the already-redirected
stdout onto stderr.

```text
program 2>&1 > stdout-only.txt
```

first duplicates the original stdout onto stderr, then redirects only stdout
to the file.

The implementation saves/restores HFILE 0, 1, and 2 independently through the
existing `CmdO2StdSave`, `CmdO2StdRedirectPath`,
`CmdO2StdRedirectHandle`, and `CmdO2StdRestore` OS/2 boundary.

## Pipelines remain real OS/2-style process pipelines

M29M does **not** implement pipes as:

```text
producer > tempfile
consumer < tempfile
```

The existing process model remains:

```text
producer CMD worker
       |
       | HFILE pipe writer
       v
   DosCreatePipe
       |
       | HFILE pipe reader
       v
consumer CMD worker
```

Both sides are started with `EXEC_ASYNCRESULT` before either side is waited,
so a multi-stage pipeline remains concurrent.

Descriptor redirection is rendered into nested worker command lines too, so
this is now a meaningful test:

```text
m29m-stderr 2>&1 | upper-test > captured.txt
```

Both stdout and stderr are merged into the pipe before the filter runs.

## Build

The normal shell build now also creates the M29M fixtures:

```cmd
make
build-os2-shell.cmd
```

This should produce, among the existing files:

```text
emit-test.exe
upper-test.exe
m29m-stderr.exe
m29m-count.exe
cmd32os2_os2.exe
```

No new DLL or ordinal import is required for M29M.

## Main regression

Run either the native bootstrap:

```cmd
cmd32os2.exe
```

or the C/386-hosted shell:

```cmd
os2host32 --run cmd32os2_os2.exe
```

then:

```text
examples\m29m-redir-pipe-test.cmd
```

The useful output includes:

```text
stdout-file:
STDOUT_LINE

stderr-file:
STDERR_LINE
```

The merged file should contain both:

```text
STDOUT_LINE
STDERR_LINE
```

The append test should show two `STDERR_LINE` entries.

For the ordering test:

```text
m29m-stderr 2>&1 >m29m-order.txt
```

`STDERR_LINE` should remain on the console while `m29m-order.txt` contains
only:

```text
STDOUT_LINE
```

The three-stage pipeline should produce:

```text
ONE
TWO
THREE
```

and the count pipeline:

```text
LINES=3
```

Finally, merging stderr into a pipe should produce a file containing uppercase
versions of both fixture lines.

The script ends with:

```text
M29M_REDIRECTION_PIPELINE_OK
```

## Optional pipeline tracing

The pre-existing worker trace remains useful:

```cmd
set CMD32_TRACE_PIPE=1
```

A pipeline then shows the rendered left/right worker commands, child PIDs, and
final left/right statuses.

## Invariants

M29M intentionally does not change Session Manager or mixed-mode execution.
The C/386 shell should still scan with:

- `DOSCALLS`
- `VIOCALLS`
- `KBDCALLS`
- `SESMGR`
- exactly seven recognized VIO/KBD migration thunks
- the 16-bit thunk object still `0x62` bytes

`START /PM`, Ctrl+C handling, ERRORLEVEL, DETACH, and DosStartSession should
remain regression-stable.
