# Milestone 28A - CMD crosses the OS/2 user-DLL boundary

M28A changes architecture rather than adding another shell command.  The goal
is to make the reconstructed CMD source progressively usable as a genuine
OS/2 LX program.

Before M28A, many CMD modules called Win32 filesystem APIs directly.  Now the
filesystem/path/process-facing command code calls the neutral `cmdos2.h`
interface.  The native bootstrap backend (`cmdos2_win32.c`) resolves historical
32-bit DOSCALLS ordinals from `DOSCALLS.dll`, and *that DLL* contains the Win32
implementation.

```
cmd32os2 / cmdfile
        |
        | CmdO2Open / Move / Find / ...
        v
cmdos2_win32.c          (temporary native binding backend)
        |
        | DOSCALLS ordinals
        v
DOSCALLS.dll
        |
        v
Win32 host services
```

The eventual LX build replaces only the binding backend; `cmdfile.c`, parser,
batch and built-in semantics should not need to know that the host is Windows.

## Moved behind DOSCALLS in M28A

- external program execution (`DosExecPgm` 283)
- open/read/write/close
- delete and move
- find-first/find-next/find-close
- create/delete directory
- set/query current directory and current disk
- path attribute queries

This covers the working commands:

```
COPY /B and COPY + concatenation
DEL / ERASE
REN / RENAME
MOVE
TYPE
DIR
CD / CHDIR
MD / MKDIR
RD / RMDIR
```

It also means batch `IF EXIST` and executable/.CMD resolution use the same
OS/2-facing path services.

## New DOSCALLS exports

```
226 DosDeleteDir
255 DosSetCurrentDir
263 DosFindClose
265 DosFindNext
271 DosMove
274 DosQueryCurrentDir
275 DosQueryCurrentDisk
```

`DosFindFirst` (264) has also been upgraded from the earlier Sarien-only
first-match implementation to retain a search handle for `DosFindNext` and
`DosFindClose`.

## What deliberately remains Win32 in CMD32OS2

M28A is the first cut, not a fake claim that CMD is already host-independent.
The remaining direct host dependencies are now concentrated around:

- pipe worker process creation and standard-handle attachment
- SET/SETLOCAL/ENDLOCAL environment snapshot/restore
- console clear/pause and native standard-handle plumbing

Those are natural M28B targets alongside real VIOCALLS/KBDCALLS and OS/2
handle/pipe operations.

## Regression

Re-run the existing M26/M27 tests; they should behave exactly as before:

```
m24-batch-test.cmd first second
m26-file-test.cmd
m27-copy-binary.cmd
copy /b args.exe args-copy.exe
args-copy 1 2 3
emit-test | upper-test
```

The architectural test is simply that these continue to work after the
filesystem code has stopped calling Win32 directly.

## Build

Normal native bootstrap build:

```
make
```

`cmd32os2.exe` now links `cmdos2_win32.c`.  `DOSCALLS.dll` must be rebuilt too
because M28A adds the directory/search/move/current-directory ordinals.

The portable parser and batch checks remain:

```
make parser-check
make batch-check
```

## Direct LX backend already scaffolded

`cmdos2_os2.c` is included as the alternate direct-Dos* implementation of the
same boundary. It is intentionally not in the default Win32 build yet. Once
pipes/environment/console have moved behind OS/2 APIs, this is the seam that
lets C/386 build CMD as LX without rewriting `cmdfile.c` again.
