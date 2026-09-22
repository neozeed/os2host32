# M29D - Microsoft C/386 `_far16` KbdStringIn bridge

M29D moves the migration bridge from one far16 pointer per call to **two**.
The historical API is:

```c
USHORT _far16 _pascal KBDSTRINGIN(char _far16 *buffer,
                                  STRINGINBUF _far16 *length,
                                  USHORT wait,
                                  USHORT hkbd);
```

with the four-byte wire structure:

```c
#pragma pack(2)
typedef struct _STRINGINBUF {
    USHORT cb;
    USHORT cchIn;
} STRINGINBUF;
#pragma pack()
```

For Microsoft C/386 the far-Pascal frame is therefore twelve bytes:

```
ESP+0   32-bit return address
ESP+4   USHORT hkbd
ESP+6   USHORT wait
ESP+8   packed far16 STRINGINBUF pointer
ESP+12  packed far16 character-buffer pointer
```

As in M29B/M29C, both compiler-generated `DosFlatToSel` conversions leave the
flat guest address intact as an opaque token.  OS2HOST32 recognizes the mixed
LE thunk from its fixups, validates the C/386 transition-helper signature and
patches the helper before its `LSS`/far jump can execute.

The native bridge rearranges the Pascal frame into Win32 cdecl:

```
KbdStringIn(char_buffer, stringinbuf, wait, hkbd)
```

and returns with `RET 12`, preserving the historical callee-cleanup contract.
The embedded 16-bit object is still mapped only as metadata and is never
executed.

## Host KBD behavior

`KBDCALLS.dll` now exports ordinal 9.  Its conservative ASCII-mode path:

* respects `STRINGINBUF.cb` (clamped to the historical maximum 255);
* returns the number of characters in `cchIn`;
* stops on Enter without placing CR/LF in the returned character buffer;
* supports Backspace editing on a console;
* does not append a NUL byte (the test program does that after the API call);
* also accepts redirected/file/pipe input as a byte stream.

This is deliberately not yet a full reimplementation of every historical
keyboard mode/status/template feature.  It is the smallest useful behavior
that proves the two-pointer far16 ABI.

## Build

Build the Win32 host/DLLs as usual:

```
make
```

Then build the historical probe on the i3:

```
build-c386-far16-kbdstring.cmd
```

The old linker is invoked through a response file:

```
link386 @m29d-far16-kbdstring.lnk
```

Scan it:

```
os2host32 --scan c386-far16-kbdstring-test.exe
```

The important tail should be:

```
C/386 far16     : recognized KBDCALLS.9 migration thunk
Direct host path: supported via native far16 bridge
```

Then run it:

```
os2host32 --run c386-far16-kbdstring-test.exe
```

Type something such as:

```
helllo<Backspace>o<Enter>
```

The program should report `text='hello'` and `cchIn=5`.

A convenience regression is also included:

```
cd examples
m29d-far16-kbdstring-test.cmd
```

## Deliberate limit

M29D still recognizes only the proven single-thunk C/386 mixed-mode form and
three targets: `VIOCALLS.19`, `KBDCALLS.4`, and `KBDCALLS.9`.  Arbitrary NE or
16-bit LE execution is not part of this milestone.
