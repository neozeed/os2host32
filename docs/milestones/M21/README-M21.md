# Milestone 21 — CMD lineage / 32-bit OS/2 personality bootstrap

M21 follows the discovery that OS/2 2.0 GA `CMD.EXE`, despite being LX, is
predominantly 16-bit code.  Rather than requiring a 16-bit execution engine just
to obtain an interactive shell, this milestone begins recovering the 32-bit
command-processor lineage from Microsoft's Dec-1991 NT `CMD.EXE`.

## Headline result

The NT binary retains COFF source-file records and named symbols.  Automated
extraction currently recovers:

- **30** Microsoft command-processor source contributions;
- **307** named functions belonging to those modules;
- **40** OS/2 command-table entries cross-referenced to NT handlers;
- **38** high-confidence direct handler matches.

Representative exact semantic matches include:

```
OS/2 6.64       Dec-1991 NT
-----------     ---------------------------
FOR  2:07B5  -> _eFor       cbatch.c
IF   2:0F39  -> _eIf        cbatch.c
GOTO 2:0D26  -> _eGoto      cbatch.c
CALL 2:12C4  -> _eCall      cbatch.c
COPY 2:28D0  -> _eCopy      cfile.c
DIR  2:396A  -> _eDirectory cinfo.c
SET  3:11B7  -> _eSet       cenv.c
KEYS 3:2E42  -> _eKeys      ckeys.c
```

The most important non-built-in boundary is:

```
_ExtCom   cext.c  RVA 1594C
_ExecPgm  cext.c  RVA 15B7C
```

The NT `_ExecPgm` path drops into the early NT CRT spawn machinery.  That is a
natural personality seam: for the OS/2 build, process execution can be replaced
by our already-working `DosExecPgm`/OS2HOST32 route while the parser, lexer,
batch language and dispatch logic remain descended from Microsoft's own 32-bit
implementation.

## New analysis files

```
analysis/nt_source_modules.tsv
analysis/nt_functions.tsv
analysis/NT_CMD_FUNCTION_INDEX.md
analysis/os2_nt_command_crosswalk.tsv
analysis/nt_module_port_plan.tsv
analysis/nt_semantic_anchors.tsv
analysis/common_strings.tsv
analysis/README-CROSSWALK.md
```

The original CMD binaries are not redistributed in this package.

## Re-run the crosswalk

With the OS/2 and NT images available locally:

```
make crosswalk OS2CMD=C:\path\CMD-OS2.EXE NTCMD=C:\path\CMD-NT1991.EXE
```

This analysis path needs Python 3 and GNU `objdump`; neither becomes a runtime
dependency.

## CMD32OS2 bootstrap

`cmd32os2.c` is intentionally small.  It exists to keep integration work moving
while the NT CMD modules are reconstructed rather than inventing a new shell.
The only built-ins currently supplied by the scaffold are enough for testing
(`EXIT`, `ECHO`, `CD`/`CHDIR`, `SET`, `VER`).  Everything else is treated as an
external executable.

External execution follows:

```
cmd32os2.exe
      |
      +--> LoadLibrary(DOSCALLS.dll)
      +--> GetProcAddress(ordinal 283)
      |
      +--> DosExecPgm(EXEC_SYNC, OS/2 packed argument block)
               |
               +--> os2host32.exe --run child.exe ...
                         |
                         +--> original unmodified LX child
```

`DOSCALLS.283` now checks the optional `OS2HOST32_LOADER` environment variable.
Normal LX-hosted callers are unchanged when it is absent.  `cmd32os2` sets the
variable to a sibling `os2host32.exe` so the same process implementation can be
used by a native Win32 personality front-end.

Expected scripted regression after building:

```
cmd32os2.exe -c args.exe one two three
```

or interactively:

```
cmd32os2.exe
[C:\2>]args.exe one two three
```

The child should be the same original LX `args.exe` that already succeeded under
M18/M19.

## C89 / MSC8 direction

`cmd32os2.c` is written in the same C89-oriented style as the rest of the host
work.  The Makefile builds it with the i686 MinGW toolchain and `build-msvc.cmd`
contains an MSC8/VC1-oriented command.  The analysis scripts are development
utilities only.

## Next porting sequence

The crosswalk suggests a much less speculative order than a wholesale rewrite:

1. recover/translate `clex.c` and `cparse.c`;
2. recover `cbatch.c` (`FOR`, `IF`, `GOTO`, `CALL`, `SHIFT`, `SETLOCAL`);
3. recover `cmd.c` / `cop.c` dispatch, operators and redirection;
4. adapt `cext.c` so `_ExecPgm` terminates in `DosExecPgm`;
5. bring over `cenv.c` and simple built-ins;
6. adapt filesystem modules through DOSCALLS;
7. adapt `console.c`/display/keyboard through VIO/KBD;
8. add SESMGR behavior for `START`/session management.

The original 16-bit OS/2 CMD remains the behavioral oracle when NT-specific
behavior diverges.
