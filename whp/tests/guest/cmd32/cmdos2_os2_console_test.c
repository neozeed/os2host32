/*
 * cmdos2_os2_console_test.c - M29H real-backend complete console bridge probe.
 *
 * This executable still knows nothing about _far16.  It exercises the stable
 * cmdos2.h surface while cmdos2_os2.c emits all seven historical VIO/KBD
 * migration helpers.  M29H adds cursor, mode and scroll paths so CLS can use
 * the same native far16 bridge machinery as keyboard and TTY output.
 */
#include <stdio.h>
#include "cmdos2.h"

int main(void)
{
    static const char intro[] =
        "M29H real cmdos2_os2 backend: press one key.\r\nKey: ";
    static const char nl[] = "\r\n";
    struct CmdO2KbdKeyInfo key;
    CmdO2Rc rc;
    unsigned short row;
    unsigned short col;

    if (!CmdO2Init()) {
        printf("CmdO2Init failed: %s\r\n", CmdO2InitError());
        return 1;
    }

    rc = CmdO2VioGetCurPos(&row, &col);
    if (rc != 0) {
        printf("CmdO2VioGetCurPos rc=%lu\r\n", rc);
        CmdO2Done();
        return 2;
    }
    rc = CmdO2VioSetCurPos(row, col);
    if (rc != 0) {
        printf("CmdO2VioSetCurPos rc=%lu\r\n", rc);
        CmdO2Done();
        return 3;
    }
    rc = CmdO2VioClear();
    if (rc != 0) {
        printf("CmdO2VioClear rc=%lu\r\n", rc);
        CmdO2Done();
        return 4;
    }

    rc = CmdO2KbdFlushBuffer();
    if (rc != 0) {
        printf("CmdO2KbdFlushBuffer rc=%lu\r\n", rc);
        CmdO2Done();
        return 5;
    }

    rc = CmdO2VioWrtTTY(intro, (unsigned long)(sizeof(intro) - 1U));
    if (rc != 0) {
        printf("CmdO2VioWrtTTY rc=%lu\r\n", rc);
        CmdO2Done();
        return 6;
    }

    rc = CmdO2KbdCharIn(&key, CMDO2_IO_WAIT);
    (void)CmdO2VioWrtTTY(nl, (unsigned long)(sizeof(nl) - 1U));
    if (rc != 0) {
        printf("CmdO2KbdCharIn rc=%lu\r\n", rc);
        CmdO2Done();
        return 7;
    }

    printf("backend key char=%u scan=%u status=%u nls=%u state=%u time=%lu\r\n",
           (unsigned)key.chChar,
           (unsigned)key.chScan,
           (unsigned)key.fbStatus,
           (unsigned)key.bNlsShift,
           (unsigned)key.fsState,
           (unsigned long)key.time);
    if (key.chChar >= 32U && key.chChar < 127U)
        printf("backend key character='%c'\r\n", key.chChar);

    CmdO2Done();
    printf("M29H_OS2_CONSOLE_BACKEND_OK\r\n");
    return 0;
}
