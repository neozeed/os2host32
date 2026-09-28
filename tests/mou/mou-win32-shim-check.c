#include <stdio.h>
#include <string.h>

#include "windows.h"
#include "os2_mou.h"
#include "os2_mou_win32.h"

static int failures;
static void check(int ok, const char *message)
{
    if (!ok) { fprintf(stderr, "FAIL: %s\n", message); ++failures; }
}

int main(void)
{
    struct Os2MouSession *session;
    struct Os2MouEventInfo event;
    struct Os2MouPtrLoc loc;
    unsigned short hmou;
    unsigned short wait;
    unsigned short row;
    unsigned short col;

    mou_stub_reset();
    session = os2_mou_win32_session();
    hmou = 0U;
    check(os2_mou_MouOpen(session, NULL, &hmou) == 0U && hmou != 0U &&
          session->pointer.row == 2U && session->pointer.col == 3U &&
          session->buttons == 3U,
          "Win32 activation samples pointer and button count");

    wait = OS2_MOU_NOWAIT;
    memset(&event, 0, sizeof(event));
    check(os2_mou_MouReadEventQue(session, &event, &wait, hmou) ==
              OS2_MOU_NO_ERROR_MOUSE_NO_DATA,
          "unchanged sampled pointer yields no event");

    mou_stub_set_pointer(4U, 5U, 0U);
    memset(&event, 0, sizeof(event));
    check(os2_mou_MouReadEventQue(session, &event, &wait, hmou) == 0U &&
          event.fs == OS2_MOU_MOUSE_MOTION && event.row == 4U &&
          event.col == 5U,
          "Win32 pointer motion becomes OS/2 motion event");

    mou_stub_set_pointer(4U, 5U, OS2_MOU_BUTTON1);
    memset(&event, 0, sizeof(event));
    check(os2_mou_MouReadEventQue(session, &event, &wait, hmou) == 0U &&
          (event.fs & OS2_MOU_MOUSE_BN1_DOWN) != 0U,
          "Win32 left-button transition becomes OS/2 button event");

    loc.row = 6U; loc.col = 7U;
    check(os2_mou_MouSetPtrPos(session, &loc, hmou) == 0U &&
          mou_stub_setpos_count() == 1U,
          "MouSetPtrPos reaches Win32 pointer warp backend");
    row = col = 0U;
    mou_stub_get_pointer(&row, &col);
    check(row == 6U && col == 7U,
          "Win32 pointer warp maps OS/2 cell coordinates to host pixels");

    mou_stub_set_pointer(8U, 9U, 0U);
    check(os2_mou_MouFlushQue(session, hmou) == 0U,
          "Win32 flush resamples baseline");
    memset(&event, 0, sizeof(event));
    check(os2_mou_MouReadEventQue(session, &event, &wait, hmou) ==
              OS2_MOU_NO_ERROR_MOUSE_NO_DATA,
          "flush prevents pre-flush sample from leaking as event");

    check(os2_mou_MouClose(session, hmou) == 0U,
          "Win32 close/deactivate path");

    if (failures != 0) {
        fprintf(stderr, "mou-win32-shim-check: %d failure(s)\n", failures);
        return 1;
    }
    printf("mou-win32-shim-check: PASS\n");
    return 0;
}
