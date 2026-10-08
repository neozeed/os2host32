# Soft386 OS/2 1.x 16-bit API discovery

This directory preserves the original user-supplied SDK/DLL evidence and the derived import-ordinal registry. It is **reference material**; the Soft386 binary/bridge has **not** been changed in this documentation checkpoint.

## Evidence

- `DOSCALLS.LIB`: historical SDK OMF import records. The CSV contains 333 ordinal-mapped imports; eight other import definitions were not encoded as ordinal rows.
- `MSG.DLL`: original 1.x NE DLL; named exports include `DOSPUTMESSAGE` (1), `DOSTRUEGETMESSAGE` (2), `DOSINSMESSAGE` (3).
- `NLS.DLL`: 1.21 NE DLL; `DOSCASEMAP` (1), `DOSGETCOLLATE` (2), `DOSGETCTRYINFO` (3), `DOSGETDBCSEV` (4).
- `DOSCALL1.DLL`: reference for real 16-bit DOSCALLS routines.

## LINK386 discovery

LINK386's *own* NE module-reference indices are 1 DOSCALLS, 2 KBDCALLS, 3 NLS, 4 MSG. These indices are **per executable** and must not be fixed global IDs. It reaches `MSG.2` after opening `C:huge.exe` successfully, before output writes. `MSG.2` is `DosTrueGetMessage`, but the exact **16-bit internal argument layout and callee stack cleanup** remain unverified. Do not substitute the documented 7-parameter `DosGetMessage` wrapper ABI for `DosTrueGetMessage` without disassembling the wrapper and original DLL entry.

The Microsoft Programmer's Library already present at `../microsoft-programmers-library/os2/os2kno.txt`, item 504, says `DosGetMessage` is a **real stub in OS2.LIB**, which calls `DosTrueGetMessage` (imported through API.LIB). Item 481 discusses searching a bound message segment before an external message file. See `prgmr3.txt` section `DosGetMessage` for the *public* wrapper's parameters. These references explain why a linker may import the internal MSG ordinal while source invokes `DosGetMessage`.

## Safe next steps

1. Identify the wrapper object in original `OS2.LIB` and disassemble its call to `DosTrueGetMessage`, recording exact pushed argument widths/order.
2. Disassemble the entry for `MSG.DLL` ordinal 2 (NE segment 3, offset 0000) to confirm cleanup (`retf imm16`) and whether the extra message segment parameter is far.
3. Implement a Soft386-local `MSG.2` handler backed by existing `common/msg` parsing where suitable, with segmented pointer checks and correct returned message length/error.
4. Use LINK386 trace to compare resulting guest behavior; require the first real write to `huge.exe` and validate output format before claiming link success.
5. Keep all existing 16-bit and 32-bit regressions; do not change system DLLs.

## Important differences

Modern 32-bit system DLL ordinals do **not** match these historical 16-bit ordinals. For instance `MSG.2` historical = `DosTrueGetMessage`, while the current native `dlls/msg/msg.def` exports `DosTrueGetMessage @6`. The dispatch table must remain (module name, historical ordinal, 16-bit ABI).

H3P: `OS2.LIB` and `OS2LIB-MSG2-WRAPPER.md` preserve the 1.2 toolkit message-wrapper evidence; Soft386 traces the unresolved MSG.2 far-call frame.
