# Soft386 R3 Windows acceptance

Use the same 32-bit RosBE/i386 compiler and native DLL set that proved R2C.

## Build

From the repository root:

```bat
make MINGW=gcc soft386-system-win32
```

## Clean Life screen

Normal scheduler context switches are no longer printed unless `--trace-hc` or
`--trace-sched` is selected:

```bat
cd soft386
soft386_os2.exe ^
  --doscalls-dll ..\DOSCALLS.dll ^
  --viocalls-dll ..\VIOCALLS.dll ^
  --kbdcalls-dll ..\KBDCALLS.dll ^
  --sesmgr-dll ..\SESMGR.dll ^
  \OS2\demos\lifeos2.exe
```

## CMD32 external child

Start CMD32 exactly as above.  Then from the guest prompt:

```text
[C:\os2\demos>]nlsinfo.exe
[C:\os2\demos>]lifeos2.exe
```

Expected behavior is a new Soft386 process vessel for the child, synchronous
wait, then return to the original CMD32 prompt with the child's exit status.

For host-side process tracing before starting the parent:

```bat
set OS2_TRACE_EXEC=1
```

`--trace-native` will additionally show marshalled DOSCALLS.283/.280 calls.

## Pipeline acceptance

R3's fixture generator supplies two real LE executables:

* `tests\fixtures\pipe-producer-soft386.exe`
* `tests\fixtures\pipe-consumer-soft386.exe`

Generate them if needed:

```bat
python tests\mkfixtures.py tests\fixtures
```

From CMD32, use full paths or copy both fixtures into a directory on the OS/2
PATH, then run:

```text
pipe-producer-soft386.exe | pipe-consumer-soft386.exe > pipe-r3.txt
type pipe-r3.txt
```

Expected file contents:

```text
SOFT386_PIPE_PASS
```

This proves concurrent `EXEC_ASYNCRESULT` workers, inherited pipe endpoints,
nested Soft386 process vessels, `DosWaitChild`, and redirected child stdout.
