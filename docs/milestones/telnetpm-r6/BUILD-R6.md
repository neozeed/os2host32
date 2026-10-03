# Complete R6 native Win32 source snapshot

This is the complete working native Win32 source tree used for TELNETPM R6,
including the earlier loader, DOSCALLS, PM, profile, switch-list and TCP/IP
work. It is self-contained source, not an R5-to-R6 overlay. No earlier
TELNETPM source packages or patches are required to build it.

The runtime source is copied unchanged from the working R6 tree. Generated
root build products, compiler caches and Git history are omitted. Existing
SDK material, test fixtures, examples and historical documentation are kept.
The historical TELNETPM executable and the compiler toolchain are not supplied.

## Build from this directory

Install GNU make and an i686 MinGW GCC toolchain, then run:

```sh
make -j2 MINGW=i686-w64-mingw32-gcc all socket-smoke.exe telnetpm-api-smoke.exe
```

This builds the native loader, transformer, shell, compatibility DLLs and
two smoke programs. The Makefile already defaults to i686-w64-mingw32-gcc,
so setting MINGW is only needed if your compiler has a different name/path.
Production builds and Windows execution do not require Python.

A clean copy of this exact source snapshot was built successfully using
the command above with MinGW GCC 13.2.0. The build retains existing warnings;
see `verification/full-build.txt`. No new runtime source changes were made
while preparing this full snapshot.

## TCP/IP source included

| File/directory | Role |
|---|---|
| `common/include/os2_net.h` | Shared guest networking ABI |
| `common/win32/os2_socket_win32.c` | Winsock implementation, descriptors, errors, resolver ownership and cancellable blocking calls |
| `dlls/so32dll/so32dll.c` | SO32DLL veneer |
| `dlls/so32dll/so32dll.def` | SO32DLL ordinal exports |
| `dlls/tcp32dll/tcp32dll.c` | TCP32DLL resolver and byte-order functions |
| `dlls/tcp32dll/tcp32dll.def` | TCP32DLL ordinal exports |
| `tests/net/socket-smoke.c` | Native Windows socket smoke |
| `tests/net/test_socket_dlls.py` | Compiled i386 adapter tests |

The full `loader/`, `common/`, `dlls/`, `shell/`, `transformer/`, `tests/`,
`tools/` and other directories present in the native working tree are
included, not just the files changed in R6. In particular, the mixed-image
intake/bridge headers and all earlier profile/switch-list test dependencies
are present.

## Run on Windows

Run from the build directory so the loader and smoke programs load the DLLs
you just built. Retain your existing ETC setting for TELNETPM's profile.

```bat
socket-smoke.exe
telnetpm-api-smoke.exe
os2host32.exe --telnetpm-probe C:\OS2\tmp\telnetpm-phase1\telnetpm.exe > phase2-r6-run.txt 2>&1
```

Use the original 273,544-byte TELNETPM.EXE with SHA-256
`16f34d712cdefb8b3f4fa93b7956fc97429697419deccd80b62b8d838d4960e2`.
The exact-image bridge remains required. This snapshot does not broaden
support to the newer specimen or arbitrary mixed 16/32-bit LX code.

The prior Windows tests passed 95 socket checks, 39 nested R6 API checks
and 131 main API checks. The live application exchanged a BBS banner,
typed login text and a server response. Rendering still needs correction.
These live results refer to the R6 build already tested; the clean rebuild
performed for this archive was a cross-build, not another Windows run.

## Tests and handoff

```sh
make telnetpm-api-host-check catalog-check
make telnetpm-bridge-check TELNETPM=/path/to/old/telnetpm.exe
make socket-dll-check MINGW=i686-w64-mingw32-gcc
```

The host checks need a native C compiler. Catalogue/bridge/socket-DLL tests
need Python 3; bridge/socket-DLL tests also need Unicorn. The native Windows
smoke executables are available for machines without Python.

* [Engineering handoff](docs/current/TELNETPM-R6-HANDOFF.md)
* [Live milestone](docs/milestones/telnetpm/TELNETPM-R6-LIVE.md)
* [Networking implementation notes](docs/current/TELNETPM-WINSOCK.md)

The handoff was originally written for the incremental source archive.
Its fifteen-file inventory describes the R6 delta. Its instructions to
apply patches on R5 are unnecessary for this full source snapshot: all
earlier native work is already included here. The earlier phase/R5 notes
describe their historical status; the live milestone records the later
R6 result.

## Integrating into GitHub

Use this as the complete native source snapshot for comparison/merging into
your checkout. Keep unrelated newer work in that checkout. The frozen WHP
and ReactOS subsystem directories were not part of the native working tree
used to build R6; their absence here does not request their deletion from
your repository. No Git commit or push was performed for this archive.

`verification/working-tree-files.sha256` identifies every original file
copied from the R6 working tree. `SHA256SUMS.txt` covers the complete archive
contents, including this packaging note and verification logs.
