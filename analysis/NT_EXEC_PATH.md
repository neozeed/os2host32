# Dec-1991 NT CMD external-execution seam

The COFF symbols identify the relevant shell routines in `cext.c`:

- `_ExtCom` — RVA `0x1594C`
- `_ECWork` — RVA `0x159AC`
- `_ExecPgm` — RVA `0x15B7C`
- `_SearchForExecutable` — RVA `0x15C80`
- `_DoFind` — RVA `0x164CC`
- `_ExecError` — RVA `0x16524`

The disassembly of `_ExecPgm` is retained in `nt_execpgm_disasm.txt`.
Its first process-launch call is the early NT CRT `_spawnl` at RVA `0x1CB58`.
Failure handling reaches `_ExecError`; the surrounding shell also retains named
process helpers such as `_WaitProc` and `_KillProc` in `ctools2.c`.

For CMD32-OS2 this is the key personality seam:

```text
Microsoft 32-bit shell/parser/batch logic
                 |
              _ExecPgm
                 |
        [replace NT spawn boundary]
                 |
          DOSCALLS.283 DosExecPgm
                 |
               OS2SS
                 |
             OS2HOST32
```

This is preferable to translating the 16-bit GA CMD instruction-by-instruction
merely to obtain a command shell, while still keeping the real OS/2 CMD as the
behavioral oracle for differences between the NT and OS/2 descendants.
