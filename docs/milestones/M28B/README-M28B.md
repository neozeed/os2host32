# Milestone 28B - environment + current directory become OS/2-facing

M28A proved that file/path/process calls can cross the DOSCALLS boundary and
that a genuine C/386 LX client can consume our compatibility ABI.  M28B moves
the next stateful pieces out of Win32-specific CMD code: the shell environment
and default-drive/current-directory behavior.

## Why the environment is CMD-owned

OS/2 exposes the process environment through `DosScanEnv` and the PIB returned
by `DosGetInfoBlocks`, but there is no symmetric `DosSetEnv` API for CMD to use.
Historically, C runtime environment changes could also diverge from the
original environment seen by OS/2 APIs and `DosExecPgm`.

M28B therefore keeps a classic double-NUL-terminated environment block at the
CMD personality boundary.  `SET`, percent expansion and `SETLOCAL/ENDLOCAL`
operate on that block, and `CmdO2ExecPgm` passes it explicitly to children.
That is important: a child LX program now sees the environment CMD actually
maintains, rather than whichever host/runtime copy happens to exist.

The direct LX backend initializes this block from:

```
DosGetInfoBlocks -> PIB.pib_pchenv
```

The native bootstrap seeds the same portable store from the Win32 process
environment, but CMD proper no longer calls the Win32 environment APIs.

## New compatibility exports

```
DOSCALLS.220  DosSetDefaultDisk
DOSCALLS.227  DosScanEnv
DOSCALLS.312  DosGetInfoBlocks
```

`DosQueryCurrentDir` now honors a nonzero disk number.  `DosSetCurrentDir`
preserves the OS/2 model in which every drive has a remembered current
directory: updating `D:\foo` while C: is current does not silently select D:.

CMD also recognizes a bare drive selector such as:

```
C:
D:
```

through `DosSetDefaultDisk`.

## PATH is live now

`PATH` is no longer a reserved/not-yet built-in.  External command resolution
checks the current directory first and then the CMD-owned PATH.  This also
avoids depending on implicit `DosExecPgm` search behavior after `SET PATH=...`.

Examples:

```
path
path C:\TOOLS;C:\OS2
set PATH=C:\TOOLS;%PATH%
```

## Native regression

After rebuilding everything, from `examples` run:

```
m28b-env-cwd-test.cmd
```

It covers SET, percent expansion, SETLOCAL/ENDLOCAL, current-directory changes,
command lookup through a locally modified PATH, and verifies that the still-native
M25 pipe worker processes receive the CMD-owned environment explicitly.

## Direct C/386/LX environment regression

First rebuild the compatibility DLLs because M28B adds DOSCALLS ordinals 220,
227 and 312.  Then:

```
build-os2-backend.cmd
os2host32.exe --run cmdos2_os2_smoke.exe
```

The build also creates `env-child.exe`, a genuine C/386 LX program.  The parent
smoke program changes `M28BTEST` only in the CMD-owned environment and launches
`env-child.exe` through `DosExecPgm`.  The child reads it with `DosScanEnv`.

A portable host-side store regression is also available with `make env-check`.

Expected key lines:

```
cwd probe=[...\\m28b-cd-probe]
environment snapshot/restore=[outer] ...
env-child: M28BTEST=[from-parent]
M28B_OS2_BACKEND_OK
```

That proves environment mutation and inheritance across two genuine LX
processes without CMD using Win32 environment APIs.

## What remains Win32-specific in CMD

The big remaining areas are now deliberately narrow:

* redirection/standard-handle switching;
* pipe worker/process plumbing;
* console input/output (`CLS`, `PAUSE`, prompt/input path).

Those map naturally onto `DosDupHandle`/`DosCreatePipe` and then VIO/KBD for the
next stage.
