#include <string.h>

#include "os2_mou.h"
#include "os2_mou_backend.h"

static int mou_ready(const struct Os2MouSession *session)
{
    return session != NULL && session->backend != NULL &&
           session->backend->activate != NULL &&
           session->backend->deactivate != NULL &&
           session->backend->read_event != NULL &&
           session->backend->flush_events != NULL;
}

static int valid_handle(const struct Os2MouSession *session,
                        unsigned short hmou)
{
    unsigned int i;
    if (session == NULL || hmou == 0U)
        return 0;
    for (i = 0U; i < OS2_MOU_MAX_HANDLES; ++i) {
        if (session->handles[i].in_use &&
            session->handles[i].value == hmou)
            return 1;
    }
    return 0;
}

static int alloc_handle(struct Os2MouSession *session,
                        unsigned short *hmou)
{
    unsigned int i;
    unsigned short candidate;
    unsigned int tries;

    if (session == NULL || hmou == NULL)
        return 0;
    for (i = 0U; i < OS2_MOU_MAX_HANDLES; ++i) {
        if (!session->handles[i].in_use) {
            candidate = session->next_handle;
            if (candidate == 0U)
                candidate = 1U;
            for (tries = 0U; tries < 0xfffeU; ++tries) {
                if (!valid_handle(session, candidate))
                    break;
                ++candidate;
                if (candidate == 0U)
                    candidate = 1U;
            }
            if (tries == 0xfffeU)
                return 0;
            session->handles[i].in_use = 1;
            session->handles[i].value = candidate;
            session->next_handle = (unsigned short)(candidate + 1U);
            if (session->next_handle == 0U)
                session->next_handle = 1U;
            *hmou = candidate;
            return 1;
        }
    }
    return 0;
}

static void free_handle(struct Os2MouSession *session,
                        unsigned short hmou)
{
    unsigned int i;
    if (session == NULL)
        return;
    for (i = 0U; i < OS2_MOU_MAX_HANDLES; ++i) {
        if (session->handles[i].in_use &&
            session->handles[i].value == hmou) {
            memset(&session->handles[i], 0, sizeof(session->handles[i]));
            return;
        }
    }
}

static void clear_queue(struct Os2MouSession *session)
{
    if (session == NULL)
        return;
    memset(session->events, 0, sizeof(session->events));
    session->event_head = 0U;
    session->event_count = 0U;
}

static void enqueue_event(struct Os2MouSession *session,
                          const struct Os2MouEventInfo *event)
{
    unsigned short pos;
    if (session == NULL || event == NULL)
        return;
    if (session->event_count >= OS2_MOU_MAX_EVENTS) {
        session->event_head = (unsigned short)
            ((session->event_head + 1U) % OS2_MOU_MAX_EVENTS);
        --session->event_count;
    }
    pos = (unsigned short)
        ((session->event_head + session->event_count) % OS2_MOU_MAX_EVENTS);
    session->events[pos] = *event;
    ++session->event_count;
}

static int dequeue_event(struct Os2MouSession *session,
                         struct Os2MouEventInfo *event)
{
    if (session == NULL || event == NULL || session->event_count == 0U)
        return 0;
    *event = session->events[session->event_head];
    memset(&session->events[session->event_head], 0,
           sizeof(session->events[session->event_head]));
    session->event_head = (unsigned short)
        ((session->event_head + 1U) % OS2_MOU_MAX_EVENTS);
    --session->event_count;
    return 1;
}

static unsigned short event_fs_from_host(const struct Os2MouHostEvent *host)
{
    unsigned short fs;
    fs = 0U;
    if (host->motion) {
        if ((host->buttons & OS2_MOU_BUTTON1) != 0U)
            fs |= OS2_MOU_MOUSE_MOTION_WITH_BN1_DOWN;
        if ((host->buttons & OS2_MOU_BUTTON2) != 0U)
            fs |= OS2_MOU_MOUSE_MOTION_WITH_BN2_DOWN;
        if ((host->buttons & OS2_MOU_BUTTON3) != 0U)
            fs |= OS2_MOU_MOUSE_MOTION_WITH_BN3_DOWN;
        if ((host->buttons & (OS2_MOU_BUTTON1 | OS2_MOU_BUTTON2 |
                              OS2_MOU_BUTTON3)) == 0U)
            fs |= OS2_MOU_MOUSE_MOTION;
    }
    if ((host->buttons & OS2_MOU_BUTTON1) != 0U)
        fs |= OS2_MOU_MOUSE_BN1_DOWN;
    if ((host->buttons & OS2_MOU_BUTTON2) != 0U)
        fs |= OS2_MOU_MOUSE_BN2_DOWN;
    if ((host->buttons & OS2_MOU_BUTTON3) != 0U)
        fs |= OS2_MOU_MOUSE_BN3_DOWN;
    return fs;
}

