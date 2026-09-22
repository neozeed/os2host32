# START HERE — AFTER M31 FINAL

Use `NEXT-CHAT-HANDOFF.md` as the authoritative restart prompt and
`MILESTONE31-FINAL.md` as the freeze definition.

Before changing code:

    make cmd32-tabcomp-check
    make m31g-neko-r19-check

Both should pass.

Then start the next milestone from the first real boundary of the next target.
Keep V1 native-Win32 work separate from frozen WHP/V2 and EMX experiments.
