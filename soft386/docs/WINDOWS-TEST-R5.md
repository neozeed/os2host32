# Soft386 R5 Windows acceptance

This is a cross-built candidate. Headless checks passed; a real Windows PM run
is the remaining acceptance step. No Python, SDK compiler or Hyper-V is needed
for the supplied smoke kit.

## Ready-to-run kit

Extract `soft386-R5-win32-smoke.zip` into a new directory and run:

```bat
RUN-R5.cmd
```

It checks LE/LX DLL imports, dynamic module APIs and legacy DLL initialization,
then launches `pmjar-soft386.exe`. Expected final guest line:

```text
Soft386 R5 PM/DLL acceptance PASS
```

The PM window may only flash briefly: this is an automatic test, and a guest
worker posts the final message after the first paint. To keep the same window
visible for five seconds, run:

```bat
VIEW-R5.cmd
```

A real native window should show red text, "Soft386 PM: DLL window procedure in
the jar", on a white background, then close automatically. The DLL initializer,
window procedure, nested sends, drawing calls, worker thread and terminator all
execute through the jar. `PMJAR.DLL` is an LX guest DLL, not a native PE DLL.

The trace files are `soft386-r5-pm.trace` and `soft386-r5-view.trace` in the kit
root. If a check fails, retain its errorlevel and trace. The test checks GPI's
actual success convention (`GpiCharStringAt` returns GPI_OK=1).

## Build from source

From the repository root, with an i686 MinGW cross compiler:

```sh
make DOSCALLS.dll PMWIN.dll PMGPI.dll PMCTLS.dll MSG.dll
make -C soft386 win32
```

Or from a native 32-bit MinGW/RosBE shell:

```sh
make -C soft386 CC=gcc WINCC=gcc win32
```

The second command expects the native service DLLs to have already been built
with the root makefile. Generated guest fixtures are included in the source
snapshot and smoke kit; Python is only needed to regenerate them or run the
host regression driver. The SDK's historical C compiler is not required.

Run `soft386\tests\run-r5.cmd` from the source tree. The script locates the
repository root so the native DLLs and the `soft386` executable can be found.

## Existing applications and sessions

Keep native DLL filenames intact. Put guest DLLs beside the guest executable,
or set `OS2LIBPATH` to their directories. Start an existing flat32 PM example:

```bat
soft386\soft386_os2.exe --max-cycles 0 --trace-hc --trace-native path\pm-example.exe 2>pm-example-r5.trace
```

Use the same R5 executable beside the existing VIO/KBD/SESMGR DLLs when testing
CMD32 and `START /PM`. The previous child/session launch route is retained and
re-enters R5. Explicit PM DLL path options propagate to child jars:

```text
--pmwin-dll PATH
--pmgpi-dll PATH
--pmctls-dll PATH
--msg-dll PATH
```

Environment equivalents are `SOFT386_PMWIN_DLL`, `SOFT386_PMGPI_DLL`,
`SOFT386_PMCTLS_DLL` and `SOFT386_MSG_DLL`. `--no-system-dlls` disables PM as well
as VIO/KBD/SESMGR. `--check` loads/fixes the guest dependency graph without
executing code or loading native PM.

The acceptance kit intentionally contains only the native DLLs used by these
fixtures. CMD32 still needs the existing VIO/KBD/SESMGR DLLs from the full build.
R5 initially supports one PM queue owner per jar. See the handoff for limits;
this is not a claim that all native os2host32 PM examples already run unchanged.
