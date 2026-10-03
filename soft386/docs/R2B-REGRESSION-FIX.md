# Soft386 R2B regression correction

## Live symptom

On Windows, both historical `hi.exe` and `nlsinfo.exe` stopped at the first native `DOSCALLS.224 DosQueryHType` after R2 eagerly loaded DOSCALLS, VIOCALLS, KBDCALLS and SESMGR.  R1 had already live-proven the exact DOSCALLS bridge and the R2 DOSCALLS bridge source plus DOSCALLS/common sources are byte-identical to R1.

## R2B changes

1. VIOCALLS/KBDCALLS/SESMGR are loaded only if the parsed LE/LX import-module table names the module.
2. DOSCALLS-only programs no longer load unrelated personality DLLs merely because the files are present or paths were supplied.
3. `_WIN64` builds are rejected at compile time; the current native DLL bridge is explicitly 32-bit x86.
4. The Makefile rejects an x86_64 MinGW `CC` with a clear message.
5. The runtime banner prints the host pointer width.
6. `--trace-native` prints the per-image system-module mask.
7. Native DOSCALLS.224 tracing prints a post-call return marker.  If the pre-call marker appears but the return marker does not, the fault occurred inside the native DLL call.

## Immediate isolation commands

For a pre-R2B tree, the equivalent DOSCALLS-only isolation is:

```bat
soft386_os2.exe --doscalls-dll ..\DOSCALLS.dll --no-system-dlls --trace-native --trace-hc \c386\hi.exe
```

If that works, eager system-DLL loading was the trigger.  If it still stops inside DOSCALLS.224, verify that both Soft386 and the native DLLs were built as 32-bit x86; do not use `C:\msys64\mingw64` for this milestone.

Expected R2B DOSCALLS-only startup includes:

```text
soft386: image system-module mask=0 (VIO=no KBD=no SES=no)
soft386: native system bridges VIO=OFF KBD=OFF SES=OFF
```
