R0 registry configuration
=========================

The pinned stock ReactOS hive contains Optional = Posix.  install-r0.reg adds:

    Os2 = %SystemRoot%\system32\os2ss.exe   (REG_EXPAND_SZ)

and writes Optional as the REG_MULTI_SZ sequence:

    Posix
    Os2

Before applying it to any non-stock machine, export/back up the current SubSystems
key because the .reg file deliberately writes the complete Optional value.

rollback-r0.reg removes Os2 and restores the pinned stock Optional = Posix value.

R0 intentionally does NOT add Os2 to Required.
