# Milestone 22 — first recovered CMD built-in boundary

M22 builds on the successful M21 personality shell and turns its ad-hoc command
checks into a dispatch table shaped after the Dec-1991 NT CMD source tree.

The command names and handler names are grounded in the recovered COFF symbols
and the older OS/2 command table.  This is still integration scaffolding, not a
claim that the Microsoft source has been reproduced line-for-line.

## Why this milestone exists

M21 proved that a native 32-bit command front-end can start original OS/2 LX
programs through:

```
cmd32os2 -> DOSCALLS.283 -> os2host32 --run -> original LX child
```

M22 adds the internal-command seam used by the 1991 NT shell so the scaffold can
now be replaced module-by-module rather than growing into a separate shell.

Representative recovered boundaries:

```
DIR      -> _eDirectory  cinfo.c
TYPE     -> _eType       cinfo.c
VER      -> _eVersion    cinfo.c
CD       -> _eChdir      cpath.c
MD       -> _eMkdir      cpath.c
RD       -> _eRmdir      cpath.c
SET      -> _eSet        cenv.c
ECHO     -> _eEcho       cbatch.c
PAUSE    -> _ePause      cbatch.c
CLS      -> _eCls        cother.c
EXIT     -> _eExit       cother.c
REM      -> _ParseRem    cparse.c
```

See `analysis/m22_builtin_dispatch.tsv` for the OS/2 handler address, NT symbol,
NT source module and current M22 status.

## Built-ins implemented in the scaffold

```
DIR
TYPE
VER
CD / CHDIR
MD / MKDIR
RD / RMDIR
SET
ECHO
PAUSE
REM
CLS
EXIT
```

`DIR` supports a normal current-directory listing, a directory path, or a
wildcard filespec.  Its formatting is intentionally simple at this stage; the
important M22 change is that `DIR` is now an internal command instead of falling
through to `DosExecPgm("dir")`.

The file/path built-ins are currently implemented against Win32 because
`cmd32os2.exe` itself is a native personality front-end.  As the recovered
Microsoft modules are brought across, the OS/2 personality boundary can be
moved down to DOSCALLS in the same way external execution already is.

## Reserved historical built-ins

The known command names are now recognized even when their implementation has
not yet been transplanted.  For example:

```
COPY
DEL / ERASE
REN / RENAME
MOVE
FOR
IF
GOTO
CALL
SHIFT
SETLOCAL / ENDLOCAL
START
DETACH
...
```

These report an explicit M22 "not implemented" message rather than being
mistaken for external programs.

## Useful regression

After `make`:

```
cmd32os2.exe
```

Try:

```
dir
cd ..
dir *.exe
type some-text-file.txt
set M22TEST=yes
set M22TEST
args.exe one two three
hi.exe
```

The final two should still travel through `DOSCALLS.283` and execute as original
LX images under `os2host32`.

## C89 / MSC8 direction

`cmd32os2.c` remains ANSI-C89-oriented.  The M22 source was syntax-checked with
`-std=c89 -Wall -Wextra -pedantic`.  It also avoids newer Win32 helpers where an
early Win32 equivalent is sufficient (for example, local file time conversion
uses `FileTimeToLocalFileTime` + `FileTimeToSystemTime`).

## Next target

The next high-value transplant is the parser/batch side rather than more simple
filesystem built-ins:

1. `clex.c` tokenization
2. `cparse.c` command/operator parse
3. `cbatch.c` control language (`IF`, `FOR`, `GOTO`, `CALL`, `SHIFT`,
   `SETLOCAL`, `ENDLOCAL`)
4. `cext.c` execution boundary ending in `DosExecPgm`

That is the point where CMD32OS2 begins to behave like the actual 1991 command
processor lineage rather than a dispatch harness.
