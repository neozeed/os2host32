#include <stdio.h>
#include <string.h>

#include "os2_mou.h"
#include "os2_mou_backend.h"

#define MAX_HOST_EVENTS 16

struct FakeBackend {
    struct Os2MouHostEvent events[MAX_HOST_EVENTS];
    unsigned int head;
    unsigned int count;
    unsigned int activate_count;
    unsigned int deactivate_count;
    unsigned int flush_count;
    unsigned int warp_count;
    unsigned short warp_row;
    unsigned short warp_col;
};

static int failures;

static void check(int ok, const char *message)
{
    if (!ok) {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

static Os2MouApiRet fake_activate(void *opaque, unsigned short *buttons,
                                  unsigned short *mickeys,
                                  unsigned short *row, unsigned short *col)
{
    struct FakeBackend *f;
    f = (struct FakeBackend *)opaque;
    ++f->activate_count;
    *buttons = 3U;
    *mickeys = 12U;
    *row = 4U;
    *col = 7U;
    return 0U;
}

static Os2MouApiRet fake_deactivate(void *opaque)
{
    struct FakeBackend *f;
    f = (struct FakeBackend *)opaque;
    ++f->deactivate_count;
    return 0U;
}

static Os2MouApiRet fake_read(void *opaque, int wait,
                              struct Os2MouHostEvent *event, int *present)
{
    struct FakeBackend *f;
    (void)wait;
    f = (struct FakeBackend *)opaque;
    *present = 0;
    memset(event, 0, sizeof(*event));
    if (f->count == 0U)
        return 0U;
    *event = f->events[f->head];
    f->head = (f->head + 1U) % MAX_HOST_EVENTS;
    --f->count;
    *present = 1;
    return 0U;
}

static Os2MouApiRet fake_flush(void *opaque)
{
    struct FakeBackend *f;
    f = (struct FakeBackend *)opaque;
    f->head = f->count = 0U;
    ++f->flush_count;
    return 0U;
}

static Os2MouApiRet fake_warp(void *opaque, unsigned short row,
                              unsigned short col)
{
    struct FakeBackend *f;
    f = (struct FakeBackend *)opaque;
    ++f->warp_count;
    f->warp_row = row;
    f->warp_col = col;
    return 0U;
}

static const struct Os2MouBackendOps fake_ops = {
    fake_activate, fake_deactivate, fake_read, fake_flush, fake_warp
};

static void push_event(struct FakeBackend *f, unsigned short row,
                       unsigned short col, unsigned short buttons,
                       unsigned short changed, int motion,
                       unsigned long time)
{
    unsigned int pos;
    pos = (f->head + f->count) % MAX_HOST_EVENTS;
    memset(&f->events[pos], 0, sizeof(f->events[pos]));
    f->events[pos].row = row;
    f->events[pos].col = col;
    f->events[pos].buttons = buttons;
    f->events[pos].changed_buttons = changed;
    f->events[pos].motion = motion;
    f->events[pos].time = (uint32_t)time;
    ++f->count;
}

int main(void)
{
    struct FakeBackend fake;
    struct Os2MouSession session;
    struct Os2MouEventInfo event;
    struct Os2MouQueInfo qi;
    struct Os2MouPtrLoc loc;
    struct Os2MouScaleFact scale;
    struct Os2MouThreshold threshold;
    struct Os2MouPtrShape shape;
    unsigned char shape_data[8];
    unsigned short hmou;
    unsigned short hmou2;
    unsigned short value;
    unsigned short wait;
    int present;

    memset(&fake, 0, sizeof(fake));
    os2_mou_session_init(&session, &fake, &fake_ops);

    hmou = hmou2 = 0U;
    check(os2_mou_MouOpen(&session, NULL, &hmou) == 0U && hmou != 0U,
          "MouOpen allocates handle");
    check(fake.activate_count == 1U && session.buttons == 3U &&
          session.mickeys == 12U && session.pointer.row == 4U &&
          session.pointer.col == 7U,
          "first open activates backend and seeds device profile");
    check(os2_mou_MouOpen(&session, NULL, &hmou2) == 0U && hmou2 != hmou &&
          fake.activate_count == 1U,
          "second handle shares one device activation");

    value = 0U;
    check(os2_mou_MouGetNumButtons(&session, &value, hmou) == 0U && value == 3U,
          "button count common state");
    check(os2_mou_MouGetNumMickeys(&session, &value, hmou) == 0U && value == 12U,
          "mickeys count common state");

    value = OS2_MOU_MOUSE_MOTION | OS2_MOU_MOUSE_BN1_DOWN |
            OS2_MOU_MOUSE_MOTION_WITH_BN1_DOWN;
    check(os2_mou_MouSetEventMask(&session, &value, hmou) == 0U,
          "set event mask");
    value = 0U;
    check(os2_mou_MouGetEventMask(&session, &value, hmou) == 0U &&
          value == (OS2_MOU_MOUSE_MOTION | OS2_MOU_MOUSE_BN1_DOWN |
                    OS2_MOU_MOUSE_MOTION_WITH_BN1_DOWN),
          "get event mask");

    push_event(&fake, 5U, 8U, 0U, 0U, 1, 100UL);
    push_event(&fake, 5U, 8U, OS2_MOU_BUTTON1, OS2_MOU_BUTTON1, 0, 101UL);
    memset(&qi, 0, sizeof(qi));
    check(os2_mou_MouGetNumQueEl(&session, &qi, hmou) == 0U &&
          qi.cEvents == 2U && qi.cmaxEvents == OS2_MOU_MAX_EVENTS,
          "queue count drains backend samples into common queue");

    wait = OS2_MOU_NOWAIT;
    memset(&event, 0, sizeof(event));
    check(os2_mou_MouReadEventQue(&session, &event, &wait, hmou) == 0U &&
          event.fs == OS2_MOU_MOUSE_MOTION && event.row == 5U &&
          event.col == 8U && event.time == 100UL,
          "absolute motion event semantics");
    memset(&event, 0, sizeof(event));
    check(os2_mou_MouReadEventQue(&session, &event, &wait, hmou) == 0U &&
          (event.fs & OS2_MOU_MOUSE_BN1_DOWN) != 0U && event.time == 101UL,
          "button-down event semantics");
    memset(&event, 0xff, sizeof(event));
    check(os2_mou_MouReadEventQue(&session, &event, &wait, hmou) ==
              OS2_MOU_NO_ERROR_MOUSE_NO_DATA &&
          event.fs == 0U && event.time == 0UL,
          "NOWAIT empty queue returns NO_DATA and zero event");

    value = OS2_MOU_STATUS_MICKEYS;
    check(os2_mou_MouSetDevStatus(&session, &value, hmou) == 0U,
          "enable relative mickey reporting");
    push_event(&fake, 8U, 10U, OS2_MOU_BUTTON1, 0U, 1, 102UL);
    memset(&event, 0, sizeof(event));
    check(os2_mou_MouReadEventQue(&session, &event, &wait, hmou) == 0U &&
          (signed short)event.row == 3 && (signed short)event.col == 2 &&
          (event.fs & OS2_MOU_MOUSE_MOTION_WITH_BN1_DOWN) != 0U,
          "relative motion uses signed two's-complement row/col");

    loc.row = 9U; loc.col = 11U;
    check(os2_mou_MouSetPtrPos(&session, &loc, hmou) == 0U &&
          fake.warp_count == 1U && fake.warp_row == 9U && fake.warp_col == 11U,
          "set pointer updates common state and backend");
    memset(&loc, 0, sizeof(loc));
    check(os2_mou_MouGetPtrPos(&session, &loc, hmou) == 0U &&
          loc.row == 9U && loc.col == 11U,
          "get pointer returns common absolute position");

    scale.rowScale = 2U; scale.colScale = 3U;
    check(os2_mou_MouSetScaleFact(&session, &scale, hmou) == 0U,
          "set scale factors");
    memset(&scale, 0, sizeof(scale));
    check(os2_mou_MouGetScaleFact(&session, &scale, hmou) == 0U &&
          scale.rowScale == 2U && scale.colScale == 3U,
          "get scale factors");

    memset(&threshold, 0, sizeof(threshold));
    threshold.Length = 10U;
    threshold.Level1 = 3U; threshold.Lev1Mult = 2U;
    threshold.Level2 = 7U; threshold.lev2Mult = 4U;
    check(os2_mou_MouSetThreshold(&session, &threshold, hmou) == 0U,
          "set threshold");
    memset(&threshold, 0, sizeof(threshold));
    check(os2_mou_MouGetThreshold(&session, &threshold, hmou) == 0U &&
          threshold.Level2 == 7U && threshold.lev2Mult == 4U,
          "get threshold");

    memset(&shape, 0, sizeof(shape));
    shape.cb = 4U; shape.col = 1U; shape.row = 1U;
    shape_data[0] = 1U; shape_data[1] = 2U;
    shape_data[2] = 3U; shape_data[3] = 4U;
    check(os2_mou_MouSetPtrShape(&session, shape_data, &shape, hmou) == 0U,
          "set logical pointer shape");
    memset(shape_data, 0, sizeof(shape_data));
    memset(&shape, 0, sizeof(shape));
    shape.cb = (unsigned short)sizeof(shape_data);
    check(os2_mou_MouGetPtrShape(&session, shape_data, &shape, hmou) == 0U &&
          shape.cb == 4U && shape_data[0] == 1U && shape_data[3] == 4U,
          "get logical pointer shape");

    push_event(&fake, 10U, 12U, 0U, OS2_MOU_BUTTON1, 0, 103UL);
    check(os2_mou_MouFlushQue(&session, hmou) == 0U && fake.flush_count == 1U,
          "flush clears common and backend pending input");
    present = 1;
    memset(&event, 0, sizeof(event));
    check(os2_mou_try_read(&session, &event, &present, hmou) == 0U && !present,
          "nonblocking scheduler primitive reports no event");

    check(os2_mou_MouRegister(&session, "X", "Y", 1UL) ==
              OS2_MOU_ERROR_MOUSE_REGISTER,
          "subsystem replacement explicitly unsupported");
    check(os2_mou_MouDeRegister(&session) == OS2_MOU_ERROR_MOUSE_DEREGISTER,
          "deregister explicitly unsupported");

    check(os2_mou_MouClose(&session, hmou) == 0U &&
          fake.deactivate_count == 0U,
          "closing one handle leaves shared device active");
    check(os2_mou_MouClose(&session, hmou2) == 0U &&
          fake.deactivate_count == 1U,
          "last close deactivates backend");
    check(os2_mou_MouClose(&session, hmou2) == OS2_MOU_ERROR_MOUSE_INV_HANDLE,
          "stale HMOU rejected");

    if (failures != 0) {
        fprintf(stderr, "mou-core-check: %d failure(s)\n", failures);
        return 1;
    }
    printf("mou-core-check: PASS\n");
    return 0;
}
