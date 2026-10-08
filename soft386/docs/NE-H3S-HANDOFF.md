# Soft386 NE-H3S handoff

H3S removes the false 30-byte Dos16Open ABI heuristic. Root cause was DOSCALLS.75 / DosQFileMode returning with 8 bytes of Pascal cleanup instead of its documented 12 bytes, leaving a DWORD on the guest stack before later DosOpen calls.

Changes are Soft386-only:
- DosQFileMode cleanup 12 bytes.
- DosOpen fixed documented 26-byte argument layout.
- DosGetPID (DOSCALLS.94) writes guest PID/TID/PPID state; initial identity is PID 1, TID 1, PPID 0.
- Existing narrow LINK386 mode 0043 create/replace compatibility remains pending deeper historical explanation.

Verification on POSIX diagnostic host:
- make check-ne: PASS
- make check-quick: PASS

Windows RC next test should confirm simcity.RES creation now uses the ordinary 26-byte DosOpen layout and that DOS16.94 no longer reports unsupported.