static unsigned short generation_mask(const struct Os2MouHostEvent *host)
{
    unsigned short mask;
    mask = 0U;
    if (host->motion) {
        if ((host->buttons & OS2_MOU_BUTTON1) != 0U)
            mask |= OS2_MOU_MOUSE_MOTION_WITH_BN1_DOWN;
        if ((host->buttons & OS2_MOU_BUTTON2) != 0U)
            mask |= OS2_MOU_MOUSE_MOTION_WITH_BN2_DOWN;
        if ((host->buttons & OS2_MOU_BUTTON3) != 0U)
            mask |= OS2_MOU_MOUSE_MOTION_WITH_BN3_DOWN;
        if ((host->buttons & (OS2_MOU_BUTTON1 | OS2_MOU_BUTTON2 |
                              OS2_MOU_BUTTON3)) == 0U)
            mask |= OS2_MOU_MOUSE_MOTION;
    }
    if ((host->changed_buttons & OS2_MOU_BUTTON1) != 0U)
        mask |= OS2_MOU_MOUSE_BN1_DOWN;
    if ((host->changed_buttons & OS2_MOU_BUTTON2) != 0U)
        mask |= OS2_MOU_MOUSE_BN2_DOWN;
    if ((host->changed_buttons & OS2_MOU_BUTTON3) != 0U)
        mask |= OS2_MOU_MOUSE_BN3_DOWN;
    return mask;
}

static void accept_host_event(struct Os2MouSession *session,
                              const struct Os2MouHostEvent *host)
{
    struct Os2MouEventInfo event;
    unsigned short generated;
    signed short dr;
    signed short dc;

    if (session == NULL || host == NULL)
        return;

    dr = 0;
    dc = 0;
    if (session->have_last_host_pointer) {
        dr = (signed short)((signed long)host->row -
                            (signed long)session->last_host_pointer.row);
        dc = (signed short)((signed long)host->col -
                            (signed long)session->last_host_pointer.col);
    }
    session->last_host_pointer.row = host->row;
    session->last_host_pointer.col = host->col;
    session->have_last_host_pointer = 1;
    session->pointer.row = host->row;
    session->pointer.col = host->col;

    if ((session->dev_status & OS2_MOU_DISABLED) != 0U)
        return;

    generated = generation_mask(host);
    if ((generated & session->event_mask) == 0U)
        return;

    memset(&event, 0, sizeof(event));
    event.fs = event_fs_from_host(host);
    event.time = host->time;
    if ((session->dev_status & OS2_MOU_STATUS_MICKEYS) != 0U) {
        event.row = (unsigned short)dr;
        event.col = (unsigned short)dc;
    } else {
        event.row = host->row;
        event.col = host->col;
    }
    enqueue_event(session, &event);
}

static Os2MouApiRet pump_one(struct Os2MouSession *session, int wait,
                             int *present)
{
    struct Os2MouHostEvent host;
    Os2MouApiRet rc;
    int host_present;

    if (present == NULL)
        return OS2_MOU_ERROR_INVALID_PARAMETER;
    *present = 0;
    if (!mou_ready(session))
        return OS2_MOU_ERROR_MOUSE_NO_DEVICE;

    memset(&host, 0, sizeof(host));
    host_present = 0;
    rc = session->backend->read_event(session->backend_opaque, wait,
                                      &host, &host_present);
    if (rc != OS2_MOU_NO_ERROR)
        return rc;
    if (host_present) {
        accept_host_event(session, &host);
        *present = 1;
    }
    return OS2_MOU_NO_ERROR;
}

