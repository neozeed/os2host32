# M21 — OS/2 CMD ↔ Dec-1991 NT CMD crosswalk

## Why this exists

OS/2 2.0 GA `CMD.EXE` is an LX image, but its objects and entry code are predominantly 16-bit.  Rather than immediately building a full 286 protected-mode execution engine merely to obtain a shell, M21 uses Microsoft's Dec-1991 NT `CMD.EXE` as the contemporary 32-bit descendant/reference implementation.

The NT binary is unusually valuable because it retains COFF FILE records and named symbols.  We can therefore recover the original Microsoft source-module layout even though we do not have the source tree itself.

## What the NT binary gives us

The current extraction finds:

- 30 source contributions under `C:\nt\private\windows\cmd\`;
- 307 named functions belonging to those command-processor modules;
- parser/lexer/batch/command handlers with useful original names;
- exact RVAs for those functions;
- a clear separation between shell logic and NT-specific I/O/process/console glue.

Key anchors include:

| Role | NT symbol | RVA | Source |
|---|---|---:|---|
| shell entry | `_main` | `0x10000` | `cmd.c` |
| dispatcher | `_Dispatch` | `0x10184` | `cmd.c` |
| lexer | `_Lex` | `0x16838` | `clex.c` |
| parser | `_Parser` | `0x19C3C` | `cparse.c` |
| external command | `_ExtCom` | `0x1594C` | `cext.c` |
| external program | `_ExecPgm` | `0x15B7C` | `cext.c` |
| batch loop | `_BatLoop` | `0x13806` | `cbatch.c` |
| directory built-in | `_eDirectory` | `0x2099C` | `cinfo.c` |
| copy built-in | `_eCopy` | `0x1E5CA` | `cfile.c` |

`_ExecPgm` calls the early NT CRT spawn path.  For the OS/2 personality, this is the natural replacement boundary for `DosExecPgm`/OS2SS rather than a reason to preserve NT `CreateProcess` semantics.

## Command-handler lineage

`os2_nt_command_crosswalk.tsv` compares the recovered OS/2 6.64 command table with symbols preserved in the Dec-1991 NT binary.  Of the 40 recovered OS/2 entries, 38 have high-confidence direct named counterparts. Examples:

| OS/2 command | OS/2 handler | Dec-1991 NT function | NT module |
|---|---:|---|---|
| `FOR` | `2:07B5` | `_eFor` | `cbatch.c` |
| `IF` | `2:0F39` | `_eIf` | `cbatch.c` |
| `GOTO` | `2:0D26` | `_eGoto` | `cbatch.c` |
| `CALL` | `2:12C4` | `_eCall` | `cbatch.c` |
| `SETLOCAL` | `2:11AF` | `_eSetlocal` | `cbatch.c` |
| `ENDLOCAL` | `2:1224` | `_eEndlocal` | `cbatch.c` |
| `COPY` | `2:28D0` | `_eCopy` | `cfile.c` |
| `DIR` | `2:396A` | `_eDirectory` | `cinfo.c` |
| `SET` | `3:11B7` | `_eSet` | `cenv.c` |
| `KEYS` | `3:2E42` | `_eKeys` | `ckeys.c` |

This is much stronger evidence of common ancestry than shared strings alone.

## Porting boundary

`nt_module_port_plan.tsv` classifies each original NT command module:

- **keep** — parser, lexer, batch language and shell-control logic;
- **mixed** — preserve shell algorithms but replace host file/session services;
- **adapt** — replace NT process/console/filesystem boundary with OS/2 personality APIs;
- **replace** — NT-specific initialization/privilege code not relevant to OS/2 semantics.

The most attractive early transplant sequence is:

1. `clex.c` — lexer;
2. `cparse.c` — parser;
3. `cbatch.c` — batch/control language;
4. `cmd.c` / `cop.c` — dispatcher/operators/redirection;
5. `cext.c` — replace its process boundary with our working `DosExecPgm`;
6. `cenv.c` and simple built-ins;
7. file/console/session families through compatibility wrappers.

## `cmd32os2.c`

M21 includes a deliberately tiny C89 bootstrap shell.  It is **not** presented as a recovered Microsoft CMD implementation.  Its role is integration scaffolding while the NT modules are reconstructed.

External executable launch is already routed as:

```text
cmd32os2.exe
   -> DOSCALLS ordinal 283 (DosExecPgm)
      -> OS2HOST32 --run
         -> original unmodified LX child
```

`DOSCALLS.dll` now accepts an optional `OS2HOST32_LOADER` environment variable.  Normal LX guests are unchanged: if it is unset, `DosExecPgm` continues to use the current `os2host32.exe` process.  The native M21 bootstrap sets it to its sibling `os2host32.exe` so the same ordinal-283 implementation can be reused.

The intended regression once built is:

```text
cmd32os2.exe
[C:\...>]args.exe one two three
hello from OS/2
argc c is 4
argv 0 is [args.exe]
argv 1 is [one]
argv 2 is [two]
argv 3 is [three]
```

That keeps the working M19 process path alive while the authentic NT-derived shell core is reconstructed around it.

## Rebuilding the analysis

The original binaries are intentionally not included in this bundle.  Put them somewhere convenient and run, for example:

```text
make crosswalk OS2CMD=C:\path\to\os2-2.0-CMD.EXE NTCMD=C:\path\to\nt-dec1991-CMD.EXE
```

The analysis tools require Python 3 and GNU `objdump` for the NT COFF symbol extraction.  They are development tools only; the eventual runtime remains C89/MSC8-oriented.
