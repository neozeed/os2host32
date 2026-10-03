# R7: complete source with merged SimCity and TELNETPM PM code

This is a full native Win32 source snapshot, including the TCP/IP adapters,
loader, DLLs, shell, tests and earlier TELNETPM work. No R1-R6 source package
or patch is required. The SimCity fixes supplied in the three comparison
files have been combined with the R6 implementations.

Start with [the merge handoff](docs/current/TELNETPM-SIMCITY-R7-MERGE.md).
It records exactly what was recovered, retained and tested. The existing
[R6 milestone](docs/milestones/telnetpm/TELNETPM-R6-LIVE.md) remains the
live execution/network preservation point; R7's application retests are pending.

## Build

Use GNU make and an i686 MinGW GCC toolchain from this directory:

```sh
make -j2 all socket-smoke.exe telnetpm-api-smoke.exe pm-merge-smoke.exe
```

Set `MINGW=/path/to/i686-w64-mingw32-gcc` if the compiler is not on PATH.
Production builds and native smoke programs require no Python. Host checks
are available with `make telnetpm-api-host-check catalog-check simcity-pmwin885-check`;
the catalogue/static checks require Python 3.

The complete tree was cross-built from a clean directory using MinGW GCC
13.2.0. Existing host checks pass. Native Windows pixel behavior remains to
be checked with the new smoke executable; no Windows execution is claimed
for this build worker.

## Test the included binaries on Windows

The `runtime/bin/` directory has newly built PMWIN.dll, PMGPI.dll and
pm-merge-smoke.exe. Its other DLLs, API/socket smokes and renamed probe loader
are unchanged R6 companion binaries. Other installed compatibility DLLs
remain available through C:\OS2 and inherited PATH.

Open CMD in `runtime/` and run:

```bat
run-pm-merge-smoke.cmd
run-api-smoke.cmd
run-telnetpm.cmd C:\OS2\tmp\telnetpm-phase1\telnetpm.exe
```

Run the same SimCity/Micropolis EXE that you used for the comparison, from
its usual asset directory, using the package's `runtime\run-pm-app.cmd`:

```bat
C:\path\to\os2host32-r7-pm-merge\runtime\run-pm-app.cmd simcity.exe
```

Runners preserve the working directory and existing ETC setting. Logs are
written into `runtime/` with `phase2-r7-` names. The new graphics smoke may
briefly show test windows. The optional socket smoke uses the unchanged
passing R5/R6 binary.

## Integration

All current source files are included at their normal project paths.
Networking is in `common/win32/os2_socket_win32.c`, `common/include/os2_net.h`,
`dlls/so32dll/` and `dlls/tcp32dll/`.

The optional `integration/r6-to-r7.patch` applies to the preserved R6 tree.
It is not needed to build this complete snapshot. Merge current source into
your checkout while retaining unrelated newer work and frozen WHP/ReactOS
directories. Their absence from this native snapshot does not request their
deletion. No commit or push was performed.

`verification/` contains the supplied comparison files, merge audit,
build output and checks. `SHA256SUMS.txt` covers the complete package.
TELNETPM text background/font/codepage issues remain separate work; this
merge does not claim to have fixed all terminal rendering glitches.
