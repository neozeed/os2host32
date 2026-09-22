# M29N1 — private OS/2 PATH namespace and DosExecPgm argv0 fidelity

M29N proved that reconstructed CMD32OS2 can own command discovery.  Its first
real-machine run also exposed two useful boundary problems: the native Windows
PATH can be enormous (and contain literal parentheses such as `Program Files
(x86)`), and the nested OS2HOST32 launch was still rebuilding C/386 `argv[0]`
from the resolved executable name.

M29N1 fixes both and begins separating the guest environment from Win32.

## OS2PATH: the personality owns its command namespace

Both frontends still snapshot their process environment so ordinary variables
remain available, but guest `PATH` is now explicitly initialized from
`OS2PATH`:

```text
host OS2PATH present   -> guest PATH = OS2PATH
host OS2PATH absent    -> OS2PATH=. and guest PATH=.
```

The Win32 host `PATH` is therefore no longer used as CMD32OS2's executable
search namespace.  This matters for correctness today and gives the future
module loader somewhere sensible to grow without conflating Windows DLL lookup
with OS/2 program/module lookup.

Inside CMD, these remain synchronized:

```cmd
set OS2PATH=.;C:\OS2;C:\OS2\APPS
```

updates `PATH`, while:

```cmd
path C:\OS2;C:\TOOLS
```

(or `set PATH=...`) updates `OS2PATH` as well.

A starter `env.cmd` is included.  From CMD32OS2 simply run:

```cmd
env
```

It establishes a small OS/2-flavoured command path and also defines
`OS2LIBPATH=.;C:\OS2\DLL` as a reserved namespace for the later real LE/LX
user-mode DLL loader.  M29N1 does **not** consume OS2LIBPATH yet.

Automatic `STARTUP.CMD` execution is intentionally left for a later milestone;
`env.cmd` keeps this first namespace change explicit and easy to test.

## SET values containing parentheses

M29N's native regression hit a parser error while saving the Windows PATH
because a value such as:

```text
C:\Program Files (x86)\...
```

was lexed as CMD grouping syntax.  M29N1 keeps `(` and `)` literal while
lexing the body of a `SET` command.  Parentheses retain their normal grouping
meaning everywhere else.

## PgmName and argv0 are separate again

For an extensionless command:

```text
pathprobe alpha
```

CMD may resolve:

```text
PgmName = m29n-bin\pathprobe.exe
```

while the first string of the OS/2 `DosExecPgm` argument block is still:

```text
pathprobe
```

M29N already constructed that correct OS/2 argument block, but DOSCALLS had to
cross a native Win32 process boundary to start OS2HOST32.  It previously threw
away the first string and OS2HOST32 rebuilt it from the resolved file name.

M29N1 adds an internal loader transport:

```text
CMD32OS2
   DosExecPgm(PgmName=m29n-bin\pathprobe.exe,
              args="pathprobe\0alpha\0\0")
        |
        v
DOSCALLS.dll
   os2host32 --run-quiet --argv0 "pathprobe" "m29n-bin\pathprobe.exe" alpha
        |
        v
OS2HOST32 startup area
   po = fully resolved executable path
   ao = "pathprobe\0alpha\0\0"
        |
        v
Microsoft C/386 startup
   argv[0] = "pathprobe"
   argv[1] = "alpha"
```

The resolved module and the command spelling the application sees are therefore
independent, as they are in the native OS/2 process-startup contract.

## Build

On the C/386 machine:

```cmd
make
build-os2-shell.cmd
```

`make` is required because M29N1 changes both `DOSCALLS.dll` and
`os2host32.exe` as well as the native CMD bootstrap.

## Regression

Run either frontend:

```cmd
cmd32os2.exe
```

or:

```cmd
os2host32.exe --run cmd32os2_os2.exe
```

Then:

```cmd
examples\m29n1-os2path-argv0-test.cmd
```

Expected useful fragments are:

```text
M29N1_PAREN=C:\Program Files (x86)\Tools
PATH="m29n path bin";m29n-bin

M29N_ARGS_OK
ARGV0_LEN=9 ARGV0=[pathprobe]
ARGV1_LEN=5 ARGV1=[alpha]

M29N_ARGS_OK
ARGV0_LEN=10 ARGV0=[qpathprobe]
ARGV1_LEN=11 ARGV1=[path spaced]

M29N_ARGS_OK
ARGC=4
ARGV0_LEN=22 ARGV0=[m29n space\quoted args]
ARGV1_LEN=7 ARGV1=[one two]
ARGV2_LEN=0 ARGV2=[]
ARGV3_LEN=5 ARGV3=[three]

M29N1_OS2PATH_ARGV0_OK
```

With `OS2_TRACE_EXEC=1`, the nested execution trace should now visibly contain
`--argv0`, for example:

```text
DOSCALLS EXEC: command=["...\os2host32.exe" --run-quiet --argv0 "pathprobe" "m29n-bin\pathprobe.exe" alpha]
```

The original M29N regression remains useful and should now run without the
native `Program Files (x86)` SET parse error.

## Frozen invariants

M29N1 changes no LE/LX mixed-mode ABI, session-manager ABI, or pipeline model.
The hosted shell should still show four import modules, `SESMGR.17`, exactly
seven VIO/KBD migration thunks, and the 98-byte (`0x62`) 16-bit thunk object.
