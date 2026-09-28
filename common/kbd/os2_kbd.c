#include <string.h>

#include "os2_kbd.h"
#include "os2_kbd_backend.h"

static int kbd_ready(const struct Os2KbdSession *session)
{
    return session != NULL && session->backend != NULL &&
           session->backend->read_event != NULL &&
           session->backend->peek_event != NULL &&
           session->backend->flush != NULL &&
           session->backend->echo_bytes != NULL &&
           session->backend->milliseconds != NULL;
}

static void clear_key_info(struct Os2KbdSession *session,
                           struct Os2KbdKeyInfo *info)
{
    memset(info, 0, sizeof(*info));
    if (kbd_ready(session))
        info->time = (uint32_t)session->backend->milliseconds(session->backend_opaque);
}

static void copy_event(struct Os2KbdKeyInfo *info,
                       const struct Os2KbdHostEvent *event)
{
    info->chChar = event->ch_char;
    info->chScan = event->ch_scan;
    info->fbStatus = OS2_KBD_STATUS_CHAR_IN;
    info->bNlsShift = 0U;
    info->fsState = event->fs_state;
    info->time = event->time;
}

void os2_kbd_session_init(struct Os2KbdSession *session,
                          void *backend_opaque,
                          const struct Os2KbdBackendOps *backend)
{
    if (session == NULL)
        return;
    memset(session, 0, sizeof(*session));
    session->backend_opaque = backend_opaque;
    session->backend = backend;
    session->status.cb = 10U;
    session->status.fsMask = 0x0009U;
    session->status.chTurnAround = 13U;
    session->status.fsInterim = 0U;
    session->status.fsState = 0U;
}

Os2KbdApiRet os2_kbd_KbdCharIn(struct Os2KbdSession *session,
                                struct Os2KbdKeyInfo *info,
                                unsigned short wait,
                                unsigned short hkbd)
{
    struct Os2KbdHostEvent event;
    Os2KbdApiRet rc;
    int present;
    (void)hkbd;

    if (info == NULL)
        return OS2_KBD_ERROR_INVALID_PARAMETER;
    clear_key_info(session, info);
    if (!kbd_ready(session))
        return OS2_KBD_ERROR_INVALID_HANDLE;

    memset(&event, 0, sizeof(event));
    present = 0;
    rc = session->backend->read_event(session->backend_opaque,
                                      wait == OS2_KBD_IO_NOWAIT ? 0 : 1,
                                      &event, &present);
    if (rc != OS2_KBD_NO_ERROR)
        return rc;
    if (present)
        copy_event(info, &event);
    return OS2_KBD_NO_ERROR;
}

Os2KbdApiRet os2_kbd_try_char(struct Os2KbdSession *session,
                               struct Os2KbdKeyInfo *info,
                               int *present)
{
    Os2KbdApiRet rc;
    if (present == NULL)
        return OS2_KBD_ERROR_INVALID_PARAMETER;
    *present = 0;
    rc = os2_kbd_KbdCharIn(session, info, OS2_KBD_IO_NOWAIT, 0U);
    if (rc == OS2_KBD_NO_ERROR && info != NULL &&
        info->fbStatus != 0U)
        *present = 1;
    return rc;
}

