# Milestone 20 - OS/2 2.0 CMD reconnaissance / mixed-LX gate

Milestone 19b proved direct execution of unmodified flat 32-bit LX programs,
including synchronous `DosExecPgm` parent -> child execution and propagation of
the child result code.

M20 turns the real OS/2 2.0 GA `CMD.EXE` into the next driver. The first result
is an important correction: **GA CMD is an LX executable, but its three objects
are 16-bit objects.** LX is a container format capable of mixing 16- and 32-bit
objects; an LX signature alone does not imply 32-bit guest code.

## Evidence from the supplied GA CMD.EXE

The three objects are:

```
object 1  base 00010000  size 0000C6A8  flags 00001005  16-bit
object 2  base 00020000  size 00007EFA  flags 00001005  16-bit
object 3  base 00030000  size 00007790  flags 00001043  16-bit
```

None has object flag `0x2000` (Big/Default), so code defaults to 16-bit operand
size. The entry point, object 2 + `44FA`, independently confirms it:

```asm
44fa: fc                 cld
44fb: a3 72 0e           mov  [0e72],ax
44fe: 89 1e 70 0e        mov  [0e70],bx
4502: 49                 dec  cx
...
452b: 9a 00 00 00 00     call far 0000:0000   ; loader fixup site
```

See `analysis/CMD_200_GA_ENTRY_i8086.txt` for a longer entry disassembly.

## Full fixup inventory

`os2host32 --scan CMD.EXE` now parses the fixups instead of stopping at the
first unsupported source type:

```
177 fixup records total
278 external fixup sites
176 PTR16:16 + alias records / 276 sites
  1 OFF16 record / 2 sites
95 distinct ordinal imports
 2 distinct named imports
```

The named imports are:

```
SESMGR.DOSSMSETTITLE    6 sites
SESMGR.DOSSMPMPRESENT   1 site
```

The imported module set is:

```
SESMGR DOSCALLS KBDCALLS VIOCALLS NLS QUECALLS MSG
```

`analysis/CMD_200_GA_IMPORTS.tsv` annotates 91 of the 95 ordinal imports using
the previously reconstructed 6.64 CMD import map. This confirms very strong API
continuity between the earlier NE command interpreter and this GA LX image.

## Loader changes

`os2host32` gains a general `--scan` mode and a broader fixup parser for:

- byte, selector, 16:16 pointer, 16-bit offset, 16:32 pointer, 32-bit offset,
  and 32-bit relative source types;
- the LX alias flag and source lists;
- ordinal and named imports;
- additive-field parsing.

The proven direct-execution path is intentionally unchanged. Flat-32 images
such as `hi.exe`, `args.exe`, and `exec-test.exe` still report:

```
Execution model : flat 32-bit LX objects
Direct host path: supported
```

For GA CMD, `--scan` reports:

```
Execution model : contains 16-bit LX objects/selectors
Direct host path: needs additional execution machinery
```

`--run` now refuses such an image explicitly rather than failing on an opaque
fixup error or accidentally jumping 16-bit bytes under a 32-bit CS.

## What this means for CMD

The current Windows-11 target cannot simply jump into GA CMD. A real path needs
one of two mechanisms:

1. **16-bit protected-mode execution inside OS2HOST**: selector/LDT semantics,
   16-bit CS/SS/DS, far calls/returns, 16:16 pointers, and thunks from legacy
   DOSCALLS/KBD/VIO APIs into the 32-bit compatibility layer. On x64 Windows,
   this needs emulation/translation rather than relying on native NTVDM.

2. **Widen/reconstruct CMD into flat 32-bit code**. The Dec-1991 NT CMD is a
   particularly valuable sibling: a crude printable-string comparison already
   finds 99 exact strings shared with GA CMD, including command names, parser
   syntax, runtime messages, and shell diagnostics. That is not proof of
   function identity, but it is strong evidence that the NT binary can be used
   as a semantic/structural Rosetta Stone while widening the OS/2 shell.

For the overall OS2SS goal, option 2 is the shorter route to a command prompt;
a 16-bit execution engine remains valuable later for NE and mixed-LX support.

## Commands

```
os2host32 --info CMD.EXE
os2host32 --scan CMD.EXE
```

The existing flat-32 tests remain:

```
os2host32 --run hi.exe
os2host32 --run args.exe one two three
os2host32 --run exec-test.exe
```
