# Soft386 R2 Windows acceptance

## Build

From the repository root under RosBE:

```bat
make MINGW=gcc soft386-system-win32
```

This builds the unchanged existing native service DLLs plus Soft386:

* DOSCALLS.dll
* VIOCALLS.dll
* KBDCALLS.dll
* SESMGR.dll
* soft386\soft386_os2.exe

## 1. Retain R1 proof

```bat
cd soft386
soft386_os2.exe --doscalls-dll ..\DOSCALLS.dll --trace-native --trace-hc tests\fixtures\native-file-soft386.le
```

Expected marker: `soft386: native DOSCALLS file bridge PASS`.

## 2. Inspect real C/386 binaries

```bat
soft386_os2.exe --check C:\path\cmd32os2_os2.exe
soft386_os2.exe --check C:\path\nlsinfo.exe
```

CMD32 should report recognized C/386 bridges for KBDCALLS.4/.13 and
VIOCALLS.7/.9/.15/.19/.21. NLSINFO should additionally expose the VIO helpers
used by its richer console surface. No 16-bit guest code is executed by
`--check`.

## 3. NLSINFO

```bat
soft386_os2.exe ^
 --doscalls-dll ..\DOSCALLS.dll ^
 --viocalls-dll ..\VIOCALLS.dll ^
 --kbdcalls-dll ..\KBDCALLS.dll ^
 --sesmgr-dll ..\SESMGR.dll ^
 --trace-native --trace-hc ^
 C:\path\nlsinfo.exe
```

The NLS DOSCALLS ordinals 289/291/395/396/397 should remain jar/common calls;
VIO/KBD calls should show `native ... [marshalled]` traces.

## 4. CMD32 builtin

```bat
soft386_os2.exe ^
 --doscalls-dll ..\DOSCALLS.dll ^
 --viocalls-dll ..\VIOCALLS.dll ^
 --kbdcalls-dll ..\KBDCALLS.dll ^
 --sesmgr-dll ..\SESMGR.dll ^
 --trace-native --trace-hc ^
 C:\path\cmd32os2_os2.exe -c "echo SOFT386_CMD32_PASS"
```

This deliberately starts with a builtin. CMD32 imports DosExecPgm/DosWaitChild,
but Soft386 process orchestration is not an R2 claim yet.

## Known first-pass keyboard limitation

A native blocking KbdCharIn/KbdStringIn blocks the one host thread that runs
Tiny386. Interactive CMD is useful as an acceptance experiment, but the next
scheduler refinement should convert wait-mode keyboard input into nonblocking
poll + virtual-thread deschedule/wakeup.
