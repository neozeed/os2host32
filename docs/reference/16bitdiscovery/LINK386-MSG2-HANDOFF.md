# LINK386 MSG.2 handoff

Baseline: Soft386 NE-H3N, whose module-aware import dispatcher already identifies LINK386 module 4 as MSG.

Observed last successful host operation: Dos16Open path `C:huge.exe`, mode `0043` compatibility case, action `0012` create-or-replace, guest HFILE 3; then DosQHandType and DosQFileMode, then unsupported `MSG.2`. Zero-byte output is expected from that stopping point.

Confirmed mapping: MSG.2 = DOSTRUEGETMESSAGE (both original MSG.DLL exports and SDK DOSCALLS.LIB IMPDEF).

**ABI remains unknown**: `DosGetMessage` is documented with 7 parameters, but the SDK describes an OS2.LIB wrapper calling `DosTrueGetMessage`. It may pass a bound message segment as an additional parameter. Avoid guessing stack cleanup or returning success without writing actual message text. For a useful runtime probe, trace/dump 40-48 words above SS:SP for MSG.2 and record CS:IP caller; compare with OS/2 1.21 native call if available.
