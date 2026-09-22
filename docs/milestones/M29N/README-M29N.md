# M29N — OS/2 command lookup, PATH probing, and argument-tail fidelity

M29N freezes the M29M redirection/pipeline engine and moves command discovery
back into reconstructed CMD32OS2 instead of depending on Win32 process lookup.

The goal is deliberately shell-facing: the command processor decides **what
file a command name means**, while `DosExecPgm` remains responsible for
starting the resolved OS/2 executable.

## Command search model

For an external command with no explicit extension, CMD now probes each search
location in this order:

```text
.COM
.EXE
.CMD
.BAT       (retained as a compatibility extension for the recovered batch layer)
```

The current directory is searched first.  If the command name contains no
path component and was not found there, each element of CMD's own `PATH`
environment is searched from left to right.

Quoted PATH elements are accepted:

```text
PATH="C:\Program Files\OS2 Tools";C:\OS2;C:\UTIL
```

An explicit path never falls through to an unrelated PATH directory.  An
explicit extension is also exact: `FOO.CMD` does not silently become
`FOO.EXE`.

`.CMD` and `.BAT` files are handed to the reconstructed batch processor.  A
resolved `.COM` deliberately reports that the DOS-session personality is not
yet implemented rather than pretending a DOS binary is an OS/2 LE/LX image.

## Typed command name versus resolved module

M29N separates two values that earlier milestones conflated:

```text
what the user typed        pathprobe
resolved executable        m29n-bin\pathprobe.exe
```

`DosExecPgm` receives the resolved executable as `PgmName`, but the first
string in the native OS/2 argument block remains the command token as typed.
That means a C/386 child can see:

```text
ARGV0=[pathprobe]
```

rather than having PATH resolution silently rewrite it to:

```text
ARGV0=[m29n-bin\pathprobe.exe]
```

The parameter tail after the executable token remains untouched by CMD.  This
is especially important for quoted and empty arguments.

## Quoted program paths

The existing parser already preserves quotes inside a simple command node.
M29N exercises that boundary explicitly:

```text
"m29n space\quoted args" "one two" "" three
```

The executable token is unquoted for lookup, `.EXE` probing occurs in the
explicit directory, and the original parameter tail is passed through the
OS/2 `program-name\0argument-tail\0\0` block.

## Trace

Set this before launching CMD32OS2:

```cmd
set CMD32_TRACE_RESOLVE=1
```

A PATH hit then reports the typed name, selected module, kind, and search
location, followed by the argv0/tail sent to `DosExecPgm`:

```text
CMD RESOLVE: typed=[pathprobe] resolved=[m29n-bin\pathprobe.exe] kind=executable via=m29n-bin
CMD ARGS: argv0=[pathprobe] tail=[alpha]
```

## Build

Build as usual on the C/386 machine:

```cmd
make
build-os2-shell.cmd
```

M29N adds `m29n-args.exe`, then copies that same C/386 executable into several
fixture locations:

```text
m29n-bin\pathprobe.exe
m29n-bin\pathpick.exe
m29n path bin\qpathprobe.exe
m29n space\quoted args.exe
```

It also includes `m29n-bin\pathpick.cmd` beside `pathpick.exe` so the regression
can prove executable-extension precedence inside one PATH directory.

## Regression

Run either frontend:

```cmd
cmd32os2.exe
```

or:

```cmd
os2host32 --run cmd32os2_os2.exe
```

Then execute:

```text
examples\m29n-command-resolution-test.cmd
```

Useful expected fragments are:

```text
--- extensionless executable through PATH; argv0 stays typed ---
M29N_ARGS_OK
ARGC=2
ARGV0_LEN=9 ARGV0=[pathprobe]
ARGV1_LEN=5 ARGV1=[alpha]

--- quoted PATH element containing spaces ---
M29N_ARGS_OK
ARGV0=[qpathprobe]
ARGV1=[path spaced]

--- quoted explicit program path + spaces + empty argument ---
M29N_ARGS_OK
ARGC=4
ARGV0=[m29n space\quoted args]
ARGV1=[one two]
ARGV2_LEN=0 ARGV2=[]
ARGV3=[three]

--- extensionless CMD fallback through PATH ---
M29N_CMD_PATH_OK ...

--- current directory is searched before PATH directories ---
M29N_CURRENT_DIR_CMD_OK

M29N_COMMAND_RESOLUTION_OK
```

For `pathpick`, seeing `M29N_ARGS_OK` is correct; seeing
`M29N_WRONG_CMD_SELECTED` means the `.CMD` file incorrectly won over the
`.EXE` in the same directory.

## Frozen invariants

M29N does not change the loader, mixed-mode bridge, Session Manager, or pipe
engine.  The hosted shell should therefore retain:

- `DOSCALLS`, `VIOCALLS`, `KBDCALLS`, and `SESMGR`
- `SESMGR.17`
- exactly seven recognized VIO/KBD migration thunks
- the 16-bit thunk object at `0x62` bytes
- M29M descriptor/pipeline behavior
- M29K/K4 Ctrl+C, ERRORLEVEL, and DETACH behavior
- M29L/L1 `START` / `STARTDATA` behavior

## Real-machine follow-up

The first native/hosted M29N run proved the resolver but also showed that the
DOSCALLS -> nested OS2HOST32 transport was still reconstructing C/386
`argv[0]` from the resolved executable basename.  M29N1 fixes that lower
process-startup boundary; the M29N trace itself was already carrying the
correct typed argument string.
