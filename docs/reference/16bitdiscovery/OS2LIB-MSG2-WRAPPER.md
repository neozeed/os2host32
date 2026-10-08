# OS/2 1.2 toolkit OS2.LIB: message thunk evidence

Original library: `OS2.LIB` (user-provided OS/2 1.2 toolkit), SHA256 `15b24848be88a28eacf9e686c7327fc9ff32e3aa2e89fe4ca1a88c54131e8847`, size 131129 bytes. Retained byte-for-byte in this directory.

## `msgseg.asm` record

Earlier OMF inspection located module `msgseg.asm` near file offset `0x5280`, exporting `DOSGETMESSAGE` and referencing `DOSTRUEGETMESSAGE` (the historical `MSG.2` export). The previously decoded instruction sequence is recorded below for reproducibility; raw bytes/OMF relocations must be consulted when changing the ABI.

```asm
DOSGETMESSAGE:
    xor  ax, ax
    push ax
    push cs
    push bp
    mov  bp, sp
    xchg word ptr [bp+6], ax
    xchg word ptr [bp+2], ax
    xchg word ptr [bp+8], ax
    mov  [bp+4], ax
    pop  bp
    jmp  far ptr DOSTRUEGETMESSAGE
```

The key property is a **far jump after rearranging the stack**, not an ordinary call to `MSG.2`. The public `DosGetMessage` signature has seven parameters (`ppchVTable`, `usVCount`, `pchBuf`, `cbBuf`, `usMsgNo`, `pszFileName`, `pcbMsg`), but the internal `DosTrueGetMessage` stack is altered by this thunk. Do **not** assume a seven-parameter callee-cleanup size.

## Next verification

The H3P source enables an `MSG.2 RAW FRAME` dump (32 words at `SS:SP`, including return address) on the first blocked call. Run LINK386 with `--trace-hc`, preserve its stack dump, and compare word-by-word with the wrapper algorithm and MSG.DLL's internal entry sequence. Only then add a handler that provides valid output text and the correct number of bytes popped on return. This diagnostic path deliberately does not report success for an unknown ABI.

No 32-bit OS/2 DLL has been modified. The public 16-bit ordinal table remains in `soft386-sdk-16bit-ordinals.csv`.
