# M28D4 - DosExecPgm inherits OS/2 HFILEs directly

M28D3 proved that ordinary native CMD redirection was healthy, while an LX
filter failed as soon as stdin and stdout were both redirected.  The failing
filter returned 2, which is the fixture's `putchar()` failure path.

The remaining layering error was in `DOSCALLS.DosExecPgm`: its Win32 creation
worker filled `STARTUPINFO` from `GetStdHandle()`.  For the temporary native
CMD bootstrap, those values are a CRT synchronization mirror and can differ
from DOSCALLS' authoritative OS/2 HFILE 0/1/2 duplicates.

M28D4 snapshots `os2_handle(0)`, `os2_handle(1)`, and `os2_handle(2)` before
starting the host-stack worker and uses those exact HANDLEs as the child's
standard handles.  This is the correct personality boundary: an OS/2 child
inherits the OS/2 handle table, not whatever copy MSVCRT currently publishes
as a Win32 process standard handle.

A native `cmd32os2.exe` pipeline worker still normalizes the inherited Win32
handles into CRT fd 0/1/2 when it starts, but that is now strictly bootstrap
glue.

## Tests

Build the Win32 pieces and the C/386 fixtures, then run:

```cmd
make
build-pipe-fixtures.cmd
cd examples
..\cmd32os2.exe
m28c-redir-test.cmd
m28d4-pipe-test.cmd
```

The important pipeline output is expected to include `ONE`, `TWO`, `THREE`
for the LX-to-LX stages and `MIXEDCASE` for the builtin-to-LX case.

For tracing:

```cmd
set CMD32_TRACE_PIPE=1
..\cmd32os2.exe
m28d4-pipe-test.cmd
```
