# M28A OS/2 API boundary

The M28A rule is: command semantics should no longer know how Windows performs
filesystem/process operations.

## CMD modules now host-neutral for file I/O

`cmdfile.c` contains no `windows.h` include and no Win32 file API references.
It uses only `cmdos2.h` for:

- open/read/write/close
- delete/move
- wildcard enumeration
- path attributes

`cmd32os2.c` also routes TYPE, DIR, CD, MD, RD, batch EXIST, executable lookup
and external DosExecPgm through `cmdos2.h`.

## Native bootstrap binding

`cmdos2_win32.c` is deliberately allowed to know that the compatibility layer
is a Windows DLL. It resolves these historical 32-bit DOSCALLS ordinals:

| Ordinal | API |
|---:|---|
| 223 | DosQueryPathInfo |
| 226 | DosDeleteDir |
| 255 | DosSetCurrentDir |
| 257 | DosClose |
| 259 | DosDelete |
| 263 | DosFindClose |
| 264 | DosFindFirst |
| 265 | DosFindNext |
| 270 | DosCreateDir |
| 271 | DosMove |
| 273 | DosOpen |
| 274 | DosQueryCurrentDir |
| 275 | DosQueryCurrentDisk |
| 281 | DosRead |
| 282 | DosWrite |
| 283 | DosExecPgm |

The host implementation remains in `DOSCALLS.dll`.

## LX backend seam

`cmdos2_os2.c` implements the same `cmdos2.h` interface using direct `Dos*`
imports. It is not part of the native build yet; it is the backend intended for
C/386 + LINK386 when enough remaining CMD code is host-neutral.

## Remaining direct Win32 in cmd32os2.c

The direct calls left in the shell are intentionally concentrated in three
areas to migrate next:

1. pipeline worker spawning / standard-handle switching,
2. environment snapshots used by SETLOCAL/ENDLOCAL,
3. console presentation/input (CLS/PAUSE and native stdio bridge).

Those map naturally to DOSCALLS handle/pipe APIs plus VIOCALLS/KBDCALLS.
