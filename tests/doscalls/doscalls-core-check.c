#include <stdio.h>
#include <string.h>

#include "os2_doscalls.h"
#include "os2_doscalls_backend.h"

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
    return 1; } } while (0)

struct FakeBackend {
    unsigned int dispatch_id;
    O2ULONG slept;
    unsigned int close_native_count;
    unsigned int close_find_count;
    unsigned int event_close_count;
    O2NATIVE next_event;
    int event_state[256];
    struct O2ExceptionRegistrationRecord *exception_head;
};

static void fake_lock(void *opaque) { (void)opaque; }
static void fake_unlock(void *opaque) { (void)opaque; }
static O2APIRET fake_dispatch(void *opaque, unsigned int call_id, void *call_args)
{
    struct FakeBackend *b;
    (void)call_args;
    b = (struct FakeBackend *)opaque;
    b->dispatch_id = call_id;
    return (O2APIRET)(0x6000UL + call_id);
}
static O2APIRET fake_standard(void *opaque, O2HFILE hfile, O2NATIVE *native_handle)
{
    (void)opaque;
    if (native_handle == NULL || hfile > 2UL)
        return O2_ERROR_INVALID_HANDLE;
    *native_handle = (O2NATIVE)(10UL + hfile);
    return O2_NO_ERROR;
}
static O2APIRET fake_close_native(void *opaque, O2NATIVE native_handle)
{
    struct FakeBackend *b;
    (void)native_handle;
    b = (struct FakeBackend *)opaque;
    ++b->close_native_count;
    return O2_NO_ERROR;
}
static O2APIRET fake_close_find(void *opaque, O2NATIVE native_handle)
{
    struct FakeBackend *b;
    (void)native_handle;
    b = (struct FakeBackend *)opaque;
    ++b->close_find_count;
    return O2_NO_ERROR;
}
static O2APIRET fake_sleep(void *opaque, O2ULONG milliseconds)
{
    struct FakeBackend *b;
    b = (struct FakeBackend *)opaque;
    b->slept = milliseconds;
    return O2_NO_ERROR;
}
static O2APIRET fake_validate(void *opaque, const void *base, O2ULONG size)
{
    (void)opaque;
    return (base != NULL && size != 0UL) ? O2_NO_ERROR : O2_ERROR_INVALID_PARAMETER;
}
static O2APIRET fake_event_create(void *opaque, int initial_state, O2NATIVE *native_event)
{
    struct FakeBackend *b;
    O2NATIVE n;
    b = (struct FakeBackend *)opaque;
    if (native_event == NULL)
        return O2_ERROR_INVALID_PARAMETER;
    n = b->next_event++;
    if (n >= (O2NATIVE)256UL)
        return O2_ERROR_TOO_MANY_OPENS;
    b->event_state[(unsigned int)n] = initial_state != 0;
    *native_event = n;
    return O2_NO_ERROR;
}
static O2APIRET fake_event_reset(void *opaque, O2NATIVE native_event)
{
    struct FakeBackend *b;
    b = (struct FakeBackend *)opaque;
    b->event_state[(unsigned int)native_event] = 0;
    return O2_NO_ERROR;
}
static O2APIRET fake_event_post(void *opaque, O2NATIVE native_event)
{
    struct FakeBackend *b;
    b = (struct FakeBackend *)opaque;
    b->event_state[(unsigned int)native_event] = 1;
    return O2_NO_ERROR;
}
static O2APIRET fake_event_wait(void *opaque, O2NATIVE native_event, O2ULONG timeout)
{
    struct FakeBackend *b;
    (void)timeout;
    b = (struct FakeBackend *)opaque;
    return b->event_state[(unsigned int)native_event] ? O2_NO_ERROR : 640UL;
}
static O2APIRET fake_event_close(void *opaque, O2NATIVE native_event)
{
    struct FakeBackend *b;
    b = (struct FakeBackend *)opaque;
    b->event_state[(unsigned int)native_event] = 0;
    ++b->event_close_count;
    return O2_NO_ERROR;
}
static void fake_exception_changed(void *opaque, struct O2ExceptionRegistrationRecord *head)
{
    struct FakeBackend *b;
    b = (struct FakeBackend *)opaque;
    b->exception_head = head;
}

