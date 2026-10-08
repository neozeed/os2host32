# Soft386 NE-H3I — real LIB.EXE OBJ insertion

Baseline: full NE-H3H source archive. Changes are confined to `soft386/`.

## Observed failure

The untouched 1989 Microsoft LIB 3.17 NE (OS/2 beta SDK) created an empty
1,033-byte `huge.lib` but could not access an existing `HUGE.OBJ` (324-byte
Microsoft OMF), reporting U2157. Debugging with `--trace-hc` revealed that
some Dos16Open calls carry the candidate filename far pointer at SS:SP+30,
while the object's open carries it at SS:SP+26. The underlying historical
call-site/prototype discrepancy has not yet been established; do not assume
that every 16-bit DosOpen caller has the same 30-byte argument extent.

## Change

The Soft386 Dos16Open bridge tests both observed layouts, with guest selector,
output-pointer and NUL-terminated pathname validation, then uses the selected
26- or 30-byte cleanup after the far return. This preserves existing H3H
creation behavior while allowing the real OMF object to be opened.

No system DLL, native DOSCALLS personality, LE/LX path, CPU decoder, or common
library was modified. Guest HFILE table ownership remains in Soft386.

## Tests

`make check-ne` includes a new `check-ne-lib-add.py`, which copies the exact
user-uploaded HUGE.OBJ into an isolated directory, runs untouched LIB.EXE with
`huge.lib\ny\n+HUGE.OBJ\n\n\n`, checks exit 0, absence of U2157 or missing
DOS16 calls, and verifies a >1033-byte .LIB with an expected module marker.
The observed output was 1551 bytes; this is stronger than empty-library
creation, but does not independently verify complete OMF library consistency.

`make check-ne` PASS; `make check-quick` PASS locally with `CFLAGS=-O0 -std=gnu99 -w`
(fast diagnostic compile due to huge historical Tiny386 single-TU build).
Default -O2 compiler compilation was not completed in this execution window.
Native Windows runtime confirmation is pending user testing.

## Future

Test extracting the object with LIB.EXE, then independently parse records.
Investigate why LIB emits both 26- and 30-byte stack frames, and strengthen
frame selection if new mixed-ABI examples appear.
