/* Simple Microsoft C/386 mixed-mode MOUCALLS probe for os2host32. */
#define INCL_16
#define INCL_MOU
#define INCL_DOSPROCESS
#include <os2.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    HMOU hmou;
    USHORT rc;
    USHORT buttons;
    USHORT mickeys;
    USHORT mask;
    USHORT wait;
    PTRLOC pos;
    MOUEVENTINFO event;
    int i;

    hmou = 0;
    rc = MouOpen((PSZ)0, &hmou);
    printf("MouOpen rc=%u hmou=%u\n", rc, hmou);
    if (rc != 0)
        return 1;

    buttons = 0;
    mickeys = 0;
    rc = MouGetNumButtons(&buttons, hmou);
    printf("MouGetNumButtons rc=%u buttons=%u\n", rc, buttons);
    rc = MouGetNumMickeys(&mickeys, hmou);
    printf("MouGetNumMickeys rc=%u mickeys/cm=%u\n", rc, mickeys);

    memset(&pos, 0, sizeof(pos));
    rc = MouGetPtrPos(&pos, hmou);
    printf("MouGetPtrPos rc=%u row=%u col=%u\n", rc, pos.row, pos.col);

    mask = MOUSE_MOTION | MOUSE_BN1_DOWN | MOUSE_MOTION_WITH_BN1_DOWN;
    rc = MouSetEventMask(&mask, hmou);
    printf("MouSetEventMask rc=%u mask=%04x\n", rc, mask);
    (void)MouDrawPtr(hmou);

    printf("Move the mouse or click button 1...\n");
    for (i = 0; i < 100; ++i) {
        memset(&event, 0, sizeof(event));
        wait = MOU_NOWAIT;
        rc = MouReadEventQue(&event, &wait, hmou);
        if (rc == 0) {
            printf("event fs=%04x time=%lu row=%u col=%u\n",
                   event.fs, (unsigned long)event.time,
                   event.row, event.col);
            break;
        }
        DosSleep(50L);
    }

    rc = MouClose(hmou);
    printf("MouClose rc=%u\n", rc);
    return 0;
}