static Os2MouApiRet drain_available(struct Os2MouSession *session)
{
    Os2MouApiRet rc;
    int present;
    unsigned int guard;

    for (guard = 0U; guard < OS2_MOU_MAX_EVENTS; ++guard) {
        present = 0;
        rc = pump_one(session, 0, &present);
        if (rc != OS2_MOU_NO_ERROR)
            return rc;
        if (!present)
            break;
    }
    return OS2_MOU_NO_ERROR;
}

void os2_mou_session_init(struct Os2MouSession *session,
                          void *backend_opaque,
                          const struct Os2MouBackendOps *backend)
{
    if (session == NULL)
        return;
    memset(session, 0, sizeof(*session));
    session->backend_opaque = backend_opaque;
    session->backend = backend;
    session->next_handle = 1U;
    session->event_mask = OS2_MOU_EVENT_MASK_ALL;
    session->buttons = 2U;
    session->mickeys = 8U;
    session->scale.rowScale = 1U;
    session->scale.colScale = 1U;
    session->threshold.Length = 10U;
    session->threshold.Level1 = 5U;
    session->threshold.Lev1Mult = 2U;
    session->threshold.Level2 = 10U;
    session->threshold.lev2Mult = 4U;
    session->shape.cb = 4U;
    session->shape.col = 1U;
    session->shape.row = 1U;
    session->shape.colHot = 0U;
    session->shape.rowHot = 0U;
    session->shape_bytes[0] = 0xffU;
    session->shape_bytes[1] = 0xffU;
    session->shape_bytes[2] = 0x00U;
    session->shape_bytes[3] = 0x70U;
}

Os2MouApiRet os2_mou_MouOpen(struct Os2MouSession *session,
                              const char *driver_name,
                              unsigned short *hmou)
{
    Os2MouApiRet rc;
    unsigned short buttons;
    unsigned short mickeys;
    unsigned short row;
    unsigned short col;
    (void)driver_name;

    if (hmou == NULL)
        return OS2_MOU_ERROR_MOUSE_INV_PARMS;
    if (!mou_ready(session))
        return OS2_MOU_ERROR_MOUSE_NO_DEVICE;

    if (session->open_count == 0U) {
        buttons = session->buttons;
        mickeys = session->mickeys;
        row = session->pointer.row;
        col = session->pointer.col;
        rc = session->backend->activate(session->backend_opaque,
                                        &buttons, &mickeys, &row, &col);
        if (rc != OS2_MOU_NO_ERROR)
            return rc;
        if (buttons != 0U)
            session->buttons = buttons;
        if (mickeys != 0U)
            session->mickeys = mickeys;
        session->pointer.row = row;
        session->pointer.col = col;
        session->last_host_pointer = session->pointer;
        session->have_last_host_pointer = 1;
        clear_queue(session);
    }
    if (!alloc_handle(session, hmou)) {
        if (session->open_count == 0U)
            (void)session->backend->deactivate(session->backend_opaque);
        return OS2_MOU_ERROR_MOUSE_NO_DEVICE;
    }
    ++session->open_count;
    return OS2_MOU_NO_ERROR;
}

Os2MouApiRet os2_mou_MouClose(struct Os2MouSession *session,
                               unsigned short hmou)
{
    Os2MouApiRet rc;
    if (!valid_handle(session, hmou))
        return OS2_MOU_ERROR_MOUSE_INV_HANDLE;
    free_handle(session, hmou);
    if (session->open_count != 0U)
        --session->open_count;
    if (session->open_count == 0U && mou_ready(session)) {
        clear_queue(session);
        rc = session->backend->deactivate(session->backend_opaque);
        if (rc != OS2_MOU_NO_ERROR)
            return rc;
    }
    return OS2_MOU_NO_ERROR;
}

Os2MouApiRet os2_mou_MouFlushQue(struct Os2MouSession *session,
                                  unsigned short hmou)
{
    Os2MouApiRet rc;
    if (!valid_handle(session, hmou))
        return OS2_MOU_ERROR_MOUSE_INV_HANDLE;
    clear_queue(session);
    rc = session->backend->flush_events(session->backend_opaque);
    return rc;
}

Os2MouApiRet os2_mou_MouGetPtrPos(struct Os2MouSession *session,
                                   struct Os2MouPtrLoc *loc,
                                   unsigned short hmou)
{
    if (loc == NULL)
        return OS2_MOU_ERROR_MOUSE_INV_PARMS;
    if (!valid_handle(session, hmou))
        return OS2_MOU_ERROR_MOUSE_INV_HANDLE;
    (void)drain_available(session);
    *loc = session->pointer;
    return OS2_MOU_NO_ERROR;
}

