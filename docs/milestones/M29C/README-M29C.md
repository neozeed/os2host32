# M29C - Microsoft C/386 `_far16` KBD bridge

M29C extends the native C/386 migration bridge from `VIOCALLS.19` to
`KBDCALLS.4` (`KbdCharIn`).  The key new case is an **output** far16 pointer:
KbdCharIn writes a `KBDKEYINFO` structure supplied by 32-bit C/386 code.

The historical declaration used by the probe is:

```c
USHORT _far16 _pascal KBDCHARIN(KBDKEYINFO _far16 *info,
                                USHORT wait,
                                USHORT hkbd);
```

This produces the same eight-byte far-Pascal argument-frame family already
proved by `VioWrtTTY`:

```
ESP+0   32-bit return address
ESP+4   USHORT hkbd
ESP+6   USHORT wait
ESP+8   packed far16 KBDKEYINFO pointer
```

OS2HOST32 still does **not** execute the tiny 16-bit thunk.  It validates the
LE fixup graph and C/386 helper signature, patches the generated 32->16 helper
to a native x86 stub, then calls `KBDCALLS.dll` ordinal 4 as ordinary Win32
cdecl:

```
32-bit C/386 caller
       |
       | _far16 _pascal
       v
C/386 transition helper  --patched-->  native bridge
                                         |
                                         v
                                  KBDCALLS.4 KbdCharIn
                                         |
                                         v
                              guest KBDKEYINFO memory
```

For this intercepted path `DOSCALLS.425` (`DosFlatToSel`) continues to leave
EAX unchanged.  The would-be 16:16 value is therefore an opaque token carrying
the original flat guest address.  Because the helper is patched before any
`LSS`/far transition executes, KBDCALLS can safely use that address directly
and write the result back into the guest's mapped memory.

## KBDKEYINFO layout

The compatibility structure is explicitly packed on a two-byte boundary:

```c
#pragma pack(2)
typedef struct _KBDKEYINFO {
    UCHAR  chChar;
    UCHAR  chScan;
    UCHAR  fbStatus;
    UCHAR  bNlsShift;
    USHORT fsState;
    ULONG  time;
} KBDKEYINFO;
```

That is 10 bytes.  M29C applies the same layout to the Win32 KBDCALLS facade
and includes compile-time size checks so a host compiler cannot silently add
32-bit alignment padding before `time`.

## Build the host

```
make
```

## Build the historical C/386 KBD probe

From the bundle root on the i3:

```
build-c386-far16-kbd.cmd
```

The script retains `m29c-far16-kbd.lnk` and invokes the old linker as:

```
link386 @m29c-far16-kbd.lnk
```

so the RUN286/DOS command-tail problem fixed in M29B2 stays fixed.

Then:

```
os2host32 --scan c386-far16-kbd-test.exe
os2host32 --run  c386-far16-kbd-test.exe
```

Expected scan tail:

```
C/386 far16     : recognized KBDCALLS.4 migration thunk
Direct host path: supported via native far16 bridge
```

The run should display a prompt and block for one key.  After the key is
pressed it prints the returned character, scan code, status, shift state and
timestamp.  For a normal printable key the status should be nonzero and the
reported character should match what was pressed.

There is also a convenience script after building the probe:

```
cd examples
m29c-far16-kbd-test.cmd
```

## Deliberate limits

M29C recognizes only the proven single-thunk Microsoft C/386 migration shape
and only two targets: `VIOCALLS.19` and `KBDCALLS.4`.  It does not execute
arbitrary 16-bit LE code, create real LDT selectors, or yet bridge other KBD
calls such as `KbdFlushBuffer`.  Unknown mixed-mode images continue to fail
closed.
