#include <stdio.h>
#include <string.h>

#include "os2_kbd.h"
#include "os2_kbd_backend.h"
#include "os2_kbd_win32.h"

static struct Os2KbdSession session;
static struct Os2KbdHostEvent queued;
static int has_key;
static int failures;

static Os2KbdApiRet fake_read(void *opaque, int wait,
                              struct Os2KbdHostEvent *event, int *present)
{
    (void)opaque; (void)wait;
    *present = has_key;
    if (has_key) {
        *event = queued;
        has_key = 0;
    }
    return 0U;
}
static Os2KbdApiRet fake_peek(void *opaque,
                              struct Os2KbdHostEvent *event, int *present)
{
    (void)opaque;
    *present = has_key;
    if (has_key)
        *event = queued;
    return 0U;
}
static Os2KbdApiRet fake_flush(void *opaque)
{
    (void)opaque; has_key = 0; return 0U;
}
static Os2KbdApiRet fake_echo(void *opaque, const char *bytes,
                              unsigned short count)
{
    (void)opaque; (void)bytes; (void)count; return 0U;
}
static unsigned long fake_ms(void *opaque)
{
    (void)opaque; return 999UL;
}
static const struct Os2KbdBackendOps ops = {
    fake_read, fake_peek, fake_flush, fake_echo, fake_ms
};

struct Os2KbdSession *os2_kbd_win32_session(void)
{
    return &session;
}

unsigned short __cdecl KbdCharIn(struct Os2KbdKeyInfo *, unsigned short,
                                  unsigned short);
unsigned short __cdecl KbdStringIn(char *, struct Os2KbdStringInBuf *,
                                    unsigned short, unsigned short);
unsigned short __cdecl KbdGetStatus(struct Os2KbdInfo *, unsigned short);
unsigned short __cdecl KbdSetStatus(const struct Os2KbdInfo *, unsigned short);
unsigned short __cdecl KbdFlushBuffer(unsigned short);
unsigned short __cdecl KbdPeek(struct Os2KbdKeyInfo *, unsigned short);

static void check(int ok, const char *message)
{
    if (!ok) {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

int main(void)
{
    struct Os2KbdKeyInfo key;
    struct Os2KbdInfo status;

    os2_kbd_session_init(&session, NULL, &ops);
    memset(&queued, 0, sizeof(queued));
    queued.ch_char = 'A';
    queued.ch_scan = 30U;
    queued.fs_state = 3U;
    queued.time = 123UL;
    has_key = 1;

    memset(&key, 0, sizeof(key));
    check(KbdPeek(&key, 0U) == 0U && key.chChar == 'A' && has_key,
          "veneer KbdPeek routes through common session without consuming");
    check(KbdCharIn(&key, OS2_KBD_IO_NOWAIT, 0U) == 0U &&
          key.chChar == 'A' && key.chScan == 30U && !has_key,
          "veneer KbdCharIn routes through common session");

    memset(&status, 0, sizeof(status));
    status.cb = 10U;
    check(KbdGetStatus(&status, 0U) == 0U && status.fsMask == 0x0009U,
          "veneer KbdGetStatus uses common state");
    status.fsMask = 0x0004U;
    check(KbdSetStatus(&status, 0U) == 0U,
          "veneer KbdSetStatus updates common state");
    memset(&status, 0, sizeof(status)); status.cb = 10U;
    check(KbdGetStatus(&status, 0U) == 0U && status.fsMask == 0x0004U,
          "veneer status round trip");

    has_key = 1;
    check(KbdFlushBuffer(0U) == 0U && !has_key,
          "veneer KbdFlushBuffer routes to backend through common layer");

    if (failures != 0) {
        fprintf(stderr, "kbd-veneer-check: %d failure(s)\n", failures);
        return 1;
    }
    printf("kbd-veneer-check: PASS\n");
    return 0;
}
