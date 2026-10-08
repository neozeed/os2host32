# Soft386 NE-H3B: 16-bit Microsoft C runtime printf proof

H3B builds on the frozen NE-H3A implementation, without modifying the CPU,
loader, 16-bit DOSCALLS dispatch, any existing 32-bit implementation, or any
support DLL. It adds an actual historical NE guest with stdio `printf` as a
fixture and checks exact guest-visible bytes and process exit.

## Source and binary

User-supplied `TINY.EXE` (`tests/fixtures/tiny-printf-ne16.exe`):
SHA256 `17d6a190315c3a3db5ae64e4caec7c8da52d87df6b603bea6d09ea8ac2ccd931`.
It contains 5 NE segments, 1 module, 12 unique DOSCALLS imports, entry 0100:0026,
initial stack 0120:12A6 in the H3A guest mapping. The user describes it as
an OS/2 16-bit tiny-memory-model Microsoft C program using libc `printf`.

## Test

From `soft386/`, run `make check-ne` or `make check-quick`.

`tests/check-ne-printf.py` runs the untouched executable and requires:

- Exit code exactly zero.
- stdout exactly `hello from tiny memory model\r\n`.
- Guest termination diagnostic containing `rc=0`.

The test is intentionally strict and fails on missing, duplicate, or unexpected
output. Existing VOID NE test remains in place. `make check-quick` passed
locally including the existing 32-bit LE/threads/sync/memory/API bridge tests.

## Compatibility boundary and next step

No CPU/emulation changes were required to execute this guest: the previously
implemented Dos16Write adapter already passes the 16-bit C runtime's output
through the common personality. Continue adding only 16-bit DOSCALLS ABI
adapters based on observed calls from the next executable. Keep existing
32-bit LE/LX, API DLLs, and system bridges unchanged.

H3A handoff remains valid for loader internals and early semantic limitations.
