# Soft386 R1 Windows/RosBE test plan

## Build

From the repository root with a normal MinGW name:

```bat
make soft386-native-win32
```

With RosBE where the compiler is exposed as `gcc`:

```bat
make MINGW=gcc soft386-native-win32
```

This builds the two sides independently:

* `DOSCALLS.dll` — existing native/common Win32 personality
* `soft386\soft386_os2.exe` — Tiny386 guest CPU/kernel + marshaller

If the DLL is not in the Windows loader search path, give the path explicitly:

```bat
cd soft386
soft386_os2.exe --doscalls-dll ..\DOSCALLS.dll --trace-native --trace-hc tests\fixtures\native-file-soft386.le
```

Expected stdout:

```text
soft386: native DOSCALLS file bridge PASS
```

The trace should contain marshalled calls for 273/282/256/281/257/259 and end
with `rc=0`.  The temporary `soft386-r1-native.tmp` file should be deleted by
the guest before exit.

## Re-test historical hi.exe through the DLL seam

The exact historical specimen previously proven under Soft386 R0 can now be
run with native HFILE services while memory/thread state remains jar-owned:

```bat
soft386_os2.exe --doscalls-dll ..\DOSCALLS.dll --trace-native --trace-hc C:\c386\hi.exe
```

Expected application output remains:

```text
hi!
```

and the program's `DosExit(EXIT_PROCESS,4)` should still produce host exit code
4.  With the bridge active, 224/256/282 are expected to show `[marshalled]` in
the native trace; 299/304/305 remain jar calls and must **not** show as native.

## Jar-kernel tests on Windows

These do not require `DOSCALLS.dll` and are useful isolation checks:

```bat
soft386_os2.exe --no-doscalls-dll --trace-hc tests\fixtures\memory-soft386.le
soft386_os2.exe --no-doscalls-dll --trace-hc tests\fixtures\sync-soft386.le
soft386_os2.exe --no-doscalls-dll --trace-hc tests\fixtures\thread-soft386.le
```

Expected output includes:

```text
soft386: jar memory semantics PASS
soft386: jar mutex/event worker PASS
soft386: jar semaphore main resumed PASS
soft386: worker thread ran
soft386: main resumed after DosWaitThread
```