Os2MouApiRet os2_mou_MouSetPtrPos(struct Os2MouSession *session,
                                   const struct Os2MouPtrLoc *loc,
                                   unsigned short hmou)
{
    Os2MouApiRet rc;
    if (loc == NULL)
        return OS2_MOU_ERROR_MOUSE_INV_PARMS;
    if (!valid_handle(session, hmou))
        return OS2_MOU_ERROR_MOUSE_INV_HANDLE;
    session->pointer = *loc;
    session->last_host_pointer = *loc;
    session->have_last_host_pointer = 1;
    if (session->backend->set_pointer_position == NULL)
        return OS2_MOU_NO_ERROR;
    rc = session->backend->set_pointer_position(session->backend_opaque,
                                                loc->row, loc->col);
    return rc;
}

Os2MouApiRet os2_mou_MouGetPtrShape(struct Os2MouSession *session,
                                     unsigned char *buffer,
                                     struct Os2MouPtrShape *shape,
                                     unsigned short hmou)
{
    unsigned short provided;
    if (shape == NULL)
        return OS2_MOU_ERROR_MOUSE_INV_PARMS;
    if (!valid_handle(session, hmou))
        return OS2_MOU_ERROR_MOUSE_INV_HANDLE;
    provided = shape->cb;
    *shape = session->shape;
    if (provided < session->shape.cb || buffer == NULL)
        return OS2_MOU_ERROR_MOUSE_INV_PARMS;
    memcpy(buffer, session->shape_bytes, session->shape.cb);
    return OS2_MOU_NO_ERROR;
}

Os2MouApiRet os2_mou_MouSetPtrShape(struct Os2MouSession *session,
                                     const unsigned char *buffer,
                                     const struct Os2MouPtrShape *shape,
                                     unsigned short hmou)
{
    if (shape == NULL || (shape->cb != 0U && buffer == NULL) ||
        shape->cb > OS2_MOU_MAX_SHAPE_BYTES)
        return OS2_MOU_ERROR_MOUSE_INV_PARMS;
    if (!valid_handle(session, hmou))
        return OS2_MOU_ERROR_MOUSE_INV_HANDLE;
    session->shape = *shape;
    if (shape->cb != 0U)
        memcpy(session->shape_bytes, buffer, shape->cb);
    return OS2_MOU_NO_ERROR;
}

Os2MouApiRet os2_mou_MouGetDevStatus(struct Os2MouSession *session,
                                      unsigned short *status,
                                      unsigned short hmou)
{
    if (status == NULL)
        return OS2_MOU_ERROR_MOUSE_INV_PARMS;
    if (!valid_handle(session, hmou))
        return OS2_MOU_ERROR_MOUSE_INV_HANDLE;
    *status = session->dev_status;
    return OS2_MOU_NO_ERROR;
}

Os2MouApiRet os2_mou_MouSetDevStatus(struct Os2MouSession *session,
                                      const unsigned short *status,
                                      unsigned short hmou)
{
    unsigned short allowed;
    if (status == NULL)
        return OS2_MOU_ERROR_MOUSE_INV_PARMS;
    if (!valid_handle(session, hmou))
        return OS2_MOU_ERROR_MOUSE_INV_HANDLE;
    allowed = (unsigned short)(OS2_MOU_DISABLED | OS2_MOU_STATUS_MICKEYS);
    if ((*status & (unsigned short)~allowed) != 0U)
        return OS2_MOU_ERROR_MOUSE_INV_PARMS;
    session->dev_status = *status;
    return OS2_MOU_NO_ERROR;
}

Os2MouApiRet os2_mou_MouGetNumButtons(struct Os2MouSession *session,
                                       unsigned short *buttons,
                                       unsigned short hmou)
{
    if (buttons == NULL)
        return OS2_MOU_ERROR_MOUSE_INV_PARMS;
    if (!valid_handle(session, hmou))
        return OS2_MOU_ERROR_MOUSE_INV_HANDLE;
    *buttons = session->buttons;
    return OS2_MOU_NO_ERROR;
}

