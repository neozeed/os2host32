# M29L - real SESMGR / DosStartSession START

M29L freezes the M29K4 `DosExecPgm` lifecycle work and opens the next OS/2
layer: **Session Manager**.

The recovered OS/2 2.0 CMD import map contains `SESMGR.17 DOSSTARTSESSION`.
Accordingly, `START` is no longer approximated with an async `DosExecPgm`.
It crosses a separate `SESMGR.dll` personality boundary and creates a new
hosted OS/2 session.

## Architecture

```
CMD START foo args
    |
    v
CmdO2StartSession
    |
    +-- native CMD ------> GetProcAddress(SESMGR.dll, ordinal 17)
    |
    `-- C/386 CMD -------> import SESMGR.17
                              |
                              v
                        DosStartSession
                              |
                              v
                  new os2host32 --run-quiet foo.exe
                  in a new host console/session
```

`DosStartSession` is a 32-bit API.  M29L therefore **does not add another
migration thunk**.  The seven proven VIO/KBD C/386 far16 thunks remain the
entire mixed-mode set.

## Build

Build the Win32 personality set first so `SESMGR.dll` is present:

```
make
build-os2-shell.cmd
```

The C/386 shell link should gain:

```
SESMGR.17
```

while still reporting seven migration thunks and a `0x62` 16-bit object.

## Direct API regression

```
set OS2_TRACE_SESSION=1
os2host32 --run cmdos2_os2_start_test.exe
```

Expected shape:

```
SESMGR START: program=[m29l-session-child.exe] ...
SESMGR START: session=... pid=...
started session=... pid=...
session is separate from DosWaitChild process set
M29L_START_SESSION_API_OK
```

The fixture creates `m29l-start-api.ok` from the new session after a short
delay; the regression checks and removes it.

## Shell START

Run either frontend:

```
cmd32os2.exe
```

or:

```
os2host32 --run cmd32os2_os2.exe
```

Then:

```
start m29l-session-child m29l-start-shell.ok
```

The parent prompt should return immediately.  After roughly a second:

```
type m29l-start-shell.ok
```

should show:

```
M29L_START_SESSION_OK
```

There is also:

```
examples\m29l-start-test.cmd
```

Run it from the milestone root.

A more interesting test is an existing hosted program:

```
cd ..\sar
start sarienlx
```

That should start Sarien through **SESMGR**, in a separate session, while the
parent CMD prompt returns immediately.

## Scope of this first slice

M29L intentionally implements only:

```
START program [arguments]
```

It does not yet claim compatibility for the historical START switch set,
starting CMD/BAT files, session switching/stopping, TermQ notifications,
fullscreen/windowed policy, or detailed PM session behavior.  Those can now
be added on top of a real `DosStartSession` boundary instead of becoming
special cases in `DosExecPgm`.
