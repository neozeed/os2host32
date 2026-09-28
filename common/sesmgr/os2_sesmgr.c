#include <stddef.h>
#include <string.h>

#include "os2_sesmgr.h"
#include "os2_sesmgr_backend.h"

static int sesmgr_ready(const struct Os2SesmgrSession *session)
{
    const struct Os2SesmgrBackendOps *b;
    if (session == NULL || session->backend == NULL)
        return 0;
    b = session->backend;
    return b->local_lock != NULL && b->local_unlock != NULL &&
           b->registry_lock != NULL && b->registry_unlock != NULL &&
           b->current_identity != NULL && b->identity_from_process != NULL &&
           b->identity_alive != NULL && b->owned_process_alive != NULL &&
           b->launch != NULL && b->resume != NULL &&
           b->close_thread != NULL && b->terminate != NULL &&
           b->wait_process != NULL && b->close_process != NULL &&
           b->set_current_title != NULL;
}

static void copy_text(char *dst, unsigned int cap, const char *src)
{
    size_t n;
    if (cap == 0U)
        return;
    if (src == NULL)
        src = "";
    n = strlen(src);
    if (n >= (size_t)cap)
        n = (size_t)cap - 1U;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

void os2_sesmgr_set_object_buffer(struct Os2SesmgrStartRequest *request,
                                  const char *text)
{
    size_t n;
    if (request == NULL || request->object_buffer == NULL ||
        request->object_buffer_length == 0UL)
        return;
    if (text == NULL)
        text = "";
    n = strlen(text);
    if (n >= (size_t)request->object_buffer_length)
        n = (size_t)request->object_buffer_length - 1U;
    memcpy(request->object_buffer, text, n);
    request->object_buffer[n] = '\0';
}

static int registry_begin(struct Os2SesmgrSession *session,
                          struct Os2SesmgrRegistry **registry)
{
    struct Os2SesmgrRegistry *r;
    if (!sesmgr_ready(session) || registry == NULL)
        return 0;
    r = NULL;
    if (!session->backend->registry_lock(session->backend_opaque, &r) ||
        r == NULL)
        return 0;
    if (r->magic != OS2_SESMGR_REGISTRY_MAGIC ||
        r->version != OS2_SESMGR_REGISTRY_VERSION) {
        memset(r, 0, sizeof(*r));
        r->magic = OS2_SESMGR_REGISTRY_MAGIC;
        r->version = OS2_SESMGR_REGISTRY_VERSION;
    }
    *registry = r;
    return 1;
}

static void registry_end(struct Os2SesmgrSession *session)
{
    session->backend->registry_unlock(session->backend_opaque);
}

static void registry_reap_locked(struct Os2SesmgrSession *session,
                                 struct Os2SesmgrRegistry *registry)
{
    unsigned int i;
    for (i = 0U; i < OS2_SESMGR_SHARED_MAX; ++i) {
        struct Os2SesmgrSharedRecord *e;
        e = &registry->sessions[i];
        if (!e->in_use)
            continue;
        if (!session->backend->identity_alive(session->backend_opaque,
                                               &e->process) ||
            !session->backend->identity_alive(session->backend_opaque,
                                               &e->owner))
            memset(e, 0, sizeof(*e));
    }
}

static Os2SesmgrU32 registry_allocate_id(struct Os2SesmgrSession *session)
{
    struct Os2SesmgrRegistry *registry;
    Os2SesmgrU32 sid;
    if (!registry_begin(session, &registry))
        return 0UL;
    registry_reap_locked(session, registry);
    sid = ++registry->next_session_id;
    if (sid == 0UL)
        sid = ++registry->next_session_id;
    registry_end(session);
    return sid;
}

static int registry_add(struct Os2SesmgrSession *session,
                        Os2SesmgrU32 sid,
                        Os2SesmgrU32 pid,
                        Os2SesmgrNative process,
                        const char *program,
                        const char *title)
{
    struct Os2SesmgrRegistry *registry;
    struct Os2SesmgrIdentity child;
    struct Os2SesmgrIdentity owner;
    unsigned int i;

    memset(&child, 0, sizeof(child));
    memset(&owner, 0, sizeof(owner));
    if (!session->backend->identity_from_process(session->backend_opaque,
                                                  process, pid, &child) ||
        !session->backend->current_identity(session->backend_opaque, &owner))
        return 0;
    if (!registry_begin(session, &registry))
        return 0;
    registry_reap_locked(session, registry);
    for (i = 0U; i < OS2_SESMGR_SHARED_MAX; ++i) {
        struct Os2SesmgrSharedRecord *e;
        e = &registry->sessions[i];
        if (e->in_use)
            continue;
        memset(e, 0, sizeof(*e));
        e->in_use = 1UL;
        e->session_id = sid;
        e->process = child;
        e->owner = owner;
        copy_text(e->program, OS2_SESMGR_TEXT_MAX, program);
        copy_text(e->title, OS2_SESMGR_TEXT_MAX,
                  title != NULL && *title != '\0' ? title : program);
        registry_end(session);
        return 1;
    }
    registry_end(session);
    return 0;
}

static void registry_remove(struct Os2SesmgrSession *session,
                            Os2SesmgrU32 sid)
{
    struct Os2SesmgrRegistry *registry;
    unsigned int i;
    if (!registry_begin(session, &registry))
        return;
    for (i = 0U; i < OS2_SESMGR_SHARED_MAX; ++i) {
        if (registry->sessions[i].in_use &&
            registry->sessions[i].session_id == sid) {
            memset(&registry->sessions[i], 0,
                   sizeof(registry->sessions[i]));
            break;
        }
    }
    registry_end(session);
}

static void reap_owned_locked(struct Os2SesmgrSession *session)
{
    unsigned int i;
    for (i = 0U; i < OS2_SESMGR_LOCAL_MAX; ++i) {
        struct Os2SesmgrOwnedSession *owned;
        owned = &session->owned[i];
        if (owned->process == OS2_SESMGR_NATIVE_INVALID)
            continue;
        if (!session->backend->owned_process_alive(session->backend_opaque,
                                                    owned->process)) {
            registry_remove(session, owned->session_id);
            session->backend->close_process(session->backend_opaque,
                                             owned->process, owned->group);
            memset(owned, 0, sizeof(*owned));
        }
    }
}

static int remember_owned(struct Os2SesmgrSession *session,
                          Os2SesmgrU32 sid,
                          const struct Os2SesmgrLaunch *launch,
                          const char *program,
                          const char *title)
{
    unsigned int i;
    int ok;
    session->backend->local_lock(session->backend_opaque);
    reap_owned_locked(session);
    ok = 0;
    for (i = 0U; i < OS2_SESMGR_LOCAL_MAX; ++i) {
        if (session->owned[i].process != OS2_SESMGR_NATIVE_INVALID)
            continue;
        session->owned[i].session_id = sid;
        session->owned[i].pid = launch->pid;
        session->owned[i].process = launch->process;
        session->owned[i].group = launch->group;
        if (!registry_add(session, sid, launch->pid, launch->process, program, title)) {
            memset(&session->owned[i], 0, sizeof(session->owned[i]));
            break;
        }
        ok = 1;
        break;
    }
    session->backend->local_unlock(session->backend_opaque);
    return ok;
}

void os2_sesmgr_session_init(struct Os2SesmgrSession *session,
                             void *backend_opaque,
                             const struct Os2SesmgrBackendOps *backend)
{
    if (session == NULL)
        return;
    memset(session, 0, sizeof(*session));
    session->backend_opaque = backend_opaque;
    session->backend = backend;
}

void os2_sesmgr_session_destroy(struct Os2SesmgrSession *session)
{
    unsigned int i;
    if (!sesmgr_ready(session))
        return;
    session->backend->local_lock(session->backend_opaque);
    for (i = 0U; i < OS2_SESMGR_LOCAL_MAX; ++i) {
        if (session->owned[i].process != OS2_SESMGR_NATIVE_INVALID) {
            session->backend->close_process(session->backend_opaque,
                                             session->owned[i].process,
                                             session->owned[i].group);
            memset(&session->owned[i], 0, sizeof(session->owned[i]));
        }
    }
    session->backend->local_unlock(session->backend_opaque);
}

static Os2SesmgrApiRet validate_start(struct Os2SesmgrStartRequest *r)
{
    if (r == NULL || r->source_length < 24U || r->program == NULL ||
        r->program[0] == '\0')
        return OS2_SESMGR_ERROR_INVALID_PARAMETER;
    os2_sesmgr_set_object_buffer(r, "");
    if (r->related != OS2_SESMGR_RELATED_INDEPENDENT &&
        r->related != OS2_SESMGR_RELATED_CHILD)
        return OS2_SESMGR_ERROR_INVALID_PARAMETER;
    if (r->fgbg != OS2_SESMGR_FGBG_FORE &&
        r->fgbg != OS2_SESMGR_FGBG_BACK)
        return OS2_SESMGR_ERROR_INVALID_PARAMETER;
    if (r->trace_opt != OS2_SESMGR_TRACEOPT_NONE)
        return OS2_SESMGR_ERROR_INVALID_CALL;
    if (r->inherit_opt != OS2_SESMGR_INHERIT_SHELL &&
        r->inherit_opt != OS2_SESMGR_INHERIT_PARENT)
        return OS2_SESMGR_ERROR_INVALID_PARAMETER;
    if (r->session_type != OS2_SESMGR_TYPE_DEFAULT &&
        r->session_type != OS2_SESMGR_TYPE_FULLSCREEN &&
        r->session_type != OS2_SESMGR_TYPE_WINDOWABLEVIO &&
        r->session_type != OS2_SESMGR_TYPE_PM)
        return OS2_SESMGR_ERROR_INVALID_CALL;
    if ((r->program_control & ~OS2_SESMGR_CONTROL_KNOWN_MASK) != 0U ||
        ((r->program_control & OS2_SESMGR_CONTROL_MAXIMIZE) != 0U &&
         (r->program_control & OS2_SESMGR_CONTROL_MINIMIZE) != 0U))
        return OS2_SESMGR_ERROR_INVALID_PARAMETER;
    if (r->reserved != 0U)
        return OS2_SESMGR_ERROR_INVALID_PARAMETER;
    if (r->term_queue != NULL && r->term_queue[0] != '\0')
        return OS2_SESMGR_ERROR_INVALID_CALL;
    if (r->program_handle != 0UL)
        return OS2_SESMGR_ERROR_INVALID_CALL;
    return OS2_SESMGR_NO_ERROR;
}

Os2SesmgrApiRet os2_sesmgr_DosStartSession(struct Os2SesmgrSession *session,
                                           struct Os2SesmgrStartRequest *request,
                                           Os2SesmgrU32 *session_id,
                                           Os2SesmgrU32 *pid_out)
{
    struct Os2SesmgrLaunch launch;
    Os2SesmgrApiRet rc;
    Os2SesmgrU32 sid;

    if (session_id != NULL)
        *session_id = 0UL;
    if (pid_out != NULL)
        *pid_out = 0UL;
    rc = validate_start(request);
    if (rc != OS2_SESMGR_NO_ERROR)
        return rc;
    if (!sesmgr_ready(session))
        return OS2_SESMGR_ERROR_INVALID_CALL;

    sid = registry_allocate_id(session);
    if (sid == 0UL)
        return OS2_SESMGR_ERROR_NOT_ENOUGH_MEMORY;

    memset(&launch, 0, sizeof(launch));
    rc = session->backend->launch(session->backend_opaque, request, &launch);
    if (rc != OS2_SESMGR_NO_ERROR)
        return rc;

    if (session_id != NULL)
        *session_id = sid;
    if (pid_out != NULL)
        *pid_out = launch.pid;

    if (request->related == OS2_SESMGR_RELATED_CHILD) {
        if (!remember_owned(session, sid, &launch, request->program,
                            request->title)) {
            (void)session->backend->terminate(session->backend_opaque,
                                              launch.process, launch.group);
            session->backend->wait_process(session->backend_opaque,
                                           launch.process);
            session->backend->close_thread(session->backend_opaque,
                                           launch.initial_thread);
            session->backend->close_process(session->backend_opaque,
                                            launch.process, launch.group);
            if (session_id != NULL)
                *session_id = 0UL;
            if (pid_out != NULL)
                *pid_out = 0UL;
            return OS2_SESMGR_ERROR_RETRY_SUB_ALLOC;
        }
        rc = session->backend->resume(session->backend_opaque,
                                      launch.initial_thread);
        session->backend->close_thread(session->backend_opaque,
                                       launch.initial_thread);
        if (rc != OS2_SESMGR_NO_ERROR) {
            (void)os2_sesmgr_DosStopSession(session, 0UL, sid);
            if (session_id != NULL)
                *session_id = 0UL;
            if (pid_out != NULL)
                *pid_out = 0UL;
            return rc;
        }
    } else {
        session->backend->close_thread(session->backend_opaque,
                                       launch.initial_thread);
        session->backend->close_process(session->backend_opaque,
                                        launch.process, launch.group);
    }
    if (session->backend->trace_start != NULL)
        session->backend->trace_start(session->backend_opaque, sid, launch.pid);
    return OS2_SESMGR_NO_ERROR;
}

Os2SesmgrApiRet os2_sesmgr_DosStopSession(struct Os2SesmgrSession *session,
                                          Os2SesmgrU32 scope,
                                          Os2SesmgrU32 session_id)
{
    unsigned int i;
    int found;
    Os2SesmgrApiRet rc;

    if (scope > 1UL)
        return OS2_SESMGR_ERROR_INVALID_STOP_OPTION;
    if (scope == 0UL && session_id == 0UL)
        return OS2_SESMGR_ERROR_INVALID_SESSION_ID;
    if (!sesmgr_ready(session))
        return OS2_SESMGR_ERROR_INVALID_CALL;

    found = 0;
    session->backend->local_lock(session->backend_opaque);
    reap_owned_locked(session);
    for (i = 0U; i < OS2_SESMGR_LOCAL_MAX; ++i) {
        struct Os2SesmgrOwnedSession *owned;
        owned = &session->owned[i];
        if (owned->process == OS2_SESMGR_NATIVE_INVALID)
            continue;
        if (scope == 0UL && owned->session_id != session_id)
            continue;
        found = 1;
        if (session->backend->trace_stop != NULL)
            session->backend->trace_stop(session->backend_opaque,
                                         owned->session_id, owned->pid);
        rc = session->backend->terminate(session->backend_opaque,
                                         owned->process, owned->group);
        if (rc != OS2_SESMGR_NO_ERROR) {
            session->backend->local_unlock(session->backend_opaque);
            return rc;
        }
        session->backend->wait_process(session->backend_opaque,
                                       owned->process);
        registry_remove(session, owned->session_id);
        session->backend->close_process(session->backend_opaque,
                                        owned->process, owned->group);
        memset(owned, 0, sizeof(*owned));
        if (scope == 0UL)
            break;
    }
    session->backend->local_unlock(session->backend_opaque);
    if (!found)
        return OS2_SESMGR_ERROR_INVALID_SESSION_ID;
    return OS2_SESMGR_NO_ERROR;
}

Os2SesmgrApiRet os2_sesmgr_DosSmSetTitle(struct Os2SesmgrSession *session,
                                         Os2SesmgrU32 session_id,
                                         const char *title)
{
    struct Os2SesmgrRegistry *registry;
    struct Os2SesmgrIdentity current;
    unsigned int i;
    int is_current;

    if (title == NULL)
        return OS2_SESMGR_ERROR_INVALID_PARAMETER;
    if (!sesmgr_ready(session))
        return OS2_SESMGR_ERROR_INVALID_CALL;
    memset(&current, 0, sizeof(current));
    if (!session->backend->current_identity(session->backend_opaque, &current))
        return OS2_SESMGR_ERROR_INVALID_PARAMETER;
    if (!registry_begin(session, &registry))
        return OS2_SESMGR_ERROR_NOT_ENOUGH_MEMORY;
    registry_reap_locked(session, registry);
    for (i = 0U; i < OS2_SESMGR_SHARED_MAX; ++i) {
        struct Os2SesmgrSharedRecord *e;
        e = &registry->sessions[i];
        if (!e->in_use)
            continue;
        if ((session_id != 0UL && e->session_id != session_id) ||
            (session_id == 0UL && e->process.pid != current.pid))
            continue;
        if (e->process.pid != current.pid && e->owner.pid != current.pid) {
            registry_end(session);
            return OS2_SESMGR_ERROR_PROCESS_NOT_PARENT;
        }
        is_current = e->process.pid == current.pid;
        copy_text(e->title, OS2_SESMGR_TEXT_MAX, title);
        registry_end(session);
        if (is_current)
            session->backend->set_current_title(session->backend_opaque,
                                                title);
        return OS2_SESMGR_NO_ERROR;
    }
    registry_end(session);
    return OS2_SESMGR_ERROR_INVALID_SESSION_ID;
}

Os2SesmgrApiRet os2_sesmgr_QuerySessions(struct Os2SesmgrSession *session,
                                         struct Os2SesmgrSessionInfo *entries,
                                         Os2SesmgrU32 capacity,
                                         Os2SesmgrU32 *count)
{
    struct Os2SesmgrRegistry *registry;
    struct Os2SesmgrIdentity owner;
    Os2SesmgrU32 n;
    unsigned int i;

    if (count == NULL || (capacity != 0UL && entries == NULL))
        return OS2_SESMGR_ERROR_INVALID_PARAMETER;
    *count = 0UL;
    if (!sesmgr_ready(session))
        return OS2_SESMGR_ERROR_INVALID_CALL;
    memset(&owner, 0, sizeof(owner));
    if (!session->backend->current_identity(session->backend_opaque, &owner))
        return OS2_SESMGR_ERROR_INVALID_PARAMETER;
    if (!registry_begin(session, &registry))
        return OS2_SESMGR_ERROR_NOT_ENOUGH_MEMORY;
    registry_reap_locked(session, registry);
    n = 0UL;
    for (i = 0U; i < OS2_SESMGR_SHARED_MAX; ++i) {
        struct Os2SesmgrSharedRecord *e;
        e = &registry->sessions[i];
        if (!e->in_use || e->owner.pid != owner.pid ||
            e->owner.token_low != owner.token_low ||
            e->owner.token_high != owner.token_high)
            continue;
        if (n < capacity) {
            entries[n].task_id = e->session_id;
            entries[n].pid = e->process.pid;
            copy_text(entries[n].program, OS2_SESMGR_TEXT_MAX, e->program);
            copy_text(entries[n].title, OS2_SESMGR_TEXT_MAX, e->title);
        }
        ++n;
    }
    registry_end(session);
    *count = n < capacity ? n : capacity;
    return OS2_SESMGR_NO_ERROR;
}
