# M24 - recovered `cbatch.c` boundary

The Dec-1991 NT `CMD.EXE` retains a COFF FILE contribution for:

```
C:\nt\private\windows\cmd\cbatch.c
RVA 0x00013716 .. 0x000153FE
size 0x1CE8
```

`analysis/NT1991_CBATCH_DISASM.txt` is the machine-code extract and
`analysis/m24_cbatch_functions.tsv` is the retained symbol index.  The image
contains 29 named functions in this source contribution, including:

```
BatProc        0x13716
BatLoop        0x13806
SetBat         0x13A36
DisplayStatement 0x13D56
OpenPosBat     0x14056
eEcho          0x140EE
FvarRestore    0x14196
eFor           0x141BE
FWork          0x14522
SubFor         0x14582
SFWork         0x1478E
ForFree        0x1495A
eGoto          0x14982
eIf            0x14BA6
eErrorLevel    0x14C3E
eExist         0x14C5A
eNot           0x14C6A
eStrCmp        0x14C92
ePause         0x14CD2
eShift         0x14D92
eSetlocal      0x14E0E
eEndlocal      0x14EE6
ElclWork       0x14F1E
eCall          0x14FA6
CallWork       0x14FBE
eExtproc       0x151F2
ExtPWork       0x15206
```

## Strong semantic anchors

The retained code is unusually helpful:

- `eErrorLevel` parses the requested integer and compares it against CMD's
  current result code with `setle`, confirming classic `IF ERRORLEVEL n`
  semantics: true when the current errorlevel is **greater than or equal to**
  `n`.
- `eExist`, `eNot` and `eStrCmp` are separate condition helpers called through
  the `eIf` condition tree.
- `eShift` visibly copies a nine-entry parameter window downward and advances
  the remaining-argument pointer.
- `eGoto` scans the open batch file for `:` labels and rewrites the current
  batch position rather than invoking another program.
- `eSetlocal` allocates/stashes a copy of environment-related state in the
  active batch frame; `eEndlocal`/`ElclWork` restore/free that state.
- `eFor` has dedicated `FWork`, `SubFor`, `SFWork` and `ForFree` helpers rather
  than being implemented by the generic expression parser.
- `eCall` is a small front end over the much larger `CallWork`, which parses and
  recursively executes another batch/command context.

## M24 semantic lift

`cmdbatch.c` is a clean-room C89 semantic reconstruction shaped around those
boundaries. It is deliberately portable and accepts host callbacks for command
execution and path existence.  M24 implements:

- `.CMD` / `.BAT` file execution;
- labels and `GOTO`, including `GOTO :EOF`;
- `%0..%9`, `%*`, and `SHIFT`;
- `IF ERRORLEVEL`, `IF EXIST`, `IF NOT`, and `IF lhs==rhs`;
- `FOR %i IN (...) DO ...` interactively and `FOR %%i ...` in batch files;
- `CALL other.cmd ...` through normal command dispatch;
- `CALL :label ...` using a nested batch frame;
- `SETLOCAL` / `ENDLOCAL` host-environment snapshots;
- automatic SETLOCAL unwind when a called batch file returns.

The implementation is not claimed to be Microsoft's original source.  It is a
behavioral reconstruction anchored to the exact 1991 binary and retaining the
historical function/module boundaries.

## Known M24 limits

- FOR set expansion currently iterates literal whitespace/comma-separated
  items; wildcard expansion is not yet lifted.
- modern `%~f1`-style parameter modifiers are not implemented.
- `CALL`'s later CMD "double expansion" tricks are not implemented.
- `EXIT /B`, command extensions, delayed expansion, and modern `/I` IF syntax
  are intentionally outside this 1991-oriented milestone.
- pipeline/redirection execution remains parsed by M23 but is still deferred.
