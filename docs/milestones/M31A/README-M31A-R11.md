# M31A R11 - WMCHAR metrics/CRT proof + guest-text isolation

R10 established that the untouched Beta-2 WMCHAR reaches its first dynamic
WinDrawText after a keypress, but the stack buffer supplied by WMCHAR contains
one byte 0x16 instead of the expected decimal message string.  The same trace
also showed character-cell geometry inconsistent with the metrics reported by
PMGPI.

R11 is intentionally diagnostic plus one architectural hardening change:

* trace the actual Beta-2 FONTMETRICS fields used by WMCHAR;
* at WinRegisterClass, dump WMCHAR's relocated gcyChar/gcxAveChar/gcxMaxChar
  globals and the relocated "%u." format string;
* extend loader fixup tracing to the metric globals, "%u." format pointer,
  sprintf's private FILE object, and the existing WMCHAR relocation proofs;
* WinDrawText copies the explicit-length guest text to host-owned storage before
  calling GDI.  No guest memory pointer is handed directly to ExtTextOutA.

Run with:

    set OS2_PM_TRACE=1
    os2host32.exe --run WMCHAR.EXE

Then press a single printable key.
