#include <stdio.h>
#include <string.h>

#include "os2_sesmgr.h"
#include "os2_sesmgr_backend.h"

struct FakeCtx {
    struct Os2SesmgrRegistry registry;
    struct Os2SesmgrIdentity current;
    unsigned char alive[1024];
    Os2SesmgrU32 next_pid;
    int launch_count;
    int resume_count;
    int terminate_count;
    int close_thread_count;
    int close_process_count;
    int set_title_count;
    char host_title[OS2_SESMGR_TEXT_MAX];
};

static void f_lock(void *o) { (void)o; }
static void f_unlock(void *o) { (void)o; }
static int f_registry_lock(void *o, struct Os2SesmgrRegistry **r)
{
    struct FakeCtx *c = (struct FakeCtx *)o;
    *r = &c->registry;
    return 1;
}
static void f_registry_unlock(void *o) { (void)o; }
static int f_current(void *o, struct Os2SesmgrIdentity *id)
{
    *id = ((struct FakeCtx *)o)->current;
    return 1;
}
static int f_from_process(void *o, Os2SesmgrNative p, Os2SesmgrU32 pid,
                          struct Os2SesmgrIdentity *id)
{
    struct FakeCtx *c = (struct FakeCtx *)o;
    (void)p;
    (void)c;
    id->pid = pid;
    id->token_low = pid + 10UL;
    id->token_high = pid + 20UL;
    return pid != 0UL;
}
static int f_identity_alive(void *o, const struct Os2SesmgrIdentity *id)
{
    struct FakeCtx *c = (struct FakeCtx *)o;
    if (id->pid >= 1024UL)
        return 1;
    return c->alive[id->pid] != 0U;
}
static int f_owned_alive(void *o, Os2SesmgrNative p)
{
    struct FakeCtx *c = (struct FakeCtx *)o;
    Os2SesmgrU32 pid = (Os2SesmgrU32)p;
    return pid < 1024UL && c->alive[pid] != 0U;
}
static Os2SesmgrApiRet f_launch(void *o, struct Os2SesmgrStartRequest *r,
                                struct Os2SesmgrLaunch *l)
{
    struct FakeCtx *c = (struct FakeCtx *)o;
    Os2SesmgrU32 pid;
    (void)r;
    pid = c->next_pid++;
    c->alive[pid] = 1U;
    ++c->launch_count;
    l->pid = pid;
    l->process = (Os2SesmgrNative)pid;
    l->initial_thread = (Os2SesmgrNative)(pid + 1000UL);
    l->group = r->related == OS2_SESMGR_RELATED_CHILD ?
               (Os2SesmgrNative)(pid + 2000UL) : OS2_SESMGR_NATIVE_INVALID;
    return OS2_SESMGR_NO_ERROR;
}
static Os2SesmgrApiRet f_resume(void *o, Os2SesmgrNative t)
{
    struct FakeCtx *c = (struct FakeCtx *)o;
    (void)t;
    ++c->resume_count;
    return OS2_SESMGR_NO_ERROR;
}
static void f_close_thread(void *o, Os2SesmgrNative t)
{
    struct FakeCtx *c = (struct FakeCtx *)o;
    (void)t;
    ++c->close_thread_count;
}
static Os2SesmgrApiRet f_terminate(void *o, Os2SesmgrNative p,
                                   Os2SesmgrNative g)
{
    struct FakeCtx *c = (struct FakeCtx *)o;
    Os2SesmgrU32 pid = (Os2SesmgrU32)p;
    (void)g;
    if (pid < 1024UL)
        c->alive[pid] = 0U;
    ++c->terminate_count;
    return OS2_SESMGR_NO_ERROR;
}
static void f_wait(void *o, Os2SesmgrNative p) { (void)o; (void)p; }
static void f_close_process(void *o, Os2SesmgrNative p, Os2SesmgrNative g)
{
    struct FakeCtx *c = (struct FakeCtx *)o;
    (void)p; (void)g;
    ++c->close_process_count;
}
static void f_set_title(void *o, const char *s)
{
    struct FakeCtx *c = (struct FakeCtx *)o;
    size_t n = strlen(s);
    if (n >= sizeof(c->host_title)) n = sizeof(c->host_title) - 1U;
    memcpy(c->host_title, s, n);
    c->host_title[n] = '\0';
    ++c->set_title_count;
}
static void f_trace(void *o, Os2SesmgrU32 sid, Os2SesmgrU32 pid)
{ (void)o; (void)sid; (void)pid; }

static const struct Os2SesmgrBackendOps ops = {
    f_lock, f_unlock, f_registry_lock, f_registry_unlock,
    f_current, f_from_process, f_identity_alive, f_owned_alive,
    f_launch, f_resume, f_close_thread, f_terminate, f_wait,
    f_close_process, f_set_title, f_trace, f_trace
};

static int fail(const char *what)
{
    fprintf(stderr, "SESMGR core check failed: %s\n", what);
    return 1;
}

