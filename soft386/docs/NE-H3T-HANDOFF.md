# Soft386 NE-H3T: synchronous 16-bit process bridge (experimental)

Baseline: NE-H3S. Source-only changes are limited to soft386/src/soft386_os2.c.

## Evidence

The RC.EXE 2.01.02 NE fixups import DOSCALLS.144 (DOSEXECPGM) and DOSCALLS.61 (DOSDUPHANDLE) according to the SDK OMF ordinal map. The prior trace shows RC.EXE writing an intermediate command file and failing its RCPP launch; it also shows unsupported DOS16.61. A proper subprocess implementation should operate through DOSCALLS.144, not hard-code the RC filename.

## H3T implementation

- DOSCALLS.144: documented seven-argument 16-bit Pascal call (24 argument bytes); validates guest segmented program/result pointers; accepts EXEC_SYNC only. Uses OS2HOST32_LOADER (already published by the 32-bit vessel), starts a fresh Soft386 OS process in Win32 CreateProcessA with inherited standard handles/current working directory, waits, writes RESULTCODES. It appends .EXE to extensionless program names, searching the current directory in this first step.
- DOSCALLS.61: duplicates guest-owned HFILE via host dup, with guest handle bookkeeping. The 16-bit six-byte Pascal argument layout is used.
- No core or system DLL changes. Existing 32-bit child bootstrap preserved.

## Verified

Linux diagnostic compilation (GNU99, O0) PASS; make check-ne PASS; make check-quick PASS including existing 32-bit bridges. The genuine Windows RC->RCPP execution path is NOT yet verified: RCPP.EXE itself was not supplied as an attachment. The uploaded RC.EXE is available for inspection, but without its RCPP.EXE companion and .RC sample there is no genuine complete resource-compile oracle.

## Outstanding

- Confirm RCPP.EXE format (NE, LE/LX, or DOS MZ) and test native Windows launch.
- Implement OS/2 guest PATH search and robust Win32 command-line quoting, custom guest environment strings, inherited non-stdio HFILE semantics, async modes and child PID lifecycle.
- Inspect RC's exact DOSCALLS.144 call frame with --trace-hc. If the trace never shows DOS16.144, investigate the actual runtime launch path rather than claiming it works.
- Implement 16-bit DOSCALLS.74/.83 and other import calls only when exercised.
