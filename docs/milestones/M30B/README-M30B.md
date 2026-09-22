# Milestone 30B - EMX bound executable self-inspection trace

M30A proved that the supplied EMX 0.9d `emx.dll` can be loaded through the
native mixed-mode bridge. M30A2 then transferred control into a real,
emxbind-produced 32-bit LX program. The minimal `hi.exe` runs on real OS/2
2.00 but under OS2HOST32 reaches EMX startup and prints:

    Invalid option in .exe file

M30B deliberately does **not** guess at a fix. It instruments the OS/2 file
and module APIs used while EMX inspects its own bound executable.

## New diagnostic switch

    OS2_TRACE_EMX_SELF=1

With it enabled, DOSCALLS logs:

* `DosQueryModuleName` - returned module name and its host full-path expansion
* `DosOpen` - path, expanded path, mode/action flags, resulting HFILE and size
* `DosQueryFileInfo` - return code, reported size, first structure bytes
* `DosSetFilePtr` - distance, origin and resulting file position
* `DosRead` - position before read, requested/actual count, position after read,
  and the first 16 bytes returned
* `DosClose` - OS/2 HFILE and native handle

The older `OS2_TRACE_IO=1` trace is also enabled by the supplied test script.
No EMX bridge semantics are intentionally changed in this checkpoint.

## Build

Using the existing MinGW/i686 build environment:

    make clean
    make tools compat

or use the same build process that was used for M30A2.

## Test

Place the exact known-good EMX files in this directory:

    emx.dll
    hi.exe

where `hi.exe` is the executable produced by:

    gcc -c hi.c
    gcc hi.o -o hi
    emxbind hi

and already verified to print `hi` on OS/2 2.00.

Then run:

    m30b-emx-hi-test.cmd

The critical output begins around `GUESTMOD QUERYNAME` / `M30B SELF:` and
continues through `Invalid option in .exe file` (or, hopefully, `hi`).

## What this is trying to distinguish

The next trace should tell us whether EMX is failing because:

1. `DosQueryModuleName` returns a relative name where real OS/2 supplies a
   qualified path;
2. `DosOpen` opens a different file than EMX intended;
3. the file size/query structure differs from OS/2;
4. a seek origin/offset or returned new position is wrong; or
5. the correct offset is reached but the bytes returned by `DosRead` differ
   from the bytes actually stored in the emxbind trailer/options area.

Once one of those is demonstrated, M30C can make the smallest semantic fix
rather than changing the mixed-mode bridge blindly.
