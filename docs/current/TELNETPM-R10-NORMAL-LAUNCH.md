# R10 handoff: normal TELNETPM launch and PM queue lifetime

Date: 2026-10-03. Baseline: complete R9, including the R7 SimCity merge,
R8 font rendering, R9 child geometry, and the complete R6 TCP/IP source.

## Milestone and launch

The known December 1993 TELNETPM image now uses the normal native launch path:

```bat
os2host32.exe --run C:\OS2\tmp\telnetpm-phase1\telnetpm.exe
os2host32.exe --run-quiet C:\OS2\tmp\telnetpm-phase1\telnetpm.exe
```

The image must have SHA-256
`16f34d712cdefb8b3f4fa93b7956fc97429697419deccd80b62b8d838d4960e2`
(273,544 bytes). Recognition uses content, so renaming the file is supported.
This promotes the existing, verified native profile; it does not enable
arbitrary mixed 16/32-bit LX execution or a newer TELNETPM image. The existing
FS-shadow and internal callback bridge remain necessary. The 16-bit code
object remains inaccessible, and unsupported authentication callbacks stop.

Normal resolved imports bind directly to their native adapters. Missing APIs
still stop with an identified import and exit code 4; unsupported authentication
thunks stop with code 5. The seven previously deferred imports are not implemented
by R10: DOSCALLS.110/.111/.300/.356, PMWIN.890, PMCTLS.4, and MSG.6.

`--telnetpm-probe` remains a compatible alias for the same execution path with
per-import tracing. `OS2_TELNETPM_TRACE=1` also enables that trace for `--run`;
unset it or set it to `0` for normal direct bindings. `OS2_PM_TRACE=1` separately
enables existing PM diagnostics. Both launch modes now forward guest arguments
and accept `--argv0 name` before the executable. The GUI workflow still uses
standard Telnet port 23; argument forwarding is not proof of nonstandard-port
support in the guest. `--scan` reports the recognized profile. For a nonexecuting
bridge check use `--telnetpm-check`; generic `--fixups` is unchanged.

## Why closing the window left the host running

The supplied R9 log reaches `WinGetMsg WM_QUIT`, then calls WinInitialize and
WinCreateMsgQueue again and recreates the application. The last blocked
WinGetMsg belongs to that second initialization. The original guest instructions
explain this behavior:

| Object 1 offset | Meaning |
| --- | --- |
| 47AA | Return from WinGetMsg; FALSE takes the exit branch |
| 47CC | Original jump back to initialization at 3A60 |
| 3A9B | Return from WinCreateMsgQueue; zero branches to cleanup |
| 47D4 | Original cleanup, including window destruction and WinTerminate |
| 4880 | Original return from main |

PMWIN previously returned success for every WinCreateMsgQueue call. OS/2
permits only one message queue per thread; another creation must return NULL
with PMERR_MSG_QUEUE_ALREADY_EXISTS (0x1052). See the IBM toolkit reference
[WinCreateMsgQueue errors](https://cyberkinetica.homeunix.net/os2tk45/pm2/345_L2H_WinCreateMsgQueueErr.html)
and [remarks](https://cyberkinetica.homeunix.net/os2tk45/pm2/344_L2H_WinCreateMsgQueueRem.html).

R10 records queue ownership in PMWIN's existing thread-local state. Duplicate
creation fails, destruction validates the current thread's handle, and
WinDestroyMsgQueue/WinTerminate release ownership and the queue accelerator.
Receiving WM_QUIT does not itself destroy the queue. This makes the guest's
second creation fail and reaches its existing cleanup path. The exit fix
changes no guest instruction and adds no process-killing timeout. Existing
DosExit process/thread semantics are unchanged.

Queue handles retain V1's thread-ID representation. HAB validation and USER32
queue disposal semantics are not fully implemented by this change; the host's
implicit USER32 queue still exists after PM ownership is released. The existing
message loop and frame/client model otherwise remain unchanged.

## Validation and remaining acceptance

- 20 host checks exercise the production queue API with controlled per-thread
  state: duplicate rejection, errors, foreign ownership, recreation, termination,
  and allocation failure.
- 300 bridge checks pass using the production emitted x86 under Unicorn,
  including exact-image CLI recognition, guest argument acceptance, fail-closed
  image rejection, traced/direct bindings, and the unresolved-import trap.
  The original post-quit instructions reproduce reinitialization with the old
  success result, and return zero from main with the corrected failure result.
  This test skips unrelated guest resource buffers and substitutes API results;
  it is not a native Windows execution or a complete CRT/thread shutdown test.
- 30 synthetic mixed-intake tests and the independent original-image object
  fingerprints/7,779 internal relocations pass.
- Existing profile (81), switch-list (281), accelerator/text (290), font-query
  (23), API catalogue, and SimCity ordinal-contract host checks pass.
- The complete source cross-build produces 24 PE32/i386 outputs. The new native
  queue smoke tests real ordinal calls, worker isolation, WM_QUIT, and recreation.
  No Windows/Wine runtime is available here; live R10 exit and GUI smoke results
  still need to be checked on Windows. Existing compiler warnings are recorded.

From `runtime/`, keeping the existing working ETC setting and application assets:

```bat
run-queue-lifecycle-smoke.cmd
run-telnetpm.cmd C:\OS2\tmp\telnetpm-phase1\telnetpm.exe
```

Close TELNETPM with its usual Exit action and with the title-bar close button.
The runner should return promptly with code 0 and its os2host32.exe process
should disappear. Repeat after connecting and disconnecting from a BBS. If it
still remains, use the diagnostic runner and send `phase2-r10-probe.txt`:

```bat
set OS2_PM_TRACE=1
run-telnetpm-probe.cmd C:\OS2\tmp\telnetpm-phase1\telnetpm.exe
```

Run the existing API/socket and PM merge smokes, and launch SimCity from its
normal asset directory with `run-pm-app.cmd`. These are the relevant live
regressions because queue ownership applies to every PM application. R9's
24-row geometry, erasure and scrolling evidence remains valid for its build;
R10 GUI acceptance, CP437 pixel-strip comparison, and font resizing remain open.

## Integration

The package contains the complete source, including SO32DLL, TCP32DLL and their
shared Winsock backend. Apply `integration/r9-to-r10.patch` to complete R9 or use
this complete tree. `integration/FILES-TO-COMMIT.txt` lists 11 changed/new files;
1,403 R9 source files remain byte-identical, for 1,414 source files in total.
The patch was applied to a separate baseline copy and compared with the build
inputs. No commit or push was performed.

Only os2host32.exe and PMWIN.dll replace existing runtime binaries; the new
queue-lifecycle-smoke.exe is added. All other packaged runtime binaries are
byte-identical to R9, including PMGPI and the socket adapters. Runners prefer
`runtime/bin`, then `C:\OS2` and the inherited PATH for additional companions;
they preserve ETC and the current working directory. The old revision-specific
loader filename is replaced by the standard os2host32.exe name. The display
server runner now defaults to port 23 (its standalone binary still defaults
to 2323). Windows needs no Python to run these binaries.

Build with GNU make and 32-bit MinGW GCC:

```sh
make -j2 all socket-smoke.exe telnetpm-api-smoke.exe pm-merge-smoke.exe font-render-smoke.exe window-size-smoke.exe queue-lifecycle-smoke.exe telnet-display-server.exe MINGW=i686-w64-mingw32-gcc
make pm-queue-check
make mixed-intake-check telnetpm-bridge-check TELNETPM=/path/to/telnetpm.exe
```

The last two loader checks require Python and the bridge check requires Unicorn
on the build host. The historical TELNETPM executable is not redistributed.
