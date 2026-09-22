# M29B/M29B2 - Microsoft C/386 `_far16` VIO bridge

M29B proves that an unmodified 1991 Microsoft C/386 program can use its
compiler-generated 32->16 OS/2 migration thunk on a Win64 machine without
executing any 16-bit protected-mode code.

The specimen is the 9,227-byte `far3.exe` produced by Microsoft C/386
6.00.081 and LINK386 1.01.015 after supplying these missing imports in a DEF
file:

```
DosFlatToSel=DOSCALLS.425
VIOWRTTTY=VIOCALLS.19
```

Its LE fixup graph contains one 16-bit object, a `PTR16:16 + ALIAS` import to
`VIOCALLS.19`, a `PTR16:32` return to the 32-bit object, and a `SEL16 + ALIAS`
transition from the 32-bit helper.  OS2HOST32 validates that graph and the
known C/386 transition-helper instruction sequence before permitting direct
execution.

Instead of allowing the helper to execute `LSS SP` and enter the 16-bit
object, OS2HOST32 patches the helper entry to a generated 32-bit stub.  The
stub receives the original far-Pascal frame:

```
ESP+0   32-bit return address
ESP+4   USHORT hvio
ESP+6   USHORT count
ESP+8   packed far pointer token
```

It converts those to normal Win32 cdecl arguments and calls the existing
`VIOCALLS.dll` ordinal 19 implementation, then returns with `RET 8` to preserve
the Pascal callee-cleanup contract.

`DOSCALLS.425` is also supplied.  C/386 passes the flat pointer in EAX and
expects the converted pointer back in EAX.  For this intercepted path the
compatibility export intentionally leaves EAX unchanged, making the original
32-bit linear address the virtual far-pointer token.  This is safe only because
the patched helper never loads the token into a segment register.

## Build

Build the Win32 host and compatibility DLLs normally:

```
make
```

To recreate the historical probe with the recovered Microsoft toolchain:

```
build-c386-far16-vio.cmd
```

M29B2 drives LINK386 through `@m29b-far16-vio.lnk` rather than placing the
absolute C/386 library paths on the DOS command tail.  This matters when the
16-bit linker is launched through the RUN286 shim: a long command tail can
lose the final module-definition argument even though the `.def` file exists.

or test the included exact specimen:

```
cd examples
m29b-far16-vio-test.cmd
```

Expected scan tail:

```
C/386 far16     : recognized VIOCALLS.19 migration thunk
Direct host path: supported via native far16 bridge
```

Expected run payload:

```
hello from far16
```

## Deliberate limits

M29B does not implement a 286 protected-mode execution environment, real LDT
selectors, arbitrary 16-bit LE objects, or general NE execution.  Any mixed
image that does not match the exact supported migration pattern is rejected.
This gives us a safe base for adding KBD/VIO migration calls one proven ABI at
a time.