Os2KbdApiRet os2_kbd_KbdStringIn(struct Os2KbdSession *session,
                                  char *buffer,
                                  struct Os2KbdStringInBuf *length,
                                  unsigned short wait,
                                  unsigned short hkbd)
{
    struct Os2KbdHostEvent event;
    unsigned short cap;
    unsigned short used;
    Os2KbdApiRet rc;
    int present;
    unsigned char c;
    static const char crlf[2] = {'\r', '\n'};
    static const char erase[3] = {'\b', ' ', '\b'};
    (void)hkbd;

    if (buffer == NULL || length == NULL)
        return OS2_KBD_ERROR_INVALID_PARAMETER;
    if (!kbd_ready(session))
        return OS2_KBD_ERROR_INVALID_HANDLE;

    cap = length->cb;
    if (cap > 255U)
        cap = 255U;
    length->cchIn = 0U;
    if (cap == 0U)
        return OS2_KBD_NO_ERROR;

    used = 0U;
    for (;;) {
        memset(&event, 0, sizeof(event));
        present = 0;
        rc = session->backend->read_event(session->backend_opaque,
                                          wait == OS2_KBD_IO_NOWAIT ? 0 : 1,
                                          &event, &present);
        if (rc != OS2_KBD_NO_ERROR)
            return rc;
        if (!present) {
            length->cchIn = used;
            return OS2_KBD_NO_ERROR;
        }

        c = event.ch_char;
        if (c == 0U)
            continue;
        if (c == '\r' || c == '\n') {
            if (event.echoable)
                (void)session->backend->echo_bytes(session->backend_opaque,
                                                   crlf, 2U);
            length->cchIn = used;
            return OS2_KBD_NO_ERROR;
        }
        if (c == '\b') {
            if (used != 0U) {
                --used;
                if (event.echoable)
                    (void)session->backend->echo_bytes(session->backend_opaque,
                                                       erase, 3U);
            }
            continue;
        }
        if (used < cap) {
            buffer[used++] = (char)c;
            if (event.echoable)
                (void)session->backend->echo_bytes(session->backend_opaque,
                                                   (const char *)&c, 1U);
            /* Preserve the pre-split native behavior: redirected byte
             * input returns when the buffer fills; the console path keeps
             * consuming/editing until CR/LF even when no more bytes fit. */
            if (used == cap && !event.echoable) {
                length->cchIn = used;
                return OS2_KBD_NO_ERROR;
            }
        }
    }
}

Os2KbdApiRet os2_kbd_KbdGetStatus(struct Os2KbdSession *session,
                                   struct Os2KbdInfo *info,
                                   unsigned short hkbd)
{
    (void)hkbd;
    if (info == NULL)
        return OS2_KBD_ERROR_INVALID_PARAMETER;
    if (info->cb != 10U)
        return OS2_KBD_ERROR_INVALID_LENGTH;
    if (session == NULL)
        return OS2_KBD_ERROR_INVALID_HANDLE;
    info->fsMask = session->status.fsMask;
    info->chTurnAround = session->status.chTurnAround;
    info->fsInterim = session->status.fsInterim;
    info->fsState = session->status.fsState;
    return OS2_KBD_NO_ERROR;
}

Os2KbdApiRet os2_kbd_KbdSetStatus(struct Os2KbdSession *session,
                                   const struct Os2KbdInfo *info,
                                   unsigned short hkbd)
{
    (void)hkbd;
    if (info == NULL)
        return OS2_KBD_ERROR_INVALID_PARAMETER;
    if (info->cb != 10U)
        return OS2_KBD_ERROR_INVALID_LENGTH;
    if (session == NULL)
        return OS2_KBD_ERROR_INVALID_HANDLE;
    session->status = *info;
    return OS2_KBD_NO_ERROR;
}

Os2KbdApiRet os2_kbd_KbdPeek(struct Os2KbdSession *session,
                              struct Os2KbdKeyInfo *info,
                              unsigned short hkbd)
{
    struct Os2KbdHostEvent event;
    Os2KbdApiRet rc;
    int present;
    (void)hkbd;

    if (info == NULL)
        return OS2_KBD_ERROR_INVALID_PARAMETER;
    clear_key_info(session, info);
    if (!kbd_ready(session))
        return OS2_KBD_ERROR_INVALID_HANDLE;
    memset(&event, 0, sizeof(event));
    present = 0;
    rc = session->backend->peek_event(session->backend_opaque,
                                      &event, &present);
    if (rc != OS2_KBD_NO_ERROR)
        return rc;
    if (present)
        copy_event(info, &event);
    return OS2_KBD_NO_ERROR;
}

Os2KbdApiRet os2_kbd_KbdFlushBuffer(struct Os2KbdSession *session,
                                     unsigned short hkbd)
{
    (void)hkbd;
    if (!kbd_ready(session))
        return OS2_KBD_ERROR_INVALID_HANDLE;
    return session->backend->flush(session->backend_opaque);
}
