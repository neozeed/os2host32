# MSG.2 DosTrueGetMessage — H3Q ABI reconstruction

Evidence: OS/2 1.2 `OS2.LIB` OMF member `msgseg.asm`, original OS/2 1.21 `MSG.DLL`, and the user's LINK386 H3P stack trace. `msgseg.asm` rearranges the public DosGetMessage stack, inserts an extra far pointer, and far-jumps to MSG.2.

At hypercall, after the far return IP:CS, the observed stack is:

| Byte offset | Width | Parameter | Captured words |
|---|---:|---|---|
| +04 | 4 | bound message segment far pointer | 0000 0110 |
| +08 | 4 | output actual-length far pointer | 7A36 0120 |
| +0C | 4 | message filename far pointer | 12F0 0120 |
| +10 | 2 | message number | 0081 |
| +12 | 2 | destination capacity | 0100 |
| +14 | 4 | destination buffer far pointer | 7930 0120 |
| +18 | 2 | substitution count | 0000 |
| +1A | 4 | substitution table far pointer | 0000 0000 |

The frame has **26 argument bytes**; after the far return address the callee removes 26 bytes. Do not use the 32-bit MSG.6 ABI or its 32-bit ordinal. The historical internal ordinal is `MSG.2`.

H3Q implements this frame and routes external MKMSGF v0/v2 message files to unchanged `common/msg/os2_msg.c`. It validates the guest pointers and writes the result length (16-bit). An unavailable file produces a genuine file error, not invented success. Nonzero substitution counts are deferred with a specific error, and bound 16-bit message-segment decoding is not yet implemented. The segment pointer is recorded but only external-file fallback is attempted.

**Verification boundary:** compile, all NE regressions, and `make check-quick` pass on Linux diagnostic build. End-to-end LINK386 after MSG.2 remains unverified: its `LLIBCE.LIB` is not present. Test under Windows with the same command and libraries, using `--trace-hc` and retain the resulting log/output artifact. Avoid assuming a nonempty output executable until it is inspected.
