# M29L1 - fleshed-out STARTDATA / START session controls

M29L proved the important architectural boundary: reconstructed CMD `START`
uses the real `SESMGR.17 DosStartSession` personality instead of abusing
`DosExecPgm`.  M29L1 keeps that boundary frozen and starts making the 60-byte
32-bit `STARTDATA` structure meaningful.

## What changed

`cmdos2.h` now exposes a `CmdO2StartOptions` structure and
`CmdO2StartSessionEx()`.  Both the native bootstrap and the direct Microsoft
C/386 backend translate that structure to the same pack(2), 60-byte
`STARTDATA` wire image before crossing SESMGR.17.

The M29L convenience `CmdO2StartSession()` wrapper remains available and
builds the old defaults on top of the extended call.

M29L1 carries these STARTDATA fields across the boundary:

* `PgmTitle`
* `PgmName`
* `PgmInputs`
* `Environment`
* `Related`
* `FgBg`
* `TraceOpt`
* `InheritOpt`
* `SessionType`
* `IconFile`
* `PgmHandle`
* `PgmControl`
* `InitXPos` / `InitYPos`
* `InitXSize` / `InitYSize`
* `ObjectBuffer` / `ObjectBuffLen`

`TermQ` and non-zero installation-database `PgmHandle` requests are explicitly
rejected for now rather than silently pretending those OS/2 services exist.
`IconFile` is carried and traced but host icon binding is not implemented yet.
`ObjectBuffer` is populated for launch failures which occur at the SESMGR host
boundary.

## PgmControl on the Win32 host

For windowed sessions SESMGR maps the useful STARTDATA controls to
`STARTUPINFO`:

* visible / invisible
* maximize
* minimize
* explicit initial position and size (`SSF_CONTROL_SETPOS`)

`SSF_CONTROL_NOAUTOCLOSE` is retained in the wire value but does not yet keep a
finished host console open.  Full-screen VIO remains a semantic session type;
modern Windows cannot recreate the historical OS/2 full-screen VIO mode.

Presentation Manager sessions use a detached host process so they do not grow
an otherwise-empty VIO console.  Applying STARTDATA geometry directly to the
future PM top-level frame belongs in PMWIN and is intentionally left for the PM
work rather than faked in SESMGR.

## CMD START syntax in M29L1

The reconstructed shell now accepts the useful historical START controls:

```
START ["title"] [/F|/B] [/FS|/WIN|/PM] [/MAX|/MIN] [/I] [/N] [/PGM] program [args]
```

Implemented meanings:

```
/F       foreground session
/B       background session
/FS      force OS/2 full-screen VIO session type
/WIN     force OS/2 windowable VIO session type
/PM      force Presentation Manager session type
/MAX     set STARTDATA PgmControl MAXIMIZE
/MIN     set STARTDATA PgmControl MINIMIZE
/I       use the shell-start/inherited environment rather than the current
         private CMD environment
/N       direct program execution (already the native M29L path)
/PGM     the next token is the program name
```

A leading quoted token is treated as the session title.  `/PGM` is therefore
also the unambiguous way to start a quoted executable pathname.

As on historical OS/2, an explicit `/FS`, `/WIN`, or `/PM` session type makes
the request foreground even if `/B` was also supplied.  Plain `START program`
now uses the historical background-session default.

`/C`, `/K`, and `/DOS` are recognized but deliberately report that their
secondary-CMD / DOS-session personalities are not implemented yet.

## Environment /I

Ordinary START snapshots CMD's private environment and passes it explicitly in
STARTDATA.  `/I` sets `SSF_INHERTOPT_SHELL` and does not pass that changed CMD
block.  In OS2HOST32 the new session then inherits the environment in which the
session-manager host itself was started.  This gives the useful historical
property that later `SET` changes in CMD do not leak through `/I`, while we do
not yet claim to have a CONFIG.SYS environment database.

## Build

```
make
build-os2-shell.cmd
```

The C/386 shell still imports only one SESMGR entry:

```
SESMGR.17 DosStartSession
```

and STARTDATA remains a normal 32-bit API.  The expected mixed-mode set is
therefore still the same seven VIO/KBD migration thunks and the same 0x62-byte
16-bit thunk object.

## Direct STARTDATA regression

```
set OS2_TRACE_SESSION=1
os2host32 --run cmdos2_os2_startdata_test.exe
```

The test requests an independent, background, windowable-VIO session with a
title, minimized initial state and explicit 640x400 geometry at 64,48.  The
trace should include roughly:

```
SESMGR START: ... title=[M29L1 STARTDATA regression] ...
 type=2 inherit=1 control=0x8004 pos=64,48 size=640,400 ...
```

and end with:

```
M29L1_STARTDATA_API_OK
```

## Shell regression

Run either frontend and then:

```
examples\m29l1-startdata-test.cmd
```

With `OS2_TRACE_SESSION=1` the three launches exercise:

1. a titled `/WIN /MIN` session using the current private CMD environment;
2. `/I`, which should omit the private `M29L_TOKEN` change;
3. `/PM`, which should use the PM session type and no VIO console.

The first marker should contain:

```
TOKEN=PARENT_ENV_OK
```

while the `/I` marker should contain:

```
TOKEN=<unset>
```

assuming `M29L_TOKEN` was not already present in the environment that launched
CMD32OS2.

## PM bridge

M29L1 intentionally stops at the point where STARTDATA's PM-relevant fields are
now present and preserved.  The next PM work can consume session type, title,
initial visibility and geometry in PMWIN instead of adding one-off START hacks.
