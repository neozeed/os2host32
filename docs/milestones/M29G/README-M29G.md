# Milestone 29G — the real C/386 backend consumes the far16 bridges

M29F proved that one genuine Microsoft C/386 program can contain several
independent `_far16 _pascal` migration thunks and that OS2HOST32 can recognize,
validate and replace all of them without executing the 286 fragments.

M29G moves that machinery out of the synthetic compiler probes and into the
actual direct OS/2 backend, `cmdos2_os2.c`.

## What changed

The temporary M29A console fallbacks are gone for the three APIs whose historical
migration contracts are already proven:

| CMD backend operation | Historical API | Import |
|---|---|---|
| `CmdO2VioWrtTTY` | `VioWrtTTY` | `VIOCALLS.19` |
| `CmdO2KbdCharIn` | `KbdCharIn` | `KBDCALLS.4` |
| `CmdO2KbdFlushBuffer` | `KbdFlushBuffer` | `KBDCALLS.13` |

`CmdO2VioWrtTTY` therefore no longer routes console output through `DosWrite`,
`CmdO2KbdCharIn` no longer emulates keyboard input with `DosRead`, and
`CmdO2KbdFlushBuffer` is no longer a no-op.  Microsoft C/386 emits the real
32->16 migration helpers in the backend object and OS2HOST32 redirects those
helpers to the native VIOCALLS/KBDCALLS implementations.

No loader changes were needed for this milestone.  M29F's generalized bridge
recognizer is deliberately left untouched.

## KBDKEYINFO packing

The historical `KBDKEYINFO` wire object is two-byte packed and ten bytes long.
`cmdos2.h` is shared with the native bootstrap and intentionally does not impose
that compiler-specific packing on its public `CmdO2KbdKeyInfo` structure.

M29G therefore uses a private packed ten-byte structure inside `cmdos2_os2.c`,
passes that object through the far16 migration helper, and copies each returned
field into the public structure.  This keeps the backend boundary stable and
avoids depending on whichever padding the host compiler chooses.

## Link contract

The C/386-generated migration helpers reference `DosFlatToSel`, and the classic
VIO/KBD targets are imported by ordinal.  The M29G smoke/test DEF files make
those bindings explicit:

```text
DosFlatToSel= DOSCALLS.425
VIOWRTTTY=    VIOCALLS.19
KBDCHARIN=    KBDCALLS.4
KBDFLUSHBUFFER=KBDCALLS.13
```

`build-os2-backend.cmd` now uses that import contract for the existing direct
backend smoke executable and also builds a dedicated interactive probe.

## Build on the C/386 machine

From the extracted milestone directory:

```cmd
make
build-os2-backend.cmd
```

The second command builds the existing process/file/pipe smoke programs plus:

```text
cmdos2_os2_console_test.exe
```

The console test source contains **no `_far16` declarations**.  It only calls
`cmdos2.h`.  The thunks must therefore come from the real backend object rather
than from the test harness.

Scan it first:

```cmd
os2host32 --scan cmdos2_os2_console_test.exe
```

A successful M29G scan should report a mixed 16/32-bit image and three
recognized C/386 migration thunks, approximately:

```text
C/386 far16     : recognized 3 migration thunks
  VIOCALLS.19 VioWrtTTY       ...
  KBDCALLS.4  KbdCharIn       ...
  KBDCALLS.13 KbdFlushBuffer  ...
Direct host path: supported via native far16 bridges
```

Then run it:

```cmd
os2host32 --run cmdos2_os2_console_test.exe
```

It should print:

```text
M29G real cmdos2_os2 backend: press one key.
Key:
```

Press one printable key.  After the returned key fields it should finish with:

```text
M29G_OS2_CONSOLE_BACKEND_OK
```

The wrapper is also available as:

```cmd
examples\m29g-os2-backend-console-test.cmd
```

## Existing direct-backend smoke

`cmdos2_os2_smoke.exe` is still built by the same script.  Because
`cmdos2_os2.obj` now owns the three migration helpers, its LE image may also
contain the three VIO/KBD thunks even though the old smoke program does not call
those wrapper functions at runtime.  It should remain runnable through
OS2HOST32 and continue to pass its existing environment, file, pipe and process
checks.

## Deliberately not folded into M29G

The direct backend still leaves the `CLS`-related operations as stubs:

```text
CmdO2VioGetCurPos
CmdO2VioSetCurPos
CmdO2VioClear
```

The native VIOCALLS personality already has `VioGetCurPos`, `VioSetCurPos`,
`VioGetMode` and `VioScrollUp`, but their C/386 Pascal frame descriptors have
not yet been proven through the historical compiler.  Keeping them out of M29G
means this integration milestone tests only contracts already established by
M29B-M29F.

If the M29G backend probe passes on the i3, the natural next milestone is to
capture those remaining VIO thunk shapes and make `CLS` work on the genuine
direct C/386 backend as well.

## Local validation performed here

The host-independent parser, batch, environment, file-boundary,
pipeline-boundary and console-boundary checks still pass.  `os2host32.c` also
builds cleanly as a host C89 program, and the retained M29B `VioWrtTTY` fixture
is still recognized by the unchanged generalized far16 detector.

The historical C/386 compiler/LINK386 and Win32 runtime are not available in
this environment, so the i3 remains the authoritative build and execution test
for the new backend-owned thunks.


## M29G2 - prerelease C/386 startup globals

The Beta 2 `OS2386.LIB` predates the `DosGetInfoBlocks` import used by the
first M29G source.  The matching Microsoft C runtime already initializes two
startup globals that provide the same information needed by this backend:

- `environ[]` supplies the process environment strings;
- `_pgmptr` supplies the fully-qualified executing-program pathname.

M29G2 uses those runtime globals and therefore requires no new OS/2 import or
selector bridge.  This is especially appropriate under OS2HOST32 because its
startup shim already constructs the contiguous OS/2 environment/program/argv
area expected by the original C/386 CRT.
