# Soft386 R4 Windows test

Use the same 32-bit RosBE/i386 or i686 MinGW environment that proved R3.  Do not
build the native personality bridge as Win64.

## Build

From the repository root:

```bat
make MINGW=gcc soft386-system-win32
```

or from `soft386`:

```bat
make clean
make
```

when `gcc -dumpmachine` is the working 32-bit MinGW/RosBE compiler.

## Synthetic x87 checks

```bat
soft386_os2.exe tests\fixtures\fpu-soft386.le
soft386_os2.exe --trace-sched tests\fixtures\fpu-thread-soft386.le
```

Expected output:

```text
soft386: 80387 x87 math PASS
soft386: per-thread 80387 state PASS
```

## Phoon acceptance

Run Phoon with the same personality DLL set used for the live R3 tests:

```bat
soft386_os2.exe ^
  --doscalls-dll ..\DOSCALLS.dll ^
  --viocalls-dll ..\VIOCALLS.dll ^
  --kbdcalls-dll ..\KBDCALLS.dll ^
  --sesmgr-dll ..\SESMGR.dll ^
  \OS2\demos\phoon\phoon.exe
```

R4 success is visual/behavioural: the previous `sin/cos/sqrt/asin: DOMAIN error`
stream should disappear and Phoon should render/refresh normally rather than
running until Soft386's cycle limit.

If it still fails, rerun only with `--trace-hc` and capture the final output.  Do
not enable scheduler tracing for a normal VIO screen test because trace output
shares the host console.

## Known limitations

The Tiny386 FPU source is intentionally incomplete and is not a bit-perfect 387.
Passing Phoon establishes practical x87 compatibility for that workload, not full
IEEE/x87 exception or 80-bit precision conformance.
