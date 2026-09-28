#include <stdio.h>
#include <string.h>

#include "os2_mou.h"
#include "os2_mou_backend.h"

static struct Os2MouSession session;
static int initialized;
static unsigned int activate_count;
static unsigned int read_count;

static Os2MouApiRet fake_activate(void *opaque, unsigned short *buttons,
                                  unsigned short *mickeys,
                                  unsigned short *row, unsigned short *col)
{
    (void)opaque;
    ++activate_count;
    *buttons = 2U; *mickeys = 8U; *row = 1U; *col = 2U;
    return 0U;
}
static Os2MouApiRet fake_deactivate(void *opaque) { (void)opaque; return 0U; }
static Os2MouApiRet fake_read(void *opaque, int wait,
                              struct Os2MouHostEvent *event, int *present)
{
    (void)opaque; (void)wait;
    ++read_count;
    memset(event, 0, sizeof(*event));
    *present = 0;
    if (read_count == 1U) {
        event->row = 3U; event->col = 4U; event->motion = 1;
        event->time = 123UL; *present = 1;
    }
    return 0U;
}
static Os2MouApiRet fake_flush(void *opaque) { (void)opaque; return 0U; }
static Os2MouApiRet fake_warp(void *opaque, unsigned short row,
                              unsigned short col)
{ (void)opaque; (void)row; (void)col; return 0U; }
static const struct Os2MouBackendOps ops = {
    fake_activate, fake_deactivate, fake_read, fake_flush, fake_warp
};

struct Os2MouSession *os2_mou_win32_session(void)
{
    if (!initialized) {
        os2_mou_session_init(&session, NULL, &ops);
        initialized = 1;
    }
    return &session;
}

unsigned short MouOpen(const char *, unsigned short *);
unsigned short MouClose(unsigned short);
unsigned short MouGetNumButtons(unsigned short *, unsigned short);
unsigned short MouReadEventQue(struct Os2MouEventInfo *, unsigned short *, unsigned short);
unsigned short MouSetEventMask(const unsigned short *, unsigned short);
unsigned short MouGetPtrPos(struct Os2MouPtrLoc *, unsigned short);

static int failures;
static void check(int ok, const char *message)
{
    if (!ok) { fprintf(stderr, "FAIL: %s\n", message); ++failures; }
}

int main(void)
{
    unsigned short hmou;
    unsigned short buttons;
    unsigned short mask;
    unsigned short wait;
    struct Os2MouEventInfo event;
    struct Os2MouPtrLoc loc;

    hmou = 0U;
    check(MouOpen(NULL, &hmou) == 0U && hmou != 0U && activate_count == 1U,
          "actual MouOpen veneer reaches common semantics");
    buttons = 0U;
    check(MouGetNumButtons(&buttons, hmou) == 0U && buttons == 2U,
          "actual MouGetNumButtons veneer");
    mask = OS2_MOU_MOUSE_MOTION;
    check(MouSetEventMask(&mask, hmou) == 0U,
          "actual MouSetEventMask veneer");
    wait = OS2_MOU_NOWAIT;
    memset(&event, 0, sizeof(event));
    check(MouReadEventQue(&event, &wait, hmou) == 0U &&
          event.fs == OS2_MOU_MOUSE_MOTION && event.row == 3U &&
          event.col == 4U && event.time == 123UL,
          "actual MouReadEventQue veneer");
    memset(&loc, 0, sizeof(loc));
    check(MouGetPtrPos(&loc, hmou) == 0U && loc.row == 3U && loc.col == 4U,
          "actual MouGetPtrPos veneer shares common state");
    check(MouClose(hmou) == 0U, "actual MouClose veneer");

    if (failures != 0) {
        fprintf(stderr, "mou-veneer-check: %d failure(s)\n", failures);
        return 1;
    }
    printf("mou-veneer-check: PASS\n");
    return 0;
}
