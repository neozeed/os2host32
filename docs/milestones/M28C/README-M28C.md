# Milestone 28C - OS/2 handles, standard redirection and pipe ABI

M28B proved that the reconstructed shell can own an OS/2-style environment and
current-directory state.  M28C moves the next piece of CMD away from direct
Win32/CRT dependence: standard-handle redirection.

## New DOSCALLS entry points

The compatibility DLL now implements the historical 32-bit Control Program API
entry points:

```text
239  DosCreatePipe
260  DosDupHandle
```

`DosCreatePipe` maps a Win32 anonymous pipe onto two OS/2 HFILE values in the
compatibility handle table.  `DosDupHandle` supports both allocation of a new
HFILE (`FFFFFFFFh`) and duplication onto the standard handles 0, 1 and 2.

## CMD redirection boundary

`cmd32os2.c` no longer contains `_open`, `_dup`, `_dup2`, `_close`,
`_get_osfhandle`, or `SetStdHandle` for `<`, `>` and `>>`.

Instead it uses:

```text
CmdO2StdSave
CmdO2StdRedirectPath
CmdO2StdRestore
```

The native bootstrap implementation lives in `cmdos2_win32.c`.  Even there,
opening the redirection target, append seeking, saving the standard handle and
duplicating the replacement now go through DOSCALLS ordinals 273/256/260/257.
The only remaining Win32/CRT-specific step is synchronizing the native bootstrap's
`stdin`/`stdout`/`stderr` descriptors after the OS/2 HFILE has changed, so its
native built-ins can continue to use ordinary `printf`/stdio safely.  That shim
disappears when CMD itself is built as LX.

The direct OS/2 backend in `cmdos2_os2.c` uses the same real SDK calls:

```text
DosOpen
DosSetFilePtr       (for >>)
DosDupHandle
DosClose
```

This is the implementation the eventual C/386-built LX CMD will use.

## Direct LX pipe test

`build-os2-backend.cmd` now also builds `handle-child.exe`, a genuine LX child
which writes this string directly with `DosWrite(1, ...)`:

```text
M28C_PIPE_CHILD_OK
```

The parent smoke test performs:

```text
DosCreatePipe
      |
      +-- read HFILE
      `-- write HFILE --DosDupHandle--> HFILE 1
                                      |
                                 DosExecPgm
                                      |
                                handle-child.exe
                                      |
                                  DosWrite(1)
                                      |
                                  OS/2 pipe
                                      |
                                 parent DosRead
```

It then separately redirects HFILE 1 to `m28c-redirect.tmp`, writes through
`DosWrite(1)`, restores stdout, reads the file back, and verifies the bytes.

### Build and test

Rebuild the Win32 compatibility DLLs/shell first:

```cmd
make
```

Native CMD regression:

```cmd
cd examples
..\cmd32os2.exe
m28c-redir-test.cmd
```

The older M24/M26/M27/M28B regressions should also remain unchanged.

Build the direct OS/2 probes:

```cmd
cd ..
build-os2-backend.cmd
os2host32.exe --run cmdos2_os2_smoke.exe
```

The new success lines are expected to include:

```text
pipe child exact payload=OK (20 bytes)
stdout redirection/append through DosDupHandle=OK
M28C_OS2_BACKEND_OK
```


## What remains native

Full `A | B` execution in `cmd32os2.exe` still creates the two temporary native
CMD worker processes with Win32 `CreatePipe`/`CreateProcess`.  That is now the
large remaining process-control island.  Once asynchronous OS/2 child execution
and wait semantics are added, that can move behind the same boundary too.