static void init_request(struct Os2SesmgrStartRequest *r)
{
    memset(r, 0, sizeof(*r));
    r->source_length = 60U;
    r->related = OS2_SESMGR_RELATED_CHILD;
    r->fgbg = OS2_SESMGR_FGBG_FORE;
    r->trace_opt = OS2_SESMGR_TRACEOPT_NONE;
    r->program = "child.exe";
    r->title = "Child";
    r->inherit_opt = OS2_SESMGR_INHERIT_SHELL;
    r->session_type = OS2_SESMGR_TYPE_DEFAULT;
    r->program_control = OS2_SESMGR_CONTROL_VISIBLE;
}

int main(void)
{
    struct FakeCtx ctx;
    struct Os2SesmgrSession session;
    struct Os2SesmgrStartRequest r;
    struct Os2SesmgrSessionInfo info[4];
    Os2SesmgrU32 sid;
    Os2SesmgrU32 pid;
    Os2SesmgrU32 count;
    Os2SesmgrApiRet rc;

    memset(&ctx, 0, sizeof(ctx));
    ctx.current.pid = 100UL;
    ctx.current.token_low = 110UL;
    ctx.current.token_high = 120UL;
    ctx.alive[100] = 1U;
    ctx.next_pid = 200UL;
    os2_sesmgr_session_init(&session, &ctx, &ops);

    init_request(&r);
    rc = os2_sesmgr_DosStartSession(&session, &r, &sid, &pid);
    if (rc != 0UL || sid != 1UL || pid != 200UL)
        return fail("related start result");
    if (ctx.launch_count != 1 || ctx.resume_count != 1 ||
        ctx.close_thread_count != 1)
        return fail("related launch mechanics");

    memset(info, 0, sizeof(info));
    count = 0UL;
    rc = os2_sesmgr_QuerySessions(&session, info, 4UL, &count);
    if (rc != 0UL || count != 1UL || info[0].task_id != sid ||
        info[0].pid != pid || strcmp(info[0].program, "child.exe") != 0 ||
        strcmp(info[0].title, "Child") != 0)
        return fail("owned query");

    rc = os2_sesmgr_DosSmSetTitle(&session, sid, "Renamed");
    if (rc != 0UL || ctx.set_title_count != 0)
        return fail("parent title update");
    count = 0UL;
    rc = os2_sesmgr_QuerySessions(&session, info, 4UL, &count);
    if (rc != 0UL || count != 1UL || strcmp(info[0].title, "Renamed") != 0)
        return fail("title persisted");

    ctx.current.pid = pid;
    ctx.current.token_low = pid + 10UL;
    ctx.current.token_high = pid + 20UL;
    rc = os2_sesmgr_DosSmSetTitle(&session, 0UL, "SelfTitle");
    if (rc != 0UL || ctx.set_title_count != 1 ||
        strcmp(ctx.host_title, "SelfTitle") != 0)
        return fail("current-session title");

    ctx.current.pid = 100UL;
    ctx.current.token_low = 110UL;
    ctx.current.token_high = 120UL;
    rc = os2_sesmgr_DosStopSession(&session, 0UL, sid);
    if (rc != 0UL || ctx.terminate_count != 1)
        return fail("stop related");
    count = 99UL;
    rc = os2_sesmgr_QuerySessions(&session, info, 4UL, &count);
    if (rc != 0UL || count != 0UL)
        return fail("registry remove on stop");

    init_request(&r);
    r.related = OS2_SESMGR_RELATED_INDEPENDENT;
    rc = os2_sesmgr_DosStartSession(&session, &r, &sid, &pid);
    if (rc != 0UL || sid != 2UL || pid != 201UL ||
        ctx.resume_count != 1 || ctx.close_process_count < 2)
        return fail("independent start");
    count = 0UL;
    rc = os2_sesmgr_QuerySessions(&session, info, 4UL, &count);
    if (rc != 0UL || count != 0UL)
        return fail("independent not owned registry");

    init_request(&r);
    r.trace_opt = 1U;
    if (os2_sesmgr_DosStartSession(&session, &r, &sid, &pid) !=
        OS2_SESMGR_ERROR_INVALID_CALL)
        return fail("TraceOpt validation");
    init_request(&r);
    r.term_queue = "\\QUEUES\\TERM";
    if (os2_sesmgr_DosStartSession(&session, &r, &sid, &pid) !=
        OS2_SESMGR_ERROR_INVALID_CALL)
        return fail("TermQ validation");
    init_request(&r);
    r.program_control = OS2_SESMGR_CONTROL_MAXIMIZE |
                        OS2_SESMGR_CONTROL_MINIMIZE;
    if (os2_sesmgr_DosStartSession(&session, &r, &sid, &pid) !=
        OS2_SESMGR_ERROR_INVALID_PARAMETER)
        return fail("PgmControl validation");
    if (os2_sesmgr_DosStopSession(&session, 2UL, 1UL) !=
        OS2_SESMGR_ERROR_INVALID_STOP_OPTION)
        return fail("stop option validation");

    os2_sesmgr_session_destroy(&session);
    puts("SESMGR common core: PASS");
    return 0;
}
