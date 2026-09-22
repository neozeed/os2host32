# M25 I/O execution boundary

The recovered parser already emits `CMD_NODE_PIPE` and per-command redirection
lists. M25 leaves that syntax layer unchanged and adds execution beneath it.

## Redirection path

    cparse AST
       -> apply_redirections()
       -> CRT fd 0/1 + Win32 STD_INPUT/STD_OUTPUT
       -> built-in OR DosExecPgm
       -> DOSCALLS.283 worker
       -> inherited STARTF_USESTDHANDLES
       -> os2host32
       -> guest DOSCALLS handle 0/1
       -> DosRead / DosWrite

This intentionally keeps CRT and Win32 handle state synchronized because the
native CMD32 built-ins and the LX guests consume different layers of the host
I/O stack.

## Pipeline path

    left AST  -> cmd32os2 -c ... --stdout--> [anonymous pipe]
    right AST <- cmd32os2 -c ... --stdin--- [anonymous pipe]

Each side runs in a separate host process. This avoids concurrent mutation of
process-global C runtime / Win32 standard handles in the resident shell.

Before creating the left child, the read end is marked non-inheritable. Before
creating the right child, the write end is marked non-inheritable. This is
required for the right side to observe EOF after the left side exits.

M25 reports the rightmost pipeline process exit code as the pipeline status.
