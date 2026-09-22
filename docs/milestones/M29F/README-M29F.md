# Milestone 29F — multiple C/386 `_far16 _pascal` migration thunks

M29F moves the far16 work from “one mixed-mode API per executable” to the shape
we actually need for a C/386-built command processor: **several classic 16-bit
VIO/KBD imports in one 32-bit LE image**.

The 286 fragments still are not executed.  OS2HOST32 treats them as loader
metadata, identifies each Microsoft C/386 migration helper from the LE fixup
graph plus its instruction signature, and patches each 32-bit helper to its own
native Win32 bridge.

## Why this milestone matters

M29B through M29E proved four useful Pascal frame shapes individually:

| Import | Frame (low address to high after return address) | Status |
|---|---|---|
| `VIOCALLS.19` `VioWrtTTY` | `USHORT hvio`, `USHORT count`, `far16 text` | proven |
| `KBDCALLS.4` `KbdCharIn` | `USHORT hkbd`, `USHORT wait`, `far16 info` | proven |
| `KBDCALLS.9` `KbdStringIn` | `USHORT hkbd`, `USHORT wait`, `far16 length`, `far16 buffer` | proven |
| `KBDCALLS.13` `KbdFlushBuffer` | `USHORT hkbd` | proven |

M29F puts all four imports into a single genuine Microsoft C/386 program.  This
is the first test of multiple 14-byte 16-bit migration fragments, multiple
32-bit transition helpers, and multiple native bridge patches in one module.

## Loader changes

`os2host32.c` now owns an array of `C386Far16Bridge` records rather than one
singleton bridge.  Detection expands and pairs the four migration records for
each helper:

```text
16-bit thunk fragment:

+0   9A ptr16:16        -> classic VIO/KBD import
     fixup begins +1

+5   66 67 EA ptr16:32  -> 32-bit continuation
     fixup begins +8
```

The 32-bit helper contributes:

```text
SEL16                   -> guest stack object
SEL16 + ALIAS           -> 16-bit thunk object
```

For each imported thunk, M29F pairs the `PTR16:16` source at `base+1` with the
`PTR16:32` source at `base+8`, then associates that return continuation with the
alias far jump in the 32-bit helper.  At load time it additionally verifies that
the alias jump's raw 16-bit offset points at the expected 14-byte fragment.

LINK386 is allowed to compress repeated selector fixups into LE source-list
records.  M29F expands those lists during recognition instead of assuming one
fixup record equals one thunk.

Only after **every** non-flat mixed-mode record has been paired with a supported
descriptor is the image accepted for native execution.

## Native bridge generation

The M29E descriptor engine remains unchanged in principle.  Each bridge knows
only the Pascal frame widths:

```text
VIOCALLS.19   2,2,4
KBDCALLS.4    2,2,4
KBDCALLS.9    2,2,4,4
KBDCALLS.13   2
```

OS2HOST32 generates one small cdecl adapter per migration helper and patches the
helper entry with a relative `JMP` to it.  `DosFlatToSel` remains a virtual-token
operation: because the helper is intercepted before `LSS` or the far jump, no
selector is ever loaded into a segment register and no 286 instruction executes.

## Historical multi-thunk probe

Build on the C/386 machine:

```cmd
cd C:\2\m29f
make
build-c386-far16-multi.cmd
```

The historical linker is still driven through a response file to avoid the
RUN286/DOS command-tail limit:

```text
link386 @m29f-far16-multi.lnk
```

The probe imports all four APIs at once:

```text
VIOCALLS.19   VioWrtTTY
KBDCALLS.4    KbdCharIn
KBDCALLS.9    KbdStringIn
KBDCALLS.13   KbdFlushBuffer
```

Scan it:

```cmd
os2host32 --scan c386-far16-multi-test.exe
```

The exact LINK386 record packing is intentionally discovered by the test, but a
successful scan should end approximately as:

```text
Execution model : contains 16-bit LE/LX objects/selectors
C/386 far16     : recognized 4 migration thunks
  VIOCALLS.19 ... VioWrtTTY ...
  KBDCALLS.4  ... KbdCharIn ...
  KBDCALLS.9  ... KbdStringIn ...
  KBDCALLS.13 ... KbdFlushBuffer ...
Direct host path: supported via native far16 bridges
```

Then run it:

```cmd
os2host32 --run c386-far16-multi-test.exe
```

It should flush pending keyboard input, ask for one key, then ask for a short
line.  Backspace in the line is a useful test.  At the end it reports both the
returned `KBDKEYINFO` and `STRINGINBUF`/text values.

There is also a wrapper:

```cmd
cd examples
m29f-far16-multi-test.cmd
```

M29F fixes the M29B–M29E example wrappers to locate `os2host32.exe` and their
guest relative to `%~dp0`; they no longer depend on the current directory or on
`os2host32` being in `PATH`.

## Local validation performed here

The host-independent C89 build succeeds, the original M29B VIO specimen is still
recognized through the new multi-bridge detector, and the parser, batch,
environment, file-boundary, pipeline-boundary and console-boundary regressions
all pass.

The actual Win32/C386 multi-thunk executable cannot be built or executed in this
environment, so the i3 remains the authoritative runtime test for this milestone.
