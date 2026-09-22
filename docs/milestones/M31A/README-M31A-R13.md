# M31A R13 - activate native PM stack headroom

R12's trace proved the WMCHAR metrics globals are correct immediately after
GpiQueryFontMetrics but are already corrupted before the first WinGetMsg.
The historical WMCHAR stack/data object is only 0x2C80 bytes (~11 KB), with
live globals around object 2 + 0x0910..0x0BDE.  Native Win32 USER32/GDI calls
run synchronously on that same x86 stack and can grow down into those globals.

The intended R12 stack-promotion implementation existed in the working tree,
but the R12 ZIP was accidentally packaged from the pre-promotion copy.  R13
is the corrected package and actually wires pm_runtime_stack_size() into
map_objects().

For qualifying non-DLL PM executables whose initial ESP is exactly the end of
a small stack object, the runtime mapping is extended to 256 KB and initial
ESP is moved to object + 0x40000.  The original LE/LX object size, data pages,
fixups, preferred addresses, and guest globals are unchanged.  Promotion is
refused if the extension would overlap another object.

WMCHAR should print at startup:

    M31A PM stack   : promoted object 2 ESP 00002C80 -> 00040000 (256 KB runtime stack span)
    M30D TIB stack   : 01020000..01060000 (256 KB)

Sarien is intentionally unaffected because its historical stack object is
already about 383 KB.

Build and run:

    make clean
    make tools compat
    make m31a-wmchar-check
    set OS2_PM_TRACE=1
    os2host32.exe --run WMCHAR.EXE

The first behavioral test is simply to resize the window and type several
letters/arrows/function keys.  With adequate native stack headroom the metrics
globals should remain gcy=16 ave=7 max=14 across WinCreateStdWindow,
ShowWindow, painting, and keyboard dispatch.