Os2MouApiRet os2_mou_MouGetNumMickeys(struct Os2MouSession *session,
                                       unsigned short *mickeys,
                                       unsigned short hmou)
{
    if (mickeys == NULL)
        return OS2_MOU_ERROR_MOUSE_INV_PARMS;
    if (!valid_handle(session, hmou))
        return OS2_MOU_ERROR_MOUSE_INV_HANDLE;
    *mickeys = session->mickeys;
    return OS2_MOU_NO_ERROR;
}

Os2MouApiRet os2_mou_MouReadEventQue(struct Os2MouSession *session,
                                      struct Os2MouEventInfo *event,
                                      unsigned short *wait,
                                      unsigned short hmou)
{
    Os2MouApiRet rc;
    int added;
    if (event == NULL || wait == NULL)
        return OS2_MOU_ERROR_MOUSE_INV_PARMS;
    if (!valid_handle(session, hmou))
        return OS2_MOU_ERROR_MOUSE_INV_HANDLE;
    if (*wait != OS2_MOU_NOWAIT && *wait != OS2_MOU_WAIT)
        return OS2_MOU_ERROR_INVALID_IOWAIT;

    memset(event, 0, sizeof(*event));
    rc = drain_available(session);
    if (rc != OS2_MOU_NO_ERROR)
        return rc;
    if (dequeue_event(session, event))
        return OS2_MOU_NO_ERROR;
    if (*wait == OS2_MOU_NOWAIT)
        return OS2_MOU_NO_ERROR_MOUSE_NO_DATA;

    for (;;) {
        added = 0;
        rc = pump_one(session, 1, &added);
        if (rc != OS2_MOU_NO_ERROR)
            return rc;
        if (dequeue_event(session, event))
            return OS2_MOU_NO_ERROR;
    }
}

Os2MouApiRet os2_mou_try_read(struct Os2MouSession *session,
                               struct Os2MouEventInfo *event,
                               int *present,
                               unsigned short hmou)
{
    unsigned short wait;
    Os2MouApiRet rc;
    if (present == NULL || event == NULL)
        return OS2_MOU_ERROR_MOUSE_INV_PARMS;
    *present = 0;
    wait = OS2_MOU_NOWAIT;
    rc = os2_mou_MouReadEventQue(session, event, &wait, hmou);
    if (rc == OS2_MOU_NO_ERROR) {
        *present = 1;
        return rc;
    }
    if (rc == OS2_MOU_NO_ERROR_MOUSE_NO_DATA) {
        memset(event, 0, sizeof(*event));
        return OS2_MOU_NO_ERROR;
    }
    return rc;
}

Os2MouApiRet os2_mou_MouGetNumQueEl(struct Os2MouSession *session,
                                     struct Os2MouQueInfo *info,
                                     unsigned short hmou)
{
    Os2MouApiRet rc;
    if (info == NULL)
        return OS2_MOU_ERROR_MOUSE_INV_PARMS;
    if (!valid_handle(session, hmou))
        return OS2_MOU_ERROR_MOUSE_INV_HANDLE;
    rc = drain_available(session);
    if (rc != OS2_MOU_NO_ERROR)
        return rc;
    info->cEvents = session->event_count;
    info->cmaxEvents = OS2_MOU_MAX_EVENTS;
    return OS2_MOU_NO_ERROR;
}

Os2MouApiRet os2_mou_MouGetEventMask(struct Os2MouSession *session,
                                      unsigned short *mask,
                                      unsigned short hmou)
{
    if (mask == NULL)
        return OS2_MOU_ERROR_MOUSE_INV_PARMS;
    if (!valid_handle(session, hmou))
        return OS2_MOU_ERROR_MOUSE_INV_HANDLE;
    *mask = session->event_mask;
    return OS2_MOU_NO_ERROR;
}

Os2MouApiRet os2_mou_MouSetEventMask(struct Os2MouSession *session,
                                      const unsigned short *mask,
                                      unsigned short hmou)
{
    if (mask == NULL || (*mask & (unsigned short)~OS2_MOU_EVENT_MASK_ALL) != 0U)
        return OS2_MOU_ERROR_MOUSE_INV_PARMS;
    if (!valid_handle(session, hmou))
        return OS2_MOU_ERROR_MOUSE_INV_HANDLE;
    session->event_mask = *mask;
    return OS2_MOU_NO_ERROR;
}

