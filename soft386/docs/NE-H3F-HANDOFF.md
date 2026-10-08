# Soft386 NE-H3F — OS/2 2.0 beta SDK LIB.EXE intake

## Inputs
Original OS/2 NE LIB.EXE (Microsoft Library Manager 3.17.000), SHA256 93db5a0185277066b98b7fcc21b69002550f5680ca962b9acbb97ea798c3eccf, 55242 bytes.

## Finding / minimal change
The NE header uses SS:SP = segment 3:0000, autodata segment 3, minalloc 0x6850, 0x1000 stack, and only 0x09ca file-backed bytes. Previous Soft386 used `file_size + stack` (0x19ca) as initial SP for zero-SP NE images. It must use the allocated segment extent (0x7850 in the current Soft386 mapping) instead. This is an NE loader bug, not a guest memory-model issue.

Before: R6000 stack overflow and Dos16Exit rc=255.
After: original Microsoft librarian banner and prompt, then expected no-argument `LIB : fatal error U1151: syntax error : illegal file specification`, Dos16Exit rc=2. LIB is NOT yet functionally supported.

Trace reveals DOSCALLS ordinal 41 Dos16GetHugeShift, 14 Dos16SetSigHandler, 72 Dos16QCurDisk, 137 Dos16Read are currently unsupported. Other real librarian operations likely require Dos16Open/Close/Write/Seek and file metadata. Don't fake success; implement the relevant 16-bit ABIs with proper stack cleanup, segmented buffers, and guest-local handles when tested.

No system DLL modifications; no changes to 32-bit execution. Added LIB fixture and no-argument smoke test. `make check-ne` and `make check-quick` passed locally.

Next: implement ABI-audited 16-bit DOSCALLS filesystem and CLI support, and test with a real .LIB archive; retain the original no-argument diagnostic test.