static const struct Os2DosBackendOps fake_ops = {
    fake_lock, fake_unlock, fake_dispatch, fake_standard,
    fake_close_native, fake_close_find, fake_sleep, fake_validate,
    fake_event_create, fake_event_reset, fake_event_post, fake_event_wait,
    fake_event_close, fake_exception_changed
};

static int test_handles(struct Os2DosSession *s)
{
    O2NATIVE n;
    O2NATIVE old;
    O2HFILE h;
    O2ULONG fh;
    O2ULONG pid;
    O2LONG delta;
    O2ULONG maxfh;

    CHECK(s->max_file_handles == 20UL);
    n = 0;
    CHECK(os2_dos_resolve_hfile(s, 1UL, &n) == O2_NO_ERROR);
    CHECK(n == (O2NATIVE)11UL);

    h = os2_dos_alloc_hfile(s, (O2NATIVE)50UL);
    CHECK(h == 3UL);
    CHECK(os2_dos_resolve_hfile(s, h, &n) == O2_NO_ERROR && n == 50UL);
    os2_dos_free_hfile(s, h);
    CHECK(os2_dos_resolve_hfile(s, h, &n) == O2_ERROR_INVALID_HANDLE);

    old = 0;
    CHECK(os2_dos_set_owned_std_handle(s, 1UL, 77UL, &old) == O2_NO_ERROR);
    CHECK(old == O2_DOS_NATIVE_INVALID);
    CHECK(os2_dos_owned_std_handle(s, 1UL) == 77UL);
    CHECK(os2_dos_resolve_hfile(s, 1UL, &n) == O2_NO_ERROR && n == 77UL);

    CHECK(os2_dos_replace_hfile(s, 25UL, 125UL, &old) == O2_NO_ERROR);
    delta = -200;
    maxfh = 0;
    CHECK(os2_dos_DosSetRelMaxFH(s, &delta, &maxfh) == O2_NO_ERROR);
    CHECK(maxfh == 26UL);
    delta = 10;
    CHECK(os2_dos_DosSetRelMaxFH(s, &delta, &maxfh) == O2_NO_ERROR);
    CHECK(maxfh == 36UL);

    fh = os2_dos_alloc_find_handle(s, 88UL);
    CHECK(fh == 1UL);
    CHECK(os2_dos_get_find_handle(s, fh, &n) == O2_NO_ERROR && n == 88UL);
    os2_dos_free_find_handle(s, fh);
    CHECK(os2_dos_get_find_handle(s, fh, &n) == O2_ERROR_INVALID_HANDLE);

    CHECK(os2_dos_store_child(s, 99UL, 1234UL));
    pid = 0;
    CHECK(os2_dos_take_child(s, 1234UL, &pid) == 99UL);
    CHECK(pid == 1234UL);
    os2_dos_put_child(s, 99UL, 1234UL);
    CHECK(os2_dos_take_child(s, 0UL, &pid) == 99UL && pid == 1234UL);
    return 0;
}

static int test_sleep_beep(struct Os2DosSession *s, struct FakeBackend *b)
{
    b->slept = 0;
    CHECK(os2_dos_DosSleep(s, 123UL) == O2_NO_ERROR && b->slept == 123UL);
    CHECK(os2_dos_Dos16Sleep(s, 456UL) == O2_NO_ERROR && b->slept == 456UL);
    CHECK(os2_dos_DosBeep(s, 0UL, 99UL) == O2_NO_ERROR && b->slept == 99UL);
    CHECK(os2_dos_DosBeep(s, 36UL, 1UL) == O2_ERROR_INVALID_FREQUENCY);
    CHECK(os2_dos_DosBeep(s, 32768UL, 1UL) == O2_ERROR_INVALID_FREQUENCY);
    b->dispatch_id = 0;
    CHECK(os2_dos_DosBeep(s, 440UL, 0UL) == O2_NO_ERROR && b->dispatch_id == 0U);
    CHECK(os2_dos_DosBeep(s, 440UL, 10UL) == 0x6000UL + OS2_DOS_CALL_DOSBEEP);
    CHECK(b->dispatch_id == OS2_DOS_CALL_DOSBEEP);
    return 0;
}

