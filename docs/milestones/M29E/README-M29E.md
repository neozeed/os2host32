# M29E - descriptor-driven C/386 `_far16` bridge + KbdFlushBuffer

M29D proved that the Win32 replacement bridge is not limited to a single
pointer shape: `KbdStringIn` successfully carried two independent far16
pointers and returned edited data through both the character buffer and
`STRINGINBUF`.

M29E turns that evidence into infrastructure.  The native x86 stub generator
is now driven by a small API descriptor table instead of having separate
machine-code branches for the 8-byte and 12-byte Pascal frames.

Each descriptor records the argument widths **as they appear in memory when
the C/386 transition helper is entered**.  Because `_pascal` pushes arguments
left-to-right, the rightmost source argument is nearest the return address.
Reading that frame from low address to high and pushing each value constructs
the ordinary right-to-left Win32 cdecl argument list.

The currently proven descriptor table is:

```
VIOCALLS.19  VioWrtTTY       frame: 2,2,4   (8 bytes)
KBDCALLS.4   KbdCharIn       frame: 2,2,4   (8 bytes)
KBDCALLS.9   KbdStringIn     frame: 2,2,4,4 (12 bytes)
KBDCALLS.13  KbdFlushBuffer  frame: 2       (2 bytes; M29E probe)
```

Width 2 is widened from an unsigned 16-bit Pascal argument to a 32-bit cdecl
argument.  Width 4 is copied as a 32-bit value; for the pointer cases this is
the preserved flat-address token returned by the intercepted `DosFlatToSel`.

The generated native stub now has one generic form:

```asm
mov  edx,esp                 ; stable view of incoming Pascal frame
; for each descriptor argument, low address -> high:
movzx eax,word ptr [edx+N]   ; width 2
; or
mov   eax,dword ptr [edx+N]  ; width 4
push eax
...
mov  eax,target
call eax
add  esp,4*argc
ret  pascal_frame_bytes
```

No selector is loaded and the 16-bit thunk object is still never executed.

## Why KbdFlushBuffer is useful

`KbdFlushBuffer` is deliberately boring at the API level, but it exercises a
new ABI corner: **there are no far pointer arguments at all**.  The historical
source declaration is simply:

```c
USHORT _far16 _pascal KBDFLUSHBUFFER(USHORT hkbd);
```

So the Pascal frame is only:

```
ESP+0  32-bit return address
ESP+4  USHORT hkbd
```

C/386 still has to use its 32->16 migration helper because the target routine
is far16, including conversion of the temporary 16-bit call stack.  If the
M29E probe passes, the same descriptor engine will have covered 2-byte,
8-byte and 12-byte frames, scalar-only calls, one-pointer calls and two-pointer
calls.

## Build and test on the i3

Build the normal Win32 host/DLLs:

```
make
```

Build the historical C/386 probe through the short LINK386 response file:

```
build-c386-far16-kbdflush.cmd
```

Then scan it:

```
os2host32 --scan c386-far16-kbdflush-test.exe
```

The desired tail is:

```
KBDCALLS.13 (1 site)
Execution model : contains 16-bit LE/LX objects/selectors
C/386 far16     : recognized KBDCALLS.13 KbdFlushBuffer migration thunk
Direct host path: supported via native far16 bridge
```

Run it:

```
os2host32 --run c386-far16-kbdflush-test.exe
```

The expected guest output is:

```
KbdFlushBuffer rc=0
```

The bridge diagnostic should show `frame=2`.

A wrapper is included:

```
cd examples
m29e-far16-kbdflush-test.cmd
```

## What this intentionally does not claim yet

The descriptor engine still handles the **single C/386 far16 thunk form** that
M29B-M29D proved.  A real reconstructed CMD will import several VIO/KBD far16
APIs in one executable.  The next experiment after M29E is therefore a
multi-thunk C/386 specimen, so we can see exactly how LINK386 combines several
14-byte 16-bit thunk fragments before generalising the fixup pairing.  We
should derive that layout from the real compiler/linker rather than guess it.
