# Soft386 NE-H3Q — first functional historical MSG.2 bridge

Input: frozen H3P discovery archive. Changed only Soft386 runtime/Makefile and reference/16bitdiscovery notes. No system DLL implementation changed.

- Dispatch `MSG.2` / DosTrueGetMessage by resolved NE module name and ordinal.
- 26-byte 16-bit Pascal argument cleanup, derived from OS2.LIB message thunk and H3P guest frame.
- Segmented buffer, output length and path marshalling into unchanged common MKMSGF v0/v2 decoder.
- File failures are real errors; bound message segments and substitution-table insertion are deferred.
- Adds common/msg/os2_msg.c to Soft386 build, no changes to the common implementation.

Diagnostic Linux build: `make CFLAGS='-O0 -std=gnu99 -Wall -Wextra -Wno-unused-function -Wno-misleading-indentation'`. `make check-ne` and `make check-quick` passed. Windows runtime of LINK386 not run because LLIBCE.LIB is unavailable.

On Windows: `soft386_os2.exe --trace-hc --run <path>\\LINK386.EXE huge.obj > link-h3q.log 2>&1` with LLIBCE.lib and OS2.lib on its file search path. Preserve any generated huge.exe even if it is zero bytes. Examine first MSG.2 trace and next guest call or error.