static int test_events(struct Os2DosSession *s, struct FakeBackend *b)
{
    O2ULONG h;
    O2ULONG h2;
    O2ULONG count;

    h = 0;
    CHECK(os2_dos_DosCreateEventSem(s, "\\SEM32\\R2TEST", &h, 0UL, 0UL) == O2_NO_ERROR);
    CHECK(h != 0UL);
    h2 = 0;
    CHECK(os2_dos_DosOpenEventSem(s, "\\SEM32\\R2TEST", &h2) == O2_NO_ERROR);
    CHECK(h2 == h);
    CHECK(os2_dos_DosPostEventSem(s, h) == O2_NO_ERROR);
    CHECK(os2_dos_DosPostEventSem(s, h) == O2_ERROR_ALREADY_POSTED);
    count = 0;
    CHECK(os2_dos_DosQueryEventSem(s, h, &count) == O2_NO_ERROR && count == 2UL);
    count = 0;
    CHECK(os2_dos_DosResetEventSem(s, h, &count) == O2_NO_ERROR && count == 2UL);
    CHECK(os2_dos_DosResetEventSem(s, h, &count) == O2_ERROR_ALREADY_RESET);
    CHECK(os2_dos_DosCloseEventSem(s, h) == O2_NO_ERROR);
    CHECK(b->event_close_count == 0U);
    CHECK(os2_dos_DosCloseEventSem(s, h2) == O2_NO_ERROR);
    CHECK(b->event_close_count == 1U);
    CHECK(os2_dos_DosQueryEventSem(s, h, &count) == O2_ERROR_INVALID_HANDLE);
    return 0;
}

static int test_suballoc(struct Os2DosSession *s)
{
    unsigned char arena[256];
    void *a;
    void *b;
    unsigned int i;

    memset(arena, 0xa5, sizeof(arena));
    CHECK(os2_dos_DosSubSetMem(s, arena, 1UL, (O2ULONG)sizeof(arena)) == O2_NO_ERROR);
    for (i = 0U; i < O2_DOS_SUBPOOL_HEADER; ++i)
        CHECK(arena[i] == 0);
    a = NULL;
    b = NULL;
    CHECK(os2_dos_DosSubAllocMem(s, arena, &a, 1UL) == O2_NO_ERROR);
    CHECK(a == (void *)(arena + O2_DOS_SUBPOOL_HEADER));
    CHECK(os2_dos_DosSubAllocMem(s, arena, &b, 9UL) == O2_NO_ERROR);
    CHECK(b == (void *)(arena + O2_DOS_SUBPOOL_HEADER + 8));
    CHECK(os2_dos_DosSubFreeMem(s, arena, a, 1UL) == O2_NO_ERROR);
    CHECK(os2_dos_DosSubFreeMem(s, arena, b, 9UL) == O2_NO_ERROR);
    CHECK(os2_dos_DosSubUnsetMem(s, arena) == O2_NO_ERROR);
    CHECK(os2_dos_DosSubAllocMem(s, arena, &a, 8UL) == O2_ERROR_INVALID_PARAMETER);
    return 0;
}