Os2MouApiRet os2_mou_MouGetScaleFact(struct Os2MouSession *session,
                                      struct Os2MouScaleFact *scale,
                                      unsigned short hmou)
{
    if (scale == NULL)
        return OS2_MOU_ERROR_MOUSE_INV_PARMS;
    if (!valid_handle(session, hmou))
        return OS2_MOU_ERROR_MOUSE_INV_HANDLE;
    *scale = session->scale;
    return OS2_MOU_NO_ERROR;
}

Os2MouApiRet os2_mou_MouSetScaleFact(struct Os2MouSession *session,
                                      const struct Os2MouScaleFact *scale,
                                      unsigned short hmou)
{
    if (scale == NULL || scale->rowScale == 0U || scale->colScale == 0U)
        return OS2_MOU_ERROR_MOUSE_INV_PARMS;
    if (!valid_handle(session, hmou))
        return OS2_MOU_ERROR_MOUSE_INV_HANDLE;
    session->scale = *scale;
    return OS2_MOU_NO_ERROR;
}

Os2MouApiRet os2_mou_MouGetThreshold(struct Os2MouSession *session,
                                      struct Os2MouThreshold *threshold,
                                      unsigned short hmou)
{
    if (threshold == NULL)
        return OS2_MOU_ERROR_MOUSE_INV_PARMS;
    if (!valid_handle(session, hmou))
        return OS2_MOU_ERROR_MOUSE_INV_HANDLE;
    *threshold = session->threshold;
    return OS2_MOU_NO_ERROR;
}

Os2MouApiRet os2_mou_MouSetThreshold(struct Os2MouSession *session,
                                      const struct Os2MouThreshold *threshold,
                                      unsigned short hmou)
{
    if (threshold == NULL || threshold->Length != 10U)
        return OS2_MOU_ERROR_MOUSE_INV_PARMS;
    if (!valid_handle(session, hmou))
        return OS2_MOU_ERROR_MOUSE_INV_HANDLE;
    session->threshold = *threshold;
    return OS2_MOU_NO_ERROR;
}

Os2MouApiRet os2_mou_MouRemovePtr(struct Os2MouSession *session,
                                   const struct Os2MouNoPtrRect *rect,
                                   unsigned short hmou)
{
    if (rect == NULL)
        return OS2_MOU_ERROR_MOUSE_INV_PARMS;
    if (!valid_handle(session, hmou))
        return OS2_MOU_ERROR_MOUSE_INV_HANDLE;
    session->exclusion = *rect;
    session->exclusion_active = 1;
    session->pointer_drawn = 0;
    return OS2_MOU_NO_ERROR;
}

Os2MouApiRet os2_mou_MouDrawPtr(struct Os2MouSession *session,
                                 unsigned short hmou)
{
    if (!valid_handle(session, hmou))
        return OS2_MOU_ERROR_MOUSE_INV_HANDLE;
    session->exclusion_active = 0;
    session->pointer_drawn = 1;
    return OS2_MOU_NO_ERROR;
}

Os2MouApiRet os2_mou_MouSynch(struct Os2MouSession *session,
                               unsigned short wait)
{
    if (session == NULL)
        return OS2_MOU_ERROR_MOUSE_NO_DEVICE;
    if (wait != OS2_MOU_NOWAIT && wait != OS2_MOU_WAIT)
        return OS2_MOU_ERROR_INVALID_IOWAIT;
    return OS2_MOU_NO_ERROR;
}

Os2MouApiRet os2_mou_MouInitReal(struct Os2MouSession *session,
                                  const char *driver_name)
{
    (void)driver_name;
    if (session == NULL)
        return OS2_MOU_ERROR_MOUSE_NO_DEVICE;
    return OS2_MOU_NO_ERROR;
}

Os2MouApiRet os2_mou_MouRegister(struct Os2MouSession *session,
                                  const char *module_name,
                                  const char *entry_name,
                                  uint32_t functions)
{
    (void)session;
    (void)module_name;
    (void)entry_name;
    (void)functions;
    return OS2_MOU_ERROR_MOUSE_REGISTER;
}

Os2MouApiRet os2_mou_MouDeRegister(struct Os2MouSession *session)
{
    (void)session;
    return OS2_MOU_ERROR_MOUSE_DEREGISTER;
}
