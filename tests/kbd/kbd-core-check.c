#include <stdio.h>
#include <string.h>

#include "os2_kbd.h"
#include "os2_kbd_backend.h"

#define MAX_EVENTS 32

struct FakeBackend {
    struct Os2KbdHostEvent events[MAX_EVENTS];
    unsigned int head;
    unsigned int count;
    unsigned long clock;
    char echo[256];
    unsigned int echo_len;
    unsigned int read_calls;
    unsigned int peek_calls;
    unsigned int flush_calls;
    int last_wait;
};

static int failures;

static void check(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

static void push_event(struct FakeBackend *f, unsigned char ch,
                       unsigned char scan, unsigned short state,
                       unsigned long time, int echoable)
{
    unsigned int pos;
    pos = (f->head + f->count) % MAX_EVENTS;
    memset(&f->events[pos], 0, sizeof(f->events[pos]));
    f->events[pos].ch_char = ch;
    f->events[pos].ch_scan = scan;
    f->events[pos].fs_state = state;
    f->events[pos].time = (uint32_t)time;
    f->events[pos].echoable = echoable;
    ++f->count;
}

static Os2KbdApiRet fake_read(void *opaque, int wait,
                              struct Os2KbdHostEvent *event, int *present)
{
    struct FakeBackend *f;
    f = (struct FakeBackend *)opaque;
    ++f->read_calls;
    f->last_wait = wait;
    *present = 0;
    memset(event, 0, sizeof(*event));
    if (f->count == 0U)
        return OS2_KBD_NO_ERROR;
    *event = f->events[f->head];
    f->head = (f->head + 1U) % MAX_EVENTS;
    --f->count;
    *present = 1;
    return OS2_KBD_NO_ERROR;
}

static Os2KbdApiRet fake_peek(void *opaque,
                              struct Os2KbdHostEvent *event, int *present)
{
    struct FakeBackend *f;
    f = (struct FakeBackend *)opaque;
    ++f->peek_calls;
    *present = 0;
    memset(event, 0, sizeof(*event));
    if (f->count == 0U)
        return OS2_KBD_NO_ERROR;
    *event = f->events[f->head];
    *present = 1;
    return OS2_KBD_NO_ERROR;
}

static Os2KbdApiRet fake_flush(void *opaque)
{
    struct FakeBackend *f;
    f = (struct FakeBackend *)opaque;
    ++f->flush_calls;
    f->head = 0U;
    f->count = 0U;
    return OS2_KBD_NO_ERROR;
}

static Os2KbdApiRet fake_echo(void *opaque, const char *bytes,
                              unsigned short count)
{
    struct FakeBackend *f;
    unsigned int n;
    f = (struct FakeBackend *)opaque;
    n = (unsigned int)count;
    if (f->echo_len + n > sizeof(f->echo))
        return OS2_KBD_ERROR_INVALID_PARAMETER;
    memcpy(f->echo + f->echo_len, bytes, n);
    f->echo_len += n;
    return OS2_KBD_NO_ERROR;
}

static unsigned long fake_ms(void *opaque)
{
    return ((struct FakeBackend *)opaque)->clock;
}

static const struct Os2KbdBackendOps fake_ops = {
    fake_read,
    fake_peek,
    fake_flush,
    fake_echo,
    fake_ms
};

int main(void)
{
    struct FakeBackend backend;
    struct Os2KbdSession session;
    struct Os2KbdKeyInfo key;
    struct Os2KbdInfo status;
    struct Os2KbdStringInBuf sib;
    char buffer[16];
    int present;

    memset(&backend, 0, sizeof(backend));
    backend.clock = 777UL;
    os2_kbd_session_init(&session, &backend, &fake_ops);

    memset(&status, 0, sizeof(status));
    status.cb = 10U;
    check(os2_kbd_KbdGetStatus(&session, &status, 0U) == 0U,
          "get default status");
    check(status.fsMask == 0x0009U && status.chTurnAround == 13U,
          "default echo/ASCII status and CR turnaround preserved");
    status.fsMask = 0x0004U;
    status.chTurnAround = 10U;
    status.fsState = 0x55U;
    check(os2_kbd_KbdSetStatus(&session, &status, 0U) == 0U,
          "set status in common session");
    memset(&status, 0, sizeof(status));
    status.cb = 10U;
    check(os2_kbd_KbdGetStatus(&session, &status, 0U) == 0U &&
          status.fsMask == 0x0004U && status.chTurnAround == 10U &&
          status.fsState == 0x55U,
          "get returns common-owned status");
    status.cb = 8U;
    check(os2_kbd_KbdGetStatus(&session, &status, 0U) ==
          OS2_KBD_ERROR_INVALID_LENGTH,
          "KBDINFO length validation preserved");

    memset(&key, 0xff, sizeof(key));
    check(os2_kbd_KbdCharIn(&session, &key, OS2_KBD_IO_NOWAIT, 0U) == 0U,
          "NOWAIT empty succeeds");
    check(key.fbStatus == 0U && key.time == 777UL && backend.last_wait == 0,
          "NOWAIT empty reports no character and current time");

    push_event(&backend, 0U, 72U, 0x0004U, 1234UL, 1);
    check(os2_kbd_KbdCharIn(&session, &key, OS2_KBD_IO_WAIT, 0U) == 0U,
          "WAIT character read succeeds");
    check(key.chChar == 0U && key.chScan == 72U && key.fbStatus == 0x40U &&
          key.fsState == 0x0004U && key.time == 1234UL && backend.last_wait == 1,
          "extended key data copied into OS/2 KBDKEYINFO");

    push_event(&backend, 'X', 45U, 0x0003U, 2000UL, 1);
    check(os2_kbd_KbdPeek(&session, &key, 0U) == 0U &&
          key.chChar == 'X' && key.fbStatus == 0x40U,
          "peek reports queued key");
    check(backend.count == 1U, "peek does not consume key");
    check(os2_kbd_KbdCharIn(&session, &key, OS2_KBD_IO_NOWAIT, 0U) == 0U &&
          key.chChar == 'X' && backend.count == 0U,
          "char-in consumes key after peek");

    push_event(&backend, 'a', 0U, 0U, 1UL, 1);
    push_event(&backend, 'b', 0U, 0U, 2UL, 1);
    push_event(&backend, '\b', 0U, 0U, 3UL, 1);
    push_event(&backend, 'c', 0U, 0U, 4UL, 1);
    push_event(&backend, '\r', 0U, 0U, 5UL, 1);
    memset(buffer, 0, sizeof(buffer));
    sib.cb = (unsigned short)sizeof(buffer);
    sib.cchIn = 99U;
    backend.echo_len = 0U;
    check(os2_kbd_KbdStringIn(&session, buffer, &sib,
                              OS2_KBD_IO_WAIT, 0U) == 0U,
          "string input succeeds");
    check(sib.cchIn == 2U && buffer[0] == 'a' && buffer[1] == 'c',
          "string input handles backspace and excludes CR");
    check(backend.echo_len == 8U &&
          memcmp(backend.echo, "ab\b \bc\r\n", 8U) == 0,
          "console-style string input echo preserved");

    push_event(&backend, 'q', 16U, 0U, 88UL, 0);
    present = 0;
    check(os2_kbd_try_char(&session, &key, &present) == 0U &&
          present && key.chChar == 'q',
          "scheduler-friendly try-char consumes available key");
    present = 1;
    check(os2_kbd_try_char(&session, &key, &present) == 0U && !present,
          "scheduler-friendly try-char reports empty without blocking");

    push_event(&backend, 'z', 0U, 0U, 0UL, 1);
    check(os2_kbd_KbdFlushBuffer(&session, 0U) == 0U &&
          backend.count == 0U && backend.flush_calls == 1U,
          "flush delegated to backend and removes host input");

    if (failures != 0) {
        fprintf(stderr, "kbd-core-check: %d failure(s)\n", failures);
        return 1;
    }
    printf("kbd-core-check: PASS\n");
    return 0;
}