static int test_control_state(struct Os2DosSession *s, struct FakeBackend *b)
{
    struct O2ExceptionRegistrationRecord a;
    struct O2ExceptionRegistrationRecord c;
    O2ULONG times;
    O2ULONG prev_handler;
    unsigned short prev_action;

    CHECK(os2_dos_DosError(s, 2UL) == O2_NO_ERROR && s->error_flags == 2UL);
    CHECK(os2_dos_DosError(s, 4UL) == O2_ERROR_INVALID_PARAMETER);
    CHECK(os2_dos_DosExitList(s, 0UL, NULL) == O2_ERROR_INVALID_FUNCTION);
    CHECK(os2_dos_DosExitList(s, 1UL, NULL) == O2_ERROR_INVALID_PARAMETER);
    CHECK(os2_dos_DosExitList(s, 3UL, NULL) == O2_NO_ERROR);

    memset(&a, 0, sizeof(a));
    memset(&c, 0, sizeof(c));
    CHECK(os2_dos_DosSetExceptionHandler(s, &a) == O2_NO_ERROR);
    CHECK(b->exception_head == &a);
    CHECK(os2_dos_DosSetExceptionHandler(s, &c) == O2_NO_ERROR);
    CHECK(c.prev_structure == &a && b->exception_head == &c);
    CHECK(os2_dos_DosUnsetExceptionHandler(s, &a) == O2_NO_ERROR);
    CHECK(c.prev_structure == (struct O2ExceptionRegistrationRecord *)(uintptr_t)0xffffffffUL);
    CHECK(os2_dos_DosUnsetExceptionHandler(s, &c) == O2_NO_ERROR);
    CHECK(b->exception_head == (struct O2ExceptionRegistrationRecord *)(uintptr_t)0xffffffffUL);

    times = 0;
    CHECK(os2_dos_DosSetSignalExceptionFocus(s, 1UL, &times) == O2_NO_ERROR && times == 1UL);
    CHECK(os2_dos_DosSetSignalExceptionFocus(s, 1UL, &times) == O2_NO_ERROR && times == 2UL);
    CHECK(os2_dos_DosSetSignalExceptionFocus(s, 0UL, &times) == O2_NO_ERROR && times == 1UL);
    CHECK(os2_dos_DosSetSignalExceptionFocus(s, 2UL, &times) == O2_ERROR_INVALID_PARAMETER);

    prev_handler = 0xdeadbeefUL;
    prev_action = 0xffffU;
    CHECK(os2_dos_DosSetSigHandler(s, (void *)(uintptr_t)0x12345678UL,
                                   &prev_handler, &prev_action, 2U, 1U) == O2_NO_ERROR);
    CHECK(prev_handler == 0UL && prev_action == 0U);
    prev_handler = 0UL;
    prev_action = 0U;
    CHECK(os2_dos_DosSetSigHandler(s, (void *)(uintptr_t)0x87654321UL,
                                   &prev_handler, &prev_action, 1U, 1U) == O2_NO_ERROR);
    CHECK(prev_handler == 0x12345678UL && prev_action == 2U);
    CHECK(os2_dos_DosSetSigHandler(s, NULL, NULL, NULL, 4U, 1U) == O2_NO_ERROR);
    CHECK(os2_dos_DosSetSigHandler(s, NULL, NULL, NULL, 2U, 0U) == 209U);

    prev_handler = 0xdeadbeefUL;
    CHECK(os2_dos_DosSetVec(s, 0U, (void *)(uintptr_t)0x11223344UL,
                            &prev_handler) == O2_NO_ERROR);
    CHECK(prev_handler == 0UL);
    prev_handler = 0UL;
    CHECK(os2_dos_DosSetVec(s, 0U, (void *)(uintptr_t)0xaabbccddUL,
                            &prev_handler) == O2_NO_ERROR);
    CHECK(prev_handler == 0x11223344UL);
    prev_handler = 0UL;
    CHECK(os2_dos_DosSetVec(s, 0U, NULL, &prev_handler) == O2_NO_ERROR);
    CHECK(prev_handler == 0xaabbccddUL);
    prev_handler = 0xdeadbeefUL;
    CHECK(os2_dos_DosSetVec(s, 0U, NULL, &prev_handler) == O2_NO_ERROR);
    CHECK(prev_handler == 0UL);
    return 0;
}


