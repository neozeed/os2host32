# Soft386 R5D — PM font queries and control callback handles

2026-10-05. Incremental update from R5C. Changes are confined to `soft386/`.
The native DLL sources, common backends, native loader and WHP are unchanged.

## Install and test

Extract the Windows update ZIP and copy `soft386_os2.exe` into your existing
`C:\OS2` installation. Keep your installed service DLLs and `env.cmd`.
The update ZIP intentionally contains no replacement service DLLs.

Run from the same directory as the successful native-host test:

```bat
cd /d C:\OS2
call env.cmd
soft386_os2.exe --max-cycles 0 --run demos\NEKO.EXE
soft386_os2.exe --max-cycles 0 --run demos\telnetpm\telnetpm.exe
```

Keep the existing TELNET configuration and ETC environment setting.
No Python is needed to use the prebuilt executable.

The supplied executable is Win32 x86, built with Zig/Clang targeting
`x86-windows-gnu`, as in R5C. It requires Universal CRT API-set DLLs.
For an older Windows/ReactOS installation without that CRT, build from source
using your usual `i686-w64-mingw32-gcc`:

```sh
make -C soft386 win32
```

## What changed

* `GpiQueryFonts` now accepts zero-capacity count-only queries with zero stride
  and NULL metrics. Positive-capacity calls still require a valid metrics
  buffer, positive stride, nonnegative count and bounded allocation. Rejected
  font calls return -1 rather than falsely reporting zero remaining fonts.
* Native `SM_SETHANDLE` / `SM_QUERYHANDLE` callbacks can now enter guest window
  procedures. Incoming icon handles become guest tokens; guest return tokens
  are checked and translated back to native icon handles. NULL is supported;
  wrong-kind tokens and arbitrary guest values cannot escape as host handles.
  This is the icon/pointer control path, not general bitmap-control support.
* `--trace-native` now records direct guest sends and saved-procedure calls,
  including message numbers, arguments and saved-procedure results. Rejected
  PM calls include all guest arguments. Selected font, pointer-position,
  geometry, timer and subclass calls include their results.

## What the supplied traces establish

TELNETPM reached window creation and its message loop, but its first
PMGPI.586 call was rejected. Disassembly of the exact supplied executable
confirms a call at guest 0x14ff7 with public-font flags=1, capacity=0,
stride=0, metrics=NULL, followed by a check that skips initialization if the
returned count is nonpositive. The next call uses 228-byte font metrics.
The fixed boundary test exercises exactly that count-then-fetch pattern.

NEKO loaded sprite resources and received timer and paint callbacks. The
source review identified the missing reverse icon callback translations and
R5D adds them. However, a missing native WinSendMsg trace line is not by itself
proof that a send disappeared: Soft386 directly dispatches guest-to-guest
sends, and its saved-procedure route previously had no trace output. R5D
makes both routes visible. NEKO's visible cat is still a live-test gate.

The NEKO runs also used different profile files:

* Native: `C:\OS2\neko.ini`, requested timer interval 200 ms.
* Soft386: `C:\tmp\jar\os2host32-SOFT386-R5B\soft386\neko.ini`, interval 0 ms.

Use the same working directory/profile for comparison. R5D does not delete,
reset or rewrite an INI, or silently change the requested timer interval.
Three rejected slider sends in the earlier trace correspond to native
message 0x000A. Their pointer-bearing payload is not enabled by this release;
the new argument logging will distinguish these from icon-path failures.
The native baseline trace contains the same failed pointer-resource load for
resource 1000; that event alone does not distinguish the two runs.

## Validation

* Full `make check` passes, including earlier CPU/FPU, loader, threads, DLL,
  callback, network, queue and PM regressions.
* Expanded PM boundary tests cover font count-only and 228-byte fetch calls,
  invalid counts/strides/pointers, NULL icons, wrong handle kinds and rejected
  untyped pointer messages.
* The real Tiny386 PM runtime fixture now drives native icon set/query
  callbacks through original guest subclass code and its saved native
  procedure, and checks the round-trip native handle values.
* AddressSanitizer/UndefinedBehaviorSanitizer PM checks pass. Leak scanning
  was disabled because LeakSanitizer cannot inspect this container's task
  state; no leak-check pass is claimed.
* Win32 x86 cross-build passes. No Windows GUI or network acceptance run is
  claimed for this release. TELNETPM rendering/connection and NEKO animation
  remain to be verified on the Windows machine.

Evidence is in `soft386/validation/r5d/`. This is an incremental compatibility
update, not a claim of full OS2HOST32 feature parity.

## If either window is still wrong

Use the updated executable from `C:\OS2`, with both tracing layers enabled:

```bat
cd /d C:\OS2
call env.cmd
set OS2_PM_TRACE=1
soft386_os2.exe --max-cycles 0 --trace-native --run demos\NEKO.EXE 2>neko-r5d.txt
soft386_os2.exe --max-cycles 0 --trace-native --run demos\telnetpm\telnetpm.exe 2>telnet-r5d.txt
set OS2_PM_TRACE=
```

A short startup run is enough initially; close the application or press Ctrl+C
if it stays invisible. The new log records the previously hidden PM paths.
