Soft386 R29 - unlimited cycles by default

Overlay soft386/src/soft386_os2.c onto the current os2host32 source tree,
or apply the included small patch (preferred if your tree has newer edits).

Default max_cycles is now 0 (unlimited), for both LE/LX and NE execution.
Explicit --max-cycles N remains supported. Help documents the default.

Rebuild with your existing 32-bit Windows build setup, e.g.:
  make -C soft386 win32
Replace the soft386_os2.exe selected by OS2HOST32_LOADER, including the
copy under %OS2PATH% if that is what CMD32 launches.
No WinTown rebuild, resource binding, DLL or marshalling changes needed.

Source update only; no Windows executable included.
