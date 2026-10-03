#include <stdio.h>
#include <string.h>

#include "os2_doscalls.h"
#include "os2_doscalls_backend.h"
#include "os2_doscalls_win32.h"

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
    return 1; } } while (0)

extern O2APIRET __cdecl DosSleep(O2ULONG milliseconds);
extern O2APIRET __cdecl DosGetResource(O2ULONG,O2ULONG,O2ULONG,void **);
extern O2APIRET __cdecl DosFreeResource(void *);
extern O2APIRET __cdecl Dos16Sleep(O2ULONG milliseconds);
extern O2APIRET __cdecl DosDeleteDir(const char *path);
extern O2APIRET __cdecl O2NlsMapCase(O2ULONG cb, const void *countrycode, void *buffer);
extern O2APIRET __cdecl DosSetRelMaxFH(O2LONG *delta, O2ULONG *current);
extern unsigned short __cdecl DosSetSigHandler(void *routine, void *prev_address,
                                                  unsigned short *prev_action,
                                                  unsigned short action,
                                                  unsigned short sig_number);
extern unsigned short __cdecl DosSetVec(unsigned short vector, void *routine,
                                        O2ULONG *prev_address);

struct Fake {
    O2ULONG slept;
    unsigned int dispatch_id;
    struct Os2DosArgs_DosGetResource resource;
    void *freed_resource;
};
static struct Os2DosSession session;
static struct Fake fake;

static O2APIRET dispatch(void *opaque, unsigned int id, void *args)
{
    struct Fake *f;
    (void)args;
    f = (struct Fake *)opaque;
    f->dispatch_id = id;
    if(id==OS2_DOS_CALL_DOSGETRESOURCE)
        f->resource=*(struct Os2DosArgs_DosGetResource *)args;
    if(id==OS2_DOS_CALL_DOSFREERESOURCE)
        f->freed_resource=((struct Os2DosArgs_DosFreeResource *)args)->buffer;
    return (O2APIRET)(0x7000UL + id);
}
static O2APIRET sleep_ms(void *opaque, O2ULONG ms)
{
    struct Fake *f;
    f = (struct Fake *)opaque;
    f->slept = ms;
    return O2_NO_ERROR;
}
static const struct Os2DosBackendOps ops = {
    NULL, NULL, dispatch, NULL, NULL, NULL, sleep_ms, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL
};

/* Host-test substitute for the real Win32 backend's session accessor. */
struct Os2DosSession *os2_doscalls_win32_session(void)
{
    return &session;
}
void os2_doscalls_win32_session_init(struct Os2DosSession *unused)
{
    (void)unused;
}

int main(void)
{
    O2LONG delta;
    O2ULONG current;
    O2ULONG prev_handler;
    unsigned short prev_action;
    char byte;
    void *resource;

    memset(&fake, 0, sizeof(fake));
    os2_dos_session_init(&session, &fake, &ops);

    CHECK(DosSleep(7UL) == O2_NO_ERROR && fake.slept == 7UL);
    CHECK(Dos16Sleep(8UL) == O2_NO_ERROR && fake.slept == 8UL);
    resource=NULL;
    CHECK(DosGetResource(1,10,42,&resource)==0x7000UL+OS2_DOS_CALL_DOSGETRESOURCE);
    CHECK(fake.resource.module==1 && fake.resource.type==10 && fake.resource.id==42);
    CHECK(fake.resource.buffer==&resource);
    CHECK(DosFreeResource(&byte)==0x7000UL+OS2_DOS_CALL_DOSFREERESOURCE);
    CHECK(fake.freed_resource==&byte);

    fake.dispatch_id = 0U;
    CHECK(DosDeleteDir("dummy") == 0x7000UL + OS2_DOS_CALL_DOSDELETEDIR);
    CHECK(fake.dispatch_id == OS2_DOS_CALL_DOSDELETEDIR);

    byte = 'a';
    fake.dispatch_id = 0U;
    CHECK(O2NlsMapCase(1UL, NULL, &byte) == O2_NO_ERROR);
    CHECK(byte == 'A');
    CHECK(fake.dispatch_id == 0U);

    delta = 5;
    current = 0;
    CHECK(DosSetRelMaxFH(&delta, &current) == O2_NO_ERROR);
    CHECK(current == 25UL);

    prev_handler = 0UL;
    prev_action = 0U;
    CHECK(DosSetSigHandler((void *)(uintptr_t)0x10203040UL, &prev_handler,
                           &prev_action, 2U, 1U) == O2_NO_ERROR);
    CHECK(prev_handler == 0UL && prev_action == 0U);
    prev_handler = 0UL;
    CHECK(DosSetVec(6U, (void *)(uintptr_t)0x55667788UL, &prev_handler) == O2_NO_ERROR);
    CHECK(prev_handler == 0UL);
    CHECK(DosSetVec(6U, NULL, &prev_handler) == O2_NO_ERROR &&
          prev_handler == 0x55667788UL);

    puts("doscalls-veneer-check: PASS");
    return 0;
}