static int test_nls(struct Os2DosSession *s, struct FakeBackend *b)
{
    O2ULONG cp[4];
    O2ULONG actual;
    O2ULONG countrycode[2];
    unsigned char info[OS2_NLS_COUNTRYINFO_SIZE];
    unsigned char dbcs[8];
    unsigned char text[2];

    CHECK(s->nls.country == 1UL);
    CHECK(s->nls.current_codepage == 437UL);
    b->dispatch_id = 0U;
    CHECK(os2_dos_DosSetProcessCp(s, 850UL) == O2_NO_ERROR);
    CHECK(s->nls.current_codepage == 850UL);
    CHECK(b->dispatch_id == 0U);

    memset(cp, 0, sizeof(cp));
    actual = 0UL;
    CHECK(os2_dos_DosQueryCp(s, 8UL, cp, &actual) == O2_NO_ERROR);
    CHECK(actual == 8UL && cp[0] == 850UL && cp[1] == 437UL);
    CHECK(b->dispatch_id == 0U);

    countrycode[0] = 44UL;
    countrycode[1] = 437UL;
    memset(info, 0, sizeof(info));
    actual = 0UL;
    CHECK(os2_dos_DosQueryCtryInfo(s, sizeof(info), countrycode,
                                   info, &actual) == O2_NO_ERROR);
    CHECK(actual == OS2_NLS_COUNTRYINFO_SIZE && info[0] == 44U);

    memset(dbcs, 0xcc, sizeof(dbcs));
    CHECK(os2_dos_DosQueryDBCSEnv(s, sizeof(dbcs), countrycode,
                                  dbcs) == O2_NO_ERROR);
    CHECK(dbcs[0] == 0U && dbcs[7] == 0U);

    text[0] = 'a';
    text[1] = 0x82U;
    CHECK(os2_dos_DosMapCase(s, 2UL, countrycode, text) == O2_NO_ERROR);
    CHECK(text[0] == 'A' && text[1] == 0x90U);
    CHECK(b->dispatch_id == 0U);
    return 0;
}

static int test_dispatch(struct Os2DosSession *s, struct FakeBackend *b)
{
    O2APIRET rc;
    b->dispatch_id = 0U;
    rc = os2_dos_DosDeleteDir(s, "dummy");
    CHECK(rc == 0x6000UL + OS2_DOS_CALL_DOSDELETEDIR);
    CHECK(b->dispatch_id == OS2_DOS_CALL_DOSDELETEDIR);
    return 0;
}

int main(void)
{
    struct Os2DosSession s;
    struct FakeBackend b;

    memset(&b, 0, sizeof(b));
    b.next_event = 100UL;
    os2_dos_session_init(&s, &b, &fake_ops);
    CHECK(s.initialized != 0);
    CHECK(test_handles(&s) == 0);
    CHECK(test_sleep_beep(&s, &b) == 0);
    CHECK(test_nls(&s, &b) == 0);
    CHECK(test_events(&s, &b) == 0);
    CHECK(test_suballoc(&s) == 0);
    CHECK(test_control_state(&s, &b) == 0);
    CHECK(test_dispatch(&s, &b) == 0);

    /* Leave one of each common-owned resource live to verify teardown. */
    CHECK(os2_dos_replace_hfile(&s, 4UL, 104UL, NULL) == O2_NO_ERROR);
    CHECK(os2_dos_alloc_find_handle(&s, 105UL) != 0xffffffffUL);
    CHECK(os2_dos_store_child(&s, 106UL, 42UL));
    {
        O2ULONG hev;
        hev = 0;
        CHECK(os2_dos_DosCreateEventSem(&s, NULL, &hev, 0UL, 0UL) == O2_NO_ERROR);
    }
    os2_dos_session_destroy(&s);
    CHECK(b.close_native_count >= 3U); /* owned stdout + HFILE + child */
    CHECK(b.close_find_count >= 1U);
    CHECK(b.event_close_count >= 2U);

    puts("doscalls-core-check: PASS");
    return 0;
}
