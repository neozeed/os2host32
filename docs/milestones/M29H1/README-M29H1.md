# Milestone 29H1 - complete DEF imports for the real OS/2 backend

M29H added four more C/386 `_far16 _pascal` VIO calls to `cmdos2_os2.c`.
Because LINK386 resolves all external references present in `cmdos2_os2.obj`,
the smoke executable's DEF file must list those imports too, even though that
particular test does not exercise every console wrapper.

M29H1 makes `cmdos2_os2_smoke.def` use the same complete VIO/KBD import set as
`cmdos2_os2_console_test.def`.

No implementation or bridge logic changed.

Build on the C/386 machine with:

    make
    build-os2-backend.cmd

Then scan and run the interactive test:

    os2host32 --scan cmdos2_os2_console_test.exe
    os2host32 --run cmdos2_os2_console_test.exe

For the completed backend, the expected mixed-mode set is seven migration
thunks: VioScrollUp, VioGetCurPos, VioSetCurPos, VioWrtTTY, VioGetMode,
KbdCharIn, and KbdFlushBuffer.
