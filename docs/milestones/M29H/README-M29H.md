# M29H - complete real C/386 console backend

M29G proved that the real `cmdos2_os2.c` backend can consume the generalized
M29F far16 bridge machinery for VioWrtTTY, KbdCharIn and KbdFlushBuffer.
M29H closes the remaining VIO console holes needed by CMD:

- VioGetCurPos (VIOCALLS.9)
- VioSetCurPos (VIOCALLS.15)
- VioGetMode (VIOCALLS.21)
- VioScrollUp (VIOCALLS.7)

`CmdO2VioClear` now queries the mode, scroll-clears the full screen using a
space/attribute cell, and homes the cursor.  The backend uses a private packed
12-byte VioGetMode wire prefix so it does not depend on SDK structure spelling
or later header generations.

## i3 test

    build-os2-backend.cmd
    os2host32 --scan cmdos2_os2_console_test.exe
    os2host32 --run  cmdos2_os2_console_test.exe

Expected scan headline:

    C/386 far16     : recognized 7 migration thunks
    Direct host path: supported via native far16 bridges

Expected final runtime marker after pressing a key:

    M29H_OS2_CONSOLE_BACKEND_OK

VioScrollUp is deliberately valuable here: its seven arguments exercise the
descriptor-driven Pascal frame engine much harder than the earlier 1-4 argument
fixtures.
