/*
 * Backend-neutral DOSCALLS process/session semantics.
 *
 * R2 moves OS/2-owned process state out of the Win32 DLL implementation.
 * APIs not yet decomposed to low-level backend services pass through the
 * typed legacy-dispatch seam; this is intentional and explicitly temporary.
 */
#include <stddef.h>
#include <string.h>
#include <stdint.h>

#include "os2_doscalls.h"
#include "os2_doscalls_backend.h"

#define O2_DOSSUB_INIT       0x00000001UL
#define O2_DOSSUB_GROW       0x00000002UL
#define O2_DOSSUB_SPARSE_OBJ 0x00000004UL
#define O2_DOSSUB_SERIALIZE  0x00000008UL

static void state_lock(struct Os2DosSession *session)
{
    if (session != NULL && session->backend != NULL &&
        session->backend->state_lock != NULL)
        session->backend->state_lock(session->backend_opaque);
}

static void state_unlock(struct Os2DosSession *session)
{
    if (session != NULL && session->backend != NULL &&
        session->backend->state_unlock != NULL)
        session->backend->state_unlock(session->backend_opaque);
}

static O2APIRET dispatch_call(struct Os2DosSession *session,
                              unsigned int call_id, void *args)
{
    if (session == NULL || session->backend == NULL ||
        session->backend->dispatch == NULL)
        return O2_ERROR_INVALID_FUNCTION;
    return session->backend->dispatch(session->backend_opaque, call_id, args);
}

void os2_dos_session_init(struct Os2DosSession *session,
                          void *backend_opaque,
                          const struct Os2DosBackendOps *backend)
{
    if (session == NULL)
        return;
    memset(session, 0, sizeof(*session));
    session->backend_opaque = backend_opaque;
    session->backend = backend;
    session->max_file_handles = 20UL;
    session->file_modes[0]=0x40;session->file_modes[1]=session->file_modes[2]=0x41;
    session->file_mode_known[0]=session->file_mode_known[1]=session->file_mode_known[2]=1;
    session->exception_head =
        (struct O2ExceptionRegistrationRecord *)(uintptr_t)0xffffffffUL;
    session->error_flags = 1UL;
    os2_nls_state_init(&session->nls);
    session->initialized = 1;
}

void os2_dos_session_destroy(struct Os2DosSession *session)
{
    unsigned int i;
    const struct Os2DosBackendOps *backend;
    void *opaque;

    if (session == NULL)
        return;
    backend = session->backend;
    opaque = session->backend_opaque;

    if (backend != NULL) {
        if (backend->event_close != NULL) {
            for (i = 0U; i < O2_DOS_EVENT_SLOTS; ++i) {
                if (session->events[i].native_event != O2_DOS_NATIVE_INVALID)
                    (void)backend->event_close(opaque,
                                               session->events[i].native_event);
            }
        }
        if (backend->close_find != NULL) {
            for (i = 1U; i < O2_DOS_MAX_FIND_HANDLES; ++i) {
                if (session->find_handles[i] != O2_DOS_NATIVE_INVALID)
                    (void)backend->close_find(opaque,
                                              session->find_handles[i]);
            }
        }
        if (backend->close_native != NULL) {
            for (i = 0U; i < 3U; ++i) {
                if (session->std_handles[i] != O2_DOS_NATIVE_INVALID)
                    (void)backend->close_native(opaque,
                                                session->std_handles[i]);
            }
            for (i = 3U; i < O2_DOS_MAX_HANDLES; ++i) {
                if (session->file_handles[i] != O2_DOS_NATIVE_INVALID)
                    (void)backend->close_native(opaque,
                                                session->file_handles[i]);
            }
            for (i = 0U; i < O2_DOS_MAX_CHILDREN; ++i) {
                if (session->children[i].process != O2_DOS_NATIVE_INVALID)
                    (void)backend->close_native(opaque,
                                                session->children[i].process);
            }
        }
    }
    memset(session, 0, sizeof(*session));
}

O2APIRET os2_dos_resolve_hfile(struct Os2DosSession *session, O2HFILE hfile,
                               O2NATIVE *native_handle)
{
    O2APIRET rc;
    O2NATIVE h;

    if (session == NULL || native_handle == NULL)
        return O2_ERROR_INVALID_PARAMETER;
    h = O2_DOS_NATIVE_INVALID;
    if (hfile <= 2UL) {
        state_lock(session);
        h = session->std_handles[hfile];
        state_unlock(session);
        if (h == O2_DOS_NATIVE_INVALID) {
            if (session->backend == NULL ||
                session->backend->standard_handle == NULL)
                return O2_ERROR_INVALID_HANDLE;
            rc = session->backend->standard_handle(session->backend_opaque,
                                                   hfile, &h);
            if (rc != O2_NO_ERROR)
                return rc;
        }
    } else {
        if (hfile >= O2_DOS_MAX_HANDLES)
            return O2_ERROR_INVALID_HANDLE;
        state_lock(session);
        h = session->file_handles[hfile];
        state_unlock(session);
    }
    if (h == O2_DOS_NATIVE_INVALID)
        return O2_ERROR_INVALID_HANDLE;
    *native_handle = h;
    return O2_NO_ERROR;
}

O2HFILE os2_dos_alloc_hfile(struct Os2DosSession *session,
                            O2NATIVE native_handle)
{
    O2HFILE i;
    O2HFILE result;

    if (session == NULL || native_handle == O2_DOS_NATIVE_INVALID)
        return 0xffffffffUL;
    result = 0xffffffffUL;
    state_lock(session);
    for (i = 3UL; i < session->max_file_handles &&
                       i < O2_DOS_MAX_HANDLES; ++i) {
        if (session->file_handles[i] == O2_DOS_NATIVE_INVALID) {
            session->file_handles[i] = native_handle;
            session->file_mode_known[i]=0;
            result = i;
            break;
        }
    }
    state_unlock(session);
    return result;
}

void os2_dos_free_hfile(struct Os2DosSession *session, O2HFILE hfile)
{
    if (session == NULL || hfile < 3UL || hfile >= O2_DOS_MAX_HANDLES)
        return;
    state_lock(session);
    session->file_handles[hfile] = O2_DOS_NATIVE_INVALID;
    session->file_mode_known[hfile]=0;
    state_unlock(session);
}

O2APIRET os2_dos_replace_hfile(struct Os2DosSession *session, O2HFILE hfile,
                               O2NATIVE native_handle, O2NATIVE *old_handle)
{
    if (session == NULL || hfile < 3UL || hfile >= O2_DOS_MAX_HANDLES ||
        native_handle == O2_DOS_NATIVE_INVALID)
        return O2_ERROR_INVALID_TARGET_HANDLE;
    state_lock(session);
    if (old_handle != NULL)
        *old_handle = session->file_handles[hfile];
    session->file_handles[hfile] = native_handle;
    state_unlock(session);
    return O2_NO_ERROR;
}

O2NATIVE os2_dos_owned_std_handle(struct Os2DosSession *session,
                                  O2HFILE hfile)
{
    O2NATIVE h;
    if (session == NULL || hfile > 2UL)
        return O2_DOS_NATIVE_INVALID;
    state_lock(session);
    h = session->std_handles[hfile];
    state_unlock(session);
    return h;
}

O2APIRET os2_dos_set_owned_std_handle(struct Os2DosSession *session,
                                      O2HFILE hfile,
                                      O2NATIVE native_handle,
                                      O2NATIVE *old_handle)
{
    if (session == NULL || hfile > 2UL ||
        native_handle == O2_DOS_NATIVE_INVALID)
        return O2_ERROR_INVALID_TARGET_HANDLE;
    state_lock(session);
    if (old_handle != NULL)
        *old_handle = session->std_handles[hfile];
    session->std_handles[hfile] = native_handle;
    state_unlock(session);
    return O2_NO_ERROR;
}

O2ULONG os2_dos_alloc_find_handle(struct Os2DosSession *session,
                                  O2NATIVE native_handle)
{
    O2ULONG i;
    O2ULONG result;
    if (session == NULL || native_handle == O2_DOS_NATIVE_INVALID)
        return 0xffffffffUL;
    result = 0xffffffffUL;
    state_lock(session);
    for (i = 1UL; i < O2_DOS_MAX_FIND_HANDLES; ++i) {
        if (session->find_handles[i] == O2_DOS_NATIVE_INVALID) {
            session->find_handles[i] = native_handle;
            result = i;
            break;
        }
    }
    state_unlock(session);
    return result;
}

O2APIRET os2_dos_get_find_handle(struct Os2DosSession *session, O2ULONG hdir,
                                 O2NATIVE *native_handle)
{
    O2NATIVE h;
    if (session == NULL || native_handle == NULL || hdir == 0UL ||
        hdir >= O2_DOS_MAX_FIND_HANDLES)
        return O2_ERROR_INVALID_HANDLE;
    state_lock(session);
    h = session->find_handles[hdir];
    state_unlock(session);
    if (h == O2_DOS_NATIVE_INVALID)
        return O2_ERROR_INVALID_HANDLE;
    *native_handle = h;
    return O2_NO_ERROR;
}

void os2_dos_free_find_handle(struct Os2DosSession *session, O2ULONG hdir)
{
    if (session == NULL || hdir == 0UL || hdir >= O2_DOS_MAX_FIND_HANDLES)
        return;
    state_lock(session);
    session->find_handles[hdir] = O2_DOS_NATIVE_INVALID;
    state_unlock(session);
}

int os2_dos_store_child(struct Os2DosSession *session, O2NATIVE process,
                        O2ULONG pid)
{
    unsigned int i;
    int stored;
    if (session == NULL || process == O2_DOS_NATIVE_INVALID || pid == 0UL)
        return 0;
    stored = 0;
    state_lock(session);
    for (i = 0U; i < O2_DOS_MAX_CHILDREN; ++i) {
        if (session->children[i].process == O2_DOS_NATIVE_INVALID) {
            session->children[i].process = process;
            session->children[i].pid = pid;
            stored = 1;
            break;
        }
    }
    state_unlock(session);
    return stored;
}

O2NATIVE os2_dos_take_child(struct Os2DosSession *session, O2ULONG wanted_pid,
                            O2ULONG *actual_pid)
{
    unsigned int i;
    O2NATIVE process;
    process = O2_DOS_NATIVE_INVALID;
    if (actual_pid != NULL)
        *actual_pid = 0UL;
    if (session == NULL)
        return process;
    state_lock(session);
    for (i = 0U; i < O2_DOS_MAX_CHILDREN; ++i) {
        if (session->children[i].process != O2_DOS_NATIVE_INVALID &&
            (wanted_pid == 0UL || session->children[i].pid == wanted_pid)) {
            process = session->children[i].process;
            if (actual_pid != NULL)
                *actual_pid = session->children[i].pid;
            session->children[i].process = O2_DOS_NATIVE_INVALID;
            session->children[i].pid = 0UL;
            break;
        }
    }
    state_unlock(session);
    return process;
}

void os2_dos_put_child(struct Os2DosSession *session, O2NATIVE process,
                       O2ULONG pid)
{
    if (process != O2_DOS_NATIVE_INVALID)
        (void)os2_dos_store_child(session, process, pid);
}

static O2ULONG event_make_handle(unsigned int slot, O2ULONG generation)
{
    return (generation << 8) | (O2ULONG)(slot + 1U);
}

static struct Os2DosEventSem *event_from_handle(struct Os2DosSession *session,
                                                 O2ULONG hev)
{
    unsigned int raw_slot;
    unsigned int slot;
    O2ULONG generation;
    struct Os2DosEventSem *sem;

    if (session == NULL)
        return NULL;
    raw_slot = (unsigned int)(hev & 0xffUL);
    if (raw_slot == 0U || raw_slot > O2_DOS_EVENT_SLOTS)
        return NULL;
    slot = raw_slot - 1U;
    generation = hev >> 8;
    sem = &session->events[slot];
    if (generation == 0UL || sem->native_event == O2_DOS_NATIVE_INVALID ||
        sem->generation != generation)
        return NULL;
    return sem;
}

static O2ULONG sub_align8(O2ULONG cb)
{
    if (cb > 0xfffffff8UL)
        return 0UL;
    return (cb + 7UL) & ~7UL;
}

static struct Os2DosSubPool *find_subpool(struct Os2DosSession *session,
                                          void *base)
{
    unsigned int i;
    if (session == NULL)
        return NULL;
    for (i = 0U; i < O2_DOS_MAX_SUBPOOLS; ++i) {
        if (session->subpools[i].in_use && session->subpools[i].base == base)
            return &session->subpools[i];
    }
    return NULL;
}

static struct Os2DosSubPool *new_subpool(struct Os2DosSession *session,
                                         void *base)
{
    unsigned int i;
    if (session == NULL)
        return NULL;
    for (i = 0U; i < O2_DOS_MAX_SUBPOOLS; ++i) {
        if (!session->subpools[i].in_use) {
            memset(&session->subpools[i], 0, sizeof(session->subpools[i]));
            session->subpools[i].in_use = 1;
            session->subpools[i].base = base;
            return &session->subpools[i];
        }
    }
    return NULL;
}

static void sub_coalesce(struct Os2DosSubPool *pool)
{
    unsigned int i;
    if (pool == NULL)
        return;
    i = 0U;
    while (i + 1U < pool->nranges) {
        struct Os2DosSubRange *a;
        struct Os2DosSubRange *b;
        a = &pool->ranges[i];
        b = &pool->ranges[i + 1U];
        if (a->is_free && b->is_free && a->off + a->len == b->off) {
            unsigned int j;
            a->len += b->len;
            for (j = i + 1U; j + 1U < pool->nranges; ++j)
                pool->ranges[j] = pool->ranges[j + 1U];
            --pool->nranges;
        } else {
            ++i;
        }
    }
}


unsigned short os2_dos_DosSetSigHandler(struct Os2DosSession *session,
                                         void *routine, void *prev_address,
                                         unsigned short *prev_action,
                                         unsigned short action,
                                         unsigned short sig_number)
{
    struct Os2DosSignalSlot *slot;
    O2ULONG handler;

    if (session == NULL)
        return (unsigned short)O2_ERROR_INVALID_PARAMETER;
    /* Historical SIG_* values are 1..7; slot zero is deliberately unused. */
    if (sig_number == 0U || sig_number >= O2_DOS_SIGNAL_SLOTS)
        return (unsigned short)209U; /* ERROR_INVALID_SIGNAL_NUMBER */
    if (action > 4U)
        return (unsigned short)O2_ERROR_INVALID_FUNCTION;

    state_lock(session);
    slot = &session->signal_slots[sig_number];
    if (prev_address != NULL)
        *(O2ULONG *)prev_address = slot->handler;
    if (prev_action != NULL)
        *prev_action = slot->action;

    /* SIGA_ACKNOWLEDGE is an operation on an already-delivered signal, not
     * a persistent disposition.  The remaining actions become process state.
     * Only SIGA_ACCEPT has a callable handler; KILL/IGNORE/ERROR do not. */
    if (action != 4U) {
        handler = (action == 2U) ? (O2ULONG)(uintptr_t)routine : 0UL;
        slot->handler = handler;
        slot->action = action;
    }
    state_unlock(session);
    return (unsigned short)O2_NO_ERROR;
}

unsigned short os2_dos_DosSetVec(struct Os2DosSession *session,
                                  unsigned short vector, void *routine,
                                  O2ULONG *prev_address)
{
    unsigned int i;
    int free_slot;
    O2ULONG handler;

    if (session == NULL)
        return (unsigned short)O2_ERROR_INVALID_PARAMETER;

    handler = (O2ULONG)(uintptr_t)routine;
    free_slot = -1;
    state_lock(session);
    for (i = 0U; i < O2_DOS_VECTOR_SLOTS; ++i) {
        if (session->vector_slots[i].in_use) {
            if (session->vector_slots[i].vector == vector) {
                if (prev_address != NULL)
                    *prev_address = session->vector_slots[i].handler;
                if (handler == 0UL) {
                    session->vector_slots[i].in_use = 0;
                    session->vector_slots[i].handler = 0UL;
                    session->vector_slots[i].vector = 0U;
                } else {
                    session->vector_slots[i].handler = handler;
                }
                state_unlock(session);
                return (unsigned short)O2_NO_ERROR;
            }
        } else if (free_slot < 0) {
            free_slot = (int)i;
        }
    }

    if (prev_address != NULL)
        *prev_address = 0UL;
    if (handler == 0UL) {
        state_unlock(session);
        return (unsigned short)O2_NO_ERROR;
    }
    if (free_slot < 0) {
        state_unlock(session);
        return (unsigned short)O2_ERROR_INVALID_FUNCTION;
    }
    session->vector_slots[(unsigned int)free_slot].in_use = 1;
    session->vector_slots[(unsigned int)free_slot].vector = vector;
    session->vector_slots[(unsigned int)free_slot].handler = handler;
    state_unlock(session);
    return (unsigned short)O2_NO_ERROR;
}

unsigned short os2_dos_DosFlagProcess(struct Os2DosSession *session,
                                      unsigned short process_id,
                                      unsigned short action_code,
                                      unsigned short flag_number,
                                      unsigned short flag_argument)
{
    (void)session;
    (void)process_id;
    (void)action_code;
    (void)flag_number;
    (void)flag_argument;
    return (unsigned short)O2_NO_ERROR;
}

O2APIRET os2_dos_DosBeep(struct Os2DosSession *session,
                         O2ULONG frequency, O2ULONG duration)
{
    struct Os2DosArgs_DosBeep call_args;
    if (frequency == 0UL)
        return os2_dos_DosSleep(session, duration);
    if (frequency < 37UL || frequency > 32767UL)
        return O2_ERROR_INVALID_FREQUENCY;
    if (duration == 0UL)
        return O2_NO_ERROR;
    call_args.frequency = frequency;
    call_args.duration = duration;
    return dispatch_call(session, OS2_DOS_CALL_DOSBEEP, &call_args);
}

O2APIRET os2_dos_Dos16Beep(struct Os2DosSession *session,
                           O2ULONG frequency, O2ULONG duration)
{
    return os2_dos_DosBeep(session, frequency, duration);
}

O2APIRET os2_dos_DosSleep(struct Os2DosSession *session, O2ULONG milliseconds)
{
    if (session == NULL || session->backend == NULL ||
        session->backend->sleep_ms == NULL)
        return O2_ERROR_INVALID_FUNCTION;
    return session->backend->sleep_ms(session->backend_opaque, milliseconds);
}

O2APIRET os2_dos_Dos16Sleep(struct Os2DosSession *session, O2ULONG milliseconds)
{
    return os2_dos_DosSleep(session, milliseconds);
}

O2APIRET os2_dos_DosCreateEventSem(struct Os2DosSession *session,
                                    const char *name, O2ULONG *phev,
                                    O2ULONG flags, O2ULONG initialState)
{
    unsigned int i;
    int free_slot;
    struct Os2DosEventSem *sem;
    O2NATIVE native_event;
    O2APIRET rc;
    size_t n;

    (void)flags;
    if (session == NULL || phev == NULL)
        return O2_ERROR_INVALID_PARAMETER;
    if (name != NULL) {
        n = strlen(name);
        if (n > O2_DOS_EVENT_NAME_MAX)
            return O2_ERROR_BUFFER_OVERFLOW;
    }
    if (session->backend == NULL || session->backend->event_create == NULL)
        return O2_ERROR_INVALID_FUNCTION;

    state_lock(session);
    free_slot = -1;
    for (i = 0U; i < O2_DOS_EVENT_SLOTS; ++i) {
        sem = &session->events[i];
        if (sem->native_event == O2_DOS_NATIVE_INVALID) {
            if (free_slot < 0)
                free_slot = (int)i;
            continue;
        }
        if (name != NULL && sem->name[0] != '\0' &&
            strcmp(sem->name, name) == 0) {
            state_unlock(session);
            return O2_ERROR_DUPLICATE_NAME;
        }
    }
    if (free_slot < 0) {
        state_unlock(session);
        return O2_ERROR_TOO_MANY_OPENS;
    }

    native_event = O2_DOS_NATIVE_INVALID;
    rc = session->backend->event_create(session->backend_opaque,
                                        initialState != 0UL, &native_event);
    if (rc != O2_NO_ERROR) {
        state_unlock(session);
        return rc;
    }
    sem = &session->events[(unsigned int)free_slot];
    ++sem->generation;
    if (sem->generation == 0UL)
        sem->generation = 1UL;
    sem->native_event = native_event;
    sem->refs = 1UL;
    sem->post_count = initialState ? 1UL : 0UL;
    sem->name[0] = '\0';
    if (name != NULL)
        strcpy(sem->name, name);
    *phev = event_make_handle((unsigned int)free_slot, sem->generation);
    state_unlock(session);
    return O2_NO_ERROR;
}

O2APIRET os2_dos_DosOpenEventSem(struct Os2DosSession *session,
                                  const char *name, O2ULONG *phev)
{
    unsigned int i;
    struct Os2DosEventSem *sem;
    if (session == NULL || phev == NULL)
        return O2_ERROR_INVALID_PARAMETER;
    state_lock(session);
    if (name != NULL) {
        for (i = 0U; i < O2_DOS_EVENT_SLOTS; ++i) {
            sem = &session->events[i];
            if (sem->native_event != O2_DOS_NATIVE_INVALID &&
                sem->name[0] != '\0' && strcmp(sem->name, name) == 0) {
                ++sem->refs;
                *phev = event_make_handle(i, sem->generation);
                state_unlock(session);
                return O2_NO_ERROR;
            }
        }
        state_unlock(session);
        return O2_ERROR_INVALID_HANDLE;
    }
    sem = event_from_handle(session, *phev);
    if (sem == NULL) {
        state_unlock(session);
        return O2_ERROR_INVALID_HANDLE;
    }
    ++sem->refs;
    state_unlock(session);
    return O2_NO_ERROR;
}

O2APIRET os2_dos_DosCloseEventSem(struct Os2DosSession *session, O2ULONG hev)
{
    struct Os2DosEventSem *sem;
    O2NATIVE native_event;
    O2APIRET rc;
    if (session == NULL)
        return O2_ERROR_INVALID_PARAMETER;
    state_lock(session);
    sem = event_from_handle(session, hev);
    if (sem == NULL) {
        state_unlock(session);
        return O2_ERROR_INVALID_HANDLE;
    }
    --sem->refs;
    if (sem->refs != 0UL) {
        state_unlock(session);
        return O2_NO_ERROR;
    }
    native_event = sem->native_event;
    sem->native_event = O2_DOS_NATIVE_INVALID;
    sem->post_count = 0UL;
    sem->name[0] = '\0';
    state_unlock(session);
    if (session->backend == NULL || session->backend->event_close == NULL)
        return O2_ERROR_INVALID_FUNCTION;
    rc = session->backend->event_close(session->backend_opaque, native_event);
    return rc;
}

O2APIRET os2_dos_DosResetEventSem(struct Os2DosSession *session, O2ULONG hev,
                                   O2ULONG *postCount)
{
    struct Os2DosEventSem *sem;
    O2ULONG old_count;
    O2APIRET rc;
    if (session == NULL || postCount == NULL)
        return O2_ERROR_INVALID_PARAMETER;
    if (session->backend == NULL || session->backend->event_reset == NULL)
        return O2_ERROR_INVALID_FUNCTION;
    state_lock(session);
    sem = event_from_handle(session, hev);
    if (sem == NULL) {
        state_unlock(session);
        return O2_ERROR_INVALID_HANDLE;
    }
    old_count = sem->post_count;
    *postCount = old_count;
    if (old_count == 0UL) {
        state_unlock(session);
        return O2_ERROR_ALREADY_RESET;
    }
    rc = session->backend->event_reset(session->backend_opaque,
                                       sem->native_event);
    if (rc == O2_NO_ERROR)
        sem->post_count = 0UL;
    state_unlock(session);
    return rc;
}

O2APIRET os2_dos_DosPostEventSem(struct Os2DosSession *session, O2ULONG hev)
{
    struct Os2DosEventSem *sem;
    int already_posted;
    O2APIRET rc;
    if (session == NULL)
        return O2_ERROR_INVALID_PARAMETER;
    if (session->backend == NULL || session->backend->event_post == NULL)
        return O2_ERROR_INVALID_FUNCTION;
    state_lock(session);
    sem = event_from_handle(session, hev);
    if (sem == NULL) {
        state_unlock(session);
        return O2_ERROR_INVALID_HANDLE;
    }
    already_posted = sem->post_count != 0UL;
    rc = session->backend->event_post(session->backend_opaque,
                                      sem->native_event);
    if (rc == O2_NO_ERROR)
        ++sem->post_count;
    state_unlock(session);
    if (rc != O2_NO_ERROR)
        return rc;
    return already_posted ? O2_ERROR_ALREADY_POSTED : O2_NO_ERROR;
}

O2APIRET os2_dos_DosWaitEventSem(struct Os2DosSession *session, O2ULONG hev,
                                  O2ULONG timeout)
{
    struct Os2DosEventSem *sem;
    O2NATIVE native_event;
    if (session == NULL)
        return O2_ERROR_INVALID_PARAMETER;
    if (session->backend == NULL || session->backend->event_wait == NULL)
        return O2_ERROR_INVALID_FUNCTION;
    state_lock(session);
    sem = event_from_handle(session, hev);
    if (sem == NULL) {
        state_unlock(session);
        return O2_ERROR_INVALID_HANDLE;
    }
    native_event = sem->native_event;
    state_unlock(session);
    return session->backend->event_wait(session->backend_opaque,
                                        native_event, timeout);
}

O2APIRET os2_dos_DosQueryEventSem(struct Os2DosSession *session, O2ULONG hev,
                                   O2ULONG *postCount)
{
    struct Os2DosEventSem *sem;
    if (session == NULL || postCount == NULL)
        return O2_ERROR_INVALID_PARAMETER;
    state_lock(session);
    sem = event_from_handle(session, hev);
    if (sem == NULL) {
        state_unlock(session);
        return O2_ERROR_INVALID_HANDLE;
    }
    *postCount = sem->post_count;
    state_unlock(session);
    return O2_NO_ERROR;
}

O2APIRET os2_dos_DosError(struct Os2DosSession *session, O2ULONG flags)
{
    if (session == NULL)
        return O2_ERROR_INVALID_PARAMETER;
    if ((flags & ~3UL) != 0UL)
        return O2_ERROR_INVALID_PARAMETER;
    state_lock(session);
    session->error_flags = flags;
    state_unlock(session);
    return O2_NO_ERROR;
}

O2APIRET os2_dos_DosQueryAppType(struct Os2DosSession *session,
                                  const char *path, O2ULONG *appType)
{
    return os2_dos_DosQAppType(session, path, appType);
}

O2APIRET os2_dos_DosExitList(struct Os2DosSession *session,
                              O2ULONG orderCode,
                              void (__cdecl *routine)(O2ULONG))
{
    O2ULONG fn;
    (void)session;
    fn = orderCode & 0xffUL;
    if (fn < 1UL || fn > 3UL)
        return O2_ERROR_INVALID_FUNCTION;
    if ((fn == 1UL || fn == 2UL) && routine == NULL)
        return O2_ERROR_INVALID_PARAMETER;
    return O2_NO_ERROR;
}

O2APIRET os2_dos_DosEnterCritSec(struct Os2DosSession *session)
{
    (void)session;
    return O2_NO_ERROR;
}

O2APIRET os2_dos_DosSubSetMem(struct Os2DosSession *session, void *base,
                               O2ULONG flags, O2ULONG size)
{
    struct Os2DosSubPool *pool;
    O2ULONG usable;
    O2APIRET rc;
    if (session == NULL || base == NULL || size < O2_DOS_SUBPOOL_HEADER)
        return O2_ERROR_INVALID_PARAMETER;
    if (session->backend != NULL &&
        session->backend->validate_native_range != NULL) {
        rc = session->backend->validate_native_range(session->backend_opaque,
                                                      base, size);
        if (rc != O2_NO_ERROR)
            return rc;
    }
    state_lock(session);
    pool = find_subpool(session, base);
    if (pool == NULL) {
        if ((flags & O2_DOSSUB_GROW) != 0UL &&
            (flags & O2_DOSSUB_INIT) == 0UL) {
            state_unlock(session);
            return O2_ERROR_INVALID_PARAMETER;
        }
        pool = new_subpool(session, base);
        if (pool == NULL) {
            state_unlock(session);
            return O2_ERROR_NOT_ENOUGH_MEMORY;
        }
        pool->size = size;
        pool->flags = flags;
        usable = sub_align8(size - O2_DOS_SUBPOOL_HEADER);
        if (usable > size - O2_DOS_SUBPOOL_HEADER)
            usable -= 8UL;
        pool->nranges = 1U;
        pool->ranges[0].off = O2_DOS_SUBPOOL_HEADER;
        pool->ranges[0].len = usable;
        pool->ranges[0].is_free = 1;
        memset(base, 0, (size_t)O2_DOS_SUBPOOL_HEADER);
    } else {
        if (size < pool->size) {
            state_unlock(session);
            return O2_ERROR_INVALID_PARAMETER;
        }
        if (size > pool->size) {
            O2ULONG old_aligned;
            O2ULONG new_aligned;
            O2ULONG extra;
            unsigned int n;
            old_aligned = pool->size & ~7UL;
            new_aligned = size & ~7UL;
            extra = new_aligned > old_aligned ? new_aligned - old_aligned : 0UL;
            if (extra != 0UL) {
                n = pool->nranges;
                if (n != 0U && pool->ranges[n - 1U].is_free &&
                    pool->ranges[n - 1U].off + pool->ranges[n - 1U].len ==
                    old_aligned) {
                    pool->ranges[n - 1U].len += extra;
                } else {
                    if (n >= O2_DOS_MAX_SUBRANGES) {
                        state_unlock(session);
                        return O2_ERROR_NOT_ENOUGH_MEMORY;
                    }
                    pool->ranges[n].off = old_aligned;
                    pool->ranges[n].len = extra;
                    pool->ranges[n].is_free = 1;
                    ++pool->nranges;
                }
            }
            pool->size = size;
        }
        pool->flags |= flags;
    }
    state_unlock(session);
    return O2_NO_ERROR;
}

O2APIRET os2_dos_DosSubAllocMem(struct Os2DosSession *session, void *base,
                                 void **ppBlock, O2ULONG size)
{
    struct Os2DosSubPool *pool;
    O2ULONG need;
    unsigned int i;
    if (session == NULL || base == NULL || ppBlock == NULL || size == 0UL)
        return O2_ERROR_INVALID_PARAMETER;
    state_lock(session);
    pool = find_subpool(session, base);
    if (pool == NULL) {
        state_unlock(session);
        return O2_ERROR_INVALID_PARAMETER;
    }
    need = sub_align8(size);
    if (need == 0UL) {
        state_unlock(session);
        return O2_ERROR_NOT_ENOUGH_MEMORY;
    }
    for (i = 0U; i < pool->nranges; ++i) {
        struct Os2DosSubRange *r;
        r = &pool->ranges[i];
        if (!r->is_free || r->len < need)
            continue;
        if (r->len > need) {
            unsigned int j;
            if (pool->nranges >= O2_DOS_MAX_SUBRANGES) {
                state_unlock(session);
                return O2_ERROR_NOT_ENOUGH_MEMORY;
            }
            for (j = pool->nranges; j > i + 1U; --j)
                pool->ranges[j] = pool->ranges[j - 1U];
            pool->ranges[i + 1U].off = r->off + need;
            pool->ranges[i + 1U].len = r->len - need;
            pool->ranges[i + 1U].is_free = 1;
            ++pool->nranges;
            r = &pool->ranges[i];
            r->len = need;
        }
        r->is_free = 0;
        *ppBlock = (void *)((unsigned char *)base + r->off);
        state_unlock(session);
        return O2_NO_ERROR;
    }
    *ppBlock = NULL;
    state_unlock(session);
    return O2_ERROR_NOT_ENOUGH_MEMORY;
}

O2APIRET os2_dos_DosSubFreeMem(struct Os2DosSession *session, void *base,
                                void *block, O2ULONG size)
{
    struct Os2DosSubPool *pool;
    O2ULONG off;
    O2ULONG cb;
    unsigned int i;
    if (session == NULL || base == NULL || block == NULL || size == 0UL)
        return O2_ERROR_INVALID_PARAMETER;
    state_lock(session);
    pool = find_subpool(session, base);
    if (pool == NULL || (unsigned char *)block < (unsigned char *)base) {
        state_unlock(session);
        return O2_ERROR_INVALID_PARAMETER;
    }
    off = (O2ULONG)((unsigned char *)block - (unsigned char *)base);
    cb = sub_align8(size);
    for (i = 0U; i < pool->nranges; ++i) {
        struct Os2DosSubRange *r;
        r = &pool->ranges[i];
        if (r->off == off && !r->is_free) {
            if (cb == 0UL || cb != r->len) {
                state_unlock(session);
                return O2_ERROR_INVALID_PARAMETER;
            }
            r->is_free = 1;
            sub_coalesce(pool);
            state_unlock(session);
            return O2_NO_ERROR;
        }
    }
    state_unlock(session);
    return O2_ERROR_INVALID_PARAMETER;
}

O2APIRET os2_dos_DosSubUnsetMem(struct Os2DosSession *session, void *base)
{
    struct Os2DosSubPool *pool;
    if (session == NULL || base == NULL)
        return O2_ERROR_INVALID_PARAMETER;
    state_lock(session);
    pool = find_subpool(session, base);
    if (pool == NULL) {
        state_unlock(session);
        return O2_ERROR_INVALID_PARAMETER;
    }
    memset(pool, 0, sizeof(*pool));
    state_unlock(session);
    return O2_NO_ERROR;
}

O2APIRET os2_dos_DosSetExceptionHandler(struct Os2DosSession *session,
                                         struct O2ExceptionRegistrationRecord *rec)
{
    if (session == NULL || rec == NULL)
        return O2_ERROR_INVALID_PARAMETER;
    state_lock(session);
    rec->prev_structure = session->exception_head;
    session->exception_head = rec;
    state_unlock(session);
    if (session->backend != NULL &&
        session->backend->exception_head_changed != NULL)
        session->backend->exception_head_changed(session->backend_opaque,
                                                 session->exception_head);
    return O2_NO_ERROR;
}

O2APIRET os2_dos_DosUnsetExceptionHandler(struct Os2DosSession *session,
                                           struct O2ExceptionRegistrationRecord *rec)
{
    struct O2ExceptionRegistrationRecord *p;
    struct O2ExceptionRegistrationRecord *next;
    unsigned int guard;
    if (session == NULL || rec == NULL)
        return O2_ERROR_INVALID_PARAMETER;
    state_lock(session);
    if (session->exception_head == rec) {
        session->exception_head = rec->prev_structure;
    } else {
        p = session->exception_head;
        guard = 0U;
        while (p != NULL &&
               p != (struct O2ExceptionRegistrationRecord *)(uintptr_t)0xffffffffUL &&
               guard++ < 64U) {
            next = p->prev_structure;
            if (next == rec) {
                p->prev_structure = rec->prev_structure;
                break;
            }
            p = next;
        }
    }
    state_unlock(session);
    if (session->backend != NULL &&
        session->backend->exception_head_changed != NULL)
        session->backend->exception_head_changed(session->backend_opaque,
                                                 session->exception_head);
    return O2_NO_ERROR;
}

O2APIRET os2_dos_DosSetSignalExceptionFocus(struct Os2DosSession *session,
                                             O2ULONG enable,
                                             O2ULONG *pulTimes)
{
    if (session == NULL || pulTimes == NULL || (enable != 0UL && enable != 1UL))
        return O2_ERROR_INVALID_PARAMETER;
    state_lock(session);
    if (enable != 0UL) {
        if (session->signal_exception_focus_count != 0xffffffffUL)
            ++session->signal_exception_focus_count;
    } else if (session->signal_exception_focus_count != 0UL) {
        --session->signal_exception_focus_count;
    }
    *pulTimes = session->signal_exception_focus_count;
    state_unlock(session);
    return O2_NO_ERROR;
}

O2APIRET os2_dos_DosSetRelMaxFH(struct Os2DosSession *session,
                                 O2LONG *pcbReqCount,
                                 O2ULONG *pcbCurMaxFH)
{
    O2LONG req;
    O2LONG wanted;
    O2ULONG floor;
    O2ULONG i;
    if (session == NULL || pcbReqCount == NULL || pcbCurMaxFH == NULL)
        return O2_ERROR_INVALID_PARAMETER;
    state_lock(session);
    req = *pcbReqCount;
    floor = 20UL;
    for (i = 3UL; i < O2_DOS_MAX_HANDLES; ++i) {
        if (session->file_handles[i] != O2_DOS_NATIVE_INVALID && i + 1UL > floor)
            floor = i + 1UL;
    }
    if (req > 0) {
        if ((O2ULONG)req >= O2_DOS_MAX_HANDLES - session->max_file_handles)
            wanted = (O2LONG)O2_DOS_MAX_HANDLES;
        else
            wanted = (O2LONG)session->max_file_handles + req;
        session->max_file_handles = (O2ULONG)wanted;
    } else if (req < 0) {
        if (req <= -(O2LONG)session->max_file_handles)
            wanted = 0;
        else
            wanted = (O2LONG)session->max_file_handles + req;
        if (wanted < (O2LONG)floor)
            wanted = (O2LONG)floor;
        if (wanted < 20)
            wanted = 20;
        session->max_file_handles = (O2ULONG)wanted;
    }
    *pcbCurMaxFH = session->max_file_handles;
    state_unlock(session);
    return O2_NO_ERROR;
}

O2APIRET os2_dos_DosAcknowledgeSignalException(struct Os2DosSession *session,
                                                O2ULONG signalNum)
{
    (void)session;
    (void)signalNum;
    return O2_NO_ERROR;
}

O2APIRET os2_dos_DosDevIOCtl(struct Os2DosSession *session, O2HFILE hDevice, O2ULONG category, O2ULONG function, void *pParmList, O2ULONG cbParmLengthMax, O2ULONG *pcbParmLengthInOut, void *pDataArea, O2ULONG cbDataLengthMax, O2ULONG *pcbDataLengthInOut)
{
    struct Os2DosArgs_DosDevIOCtl call_args;
    call_args.hDevice = hDevice;
    call_args.category = category;
    call_args.function = function;
    call_args.pParmList = pParmList;
    call_args.cbParmLengthMax = cbParmLengthMax;
    call_args.pcbParmLengthInOut = pcbParmLengthInOut;
    call_args.pDataArea = pDataArea;
    call_args.cbDataLengthMax = cbDataLengthMax;
    call_args.pcbDataLengthInOut = pcbDataLengthInOut;
    return dispatch_call(session, OS2_DOS_CALL_DOSDEVIOCTL, &call_args);
}

O2APIRET os2_dos_DosFindClose(struct Os2DosSession *session, O2ULONG hdir)
{
    struct Os2DosArgs_DosFindClose call_args;
    call_args.hdir = hdir;
    return dispatch_call(session, OS2_DOS_CALL_DOSFINDCLOSE, &call_args);
}

O2APIRET os2_dos_DosFindFirst(struct Os2DosSession *session, const char *filespec, O2ULONG *phdir, O2ULONG attributes, void *findbuf, O2ULONG cbBuf, O2ULONG *pCount, O2ULONG infoLevel)
{
    struct Os2DosArgs_DosFindFirst call_args;
    call_args.filespec = filespec;
    call_args.phdir = phdir;
    call_args.attributes = attributes;
    call_args.findbuf = findbuf;
    call_args.cbBuf = cbBuf;
    call_args.pCount = pCount;
    call_args.infoLevel = infoLevel;
    return dispatch_call(session, OS2_DOS_CALL_DOSFINDFIRST, &call_args);
}

O2APIRET os2_dos_DosFindNext(struct Os2DosSession *session, O2ULONG hdir, void *findbuf, O2ULONG cbBuf, O2ULONG *pCount)
{
    struct Os2DosArgs_DosFindNext call_args;
    call_args.hdir = hdir;
    call_args.findbuf = findbuf;
    call_args.cbBuf = cbBuf;
    call_args.pCount = pCount;
    return dispatch_call(session, OS2_DOS_CALL_DOSFINDNEXT, &call_args);
}

O2APIRET os2_dos_DosCreateThread(struct Os2DosSession *session, O2ULONG *ptid, O2THREADFN fn, O2ULONG param, O2ULONG flags, O2ULONG stackSize)
{
    struct Os2DosArgs_DosCreateThread call_args;
    call_args.ptid = ptid;
    call_args.fn = fn;
    call_args.param = param;
    call_args.flags = flags;
    call_args.stackSize = stackSize;
    return dispatch_call(session, OS2_DOS_CALL_DOSCREATETHREAD, &call_args);
}

O2APIRET os2_dos_DosCreateMutexSem(struct Os2DosSession *session, const char *name, O2ULONG *phmtx, O2ULONG flags, O2ULONG initialOwner)
{
    struct Os2DosArgs_DosCreateMutexSem call_args;
    call_args.name = name;
    call_args.phmtx = phmtx;
    call_args.flags = flags;
    call_args.initialOwner = initialOwner;
    return dispatch_call(session, OS2_DOS_CALL_DOSCREATEMUTEXSEM, &call_args);
}

O2APIRET os2_dos_DosOpenMutexSem(struct Os2DosSession *session, const char *name, O2ULONG *phmtx)
{
    struct Os2DosArgs_DosOpenMutexSem call_args;
    call_args.name = name;
    call_args.phmtx = phmtx;
    return dispatch_call(session, OS2_DOS_CALL_DOSOPENMUTEXSEM, &call_args);
}

O2APIRET os2_dos_DosCloseMutexSem(struct Os2DosSession *session, O2ULONG hmtx)
{
    struct Os2DosArgs_DosCloseMutexSem call_args;
    call_args.hmtx = hmtx;
    return dispatch_call(session, OS2_DOS_CALL_DOSCLOSEMUTEXSEM, &call_args);
}

O2APIRET os2_dos_DosRequestMutexSem(struct Os2DosSession *session, O2ULONG hmtx, O2ULONG timeout)
{
    struct Os2DosArgs_DosRequestMutexSem call_args;
    call_args.hmtx = hmtx;
    call_args.timeout = timeout;
    return dispatch_call(session, OS2_DOS_CALL_DOSREQUESTMUTEXSEM, &call_args);
}

O2APIRET os2_dos_DosReleaseMutexSem(struct Os2DosSession *session, O2ULONG hmtx)
{
    struct Os2DosArgs_DosReleaseMutexSem call_args;
    call_args.hmtx = hmtx;
    return dispatch_call(session, OS2_DOS_CALL_DOSRELEASEMUTEXSEM, &call_args);
}

O2APIRET os2_dos_DosGetDateTime(struct Os2DosSession *session, void *buffer)
{
    struct Os2DosArgs_DosGetDateTime call_args;
    call_args.buffer = buffer;
    return dispatch_call(session, OS2_DOS_CALL_DOSGETDATETIME, &call_args);
}

O2APIRET os2_dos_DosSetDefaultDisk(struct Os2DosSession *session, O2ULONG diskNum)
{
    struct Os2DosArgs_DosSetDefaultDisk call_args;
    call_args.diskNum = diskNum;
    return dispatch_call(session, OS2_DOS_CALL_DOSSETDEFAULTDISK, &call_args);
}

O2APIRET os2_dos_DosScanEnv(struct Os2DosSession *session, const char *name, char **value)
{
    struct Os2DosArgs_DosScanEnv call_args;
    call_args.name = name;
    call_args.value = value;
    return dispatch_call(session, OS2_DOS_CALL_DOSSCANENV, &call_args);
}

O2APIRET os2_dos_DosSearchPath(struct Os2DosSession *session, O2ULONG flags, const char *pathOrName, const char *filename, char *buffer, O2ULONG cbBuffer)
{
    struct Os2DosArgs_DosSearchPath call_args;
    call_args.flags = flags;
    call_args.pathOrName = pathOrName;
    call_args.filename = filename;
    call_args.buffer = buffer;
    call_args.cbBuffer = cbBuffer;
    return dispatch_call(session, OS2_DOS_CALL_DOSSEARCHPATH, &call_args);
}

O2APIRET os2_dos_DosGetInfoBlocks(struct Os2DosSession *session, void **pptib, void **pppib)
{
    struct Os2DosArgs_DosGetInfoBlocks call_args;
    call_args.pptib = pptib;
    call_args.pppib = pppib;
    return dispatch_call(session, OS2_DOS_CALL_DOSGETINFOBLOCKS, &call_args);
}

O2APIRET os2_dos_DosSetFileInfo(struct Os2DosSession *session, O2HFILE hFile, O2ULONG level, const void *buffer, O2ULONG cb)
{
    struct Os2DosArgs_DosSetFileInfo call_args;
    call_args.hFile = hFile;
    call_args.level = level;
    call_args.buffer = buffer;
    call_args.cb = cb;
    return dispatch_call(session, OS2_DOS_CALL_DOSSETFILEINFO, &call_args);
}

O2APIRET os2_dos_DosSetPathInfo(struct Os2DosSession *session, const char *path, O2ULONG level, const void *buffer, O2ULONG cb, O2ULONG options)
{
    struct Os2DosArgs_DosSetPathInfo call_args;
    call_args.path = path;
    call_args.level = level;
    call_args.buffer = buffer;
    call_args.cb = cb;
    call_args.options = options;
    return dispatch_call(session, OS2_DOS_CALL_DOSSETPATHINFO, &call_args);
}

O2APIRET os2_dos_DosQAppType(struct Os2DosSession *session, const char *path, O2ULONG *appType)
{
    struct Os2DosArgs_DosQAppType call_args;
    call_args.path = path;
    call_args.appType = appType;
    return dispatch_call(session, OS2_DOS_CALL_DOSQAPPTYPE, &call_args);
}

O2APIRET os2_dos_DosQueryPathInfo(struct Os2DosSession *session, const char *path, O2ULONG level, void *buffer, O2ULONG cb)
{
    struct Os2DosArgs_DosQueryPathInfo call_args;
    call_args.path = path;
    call_args.level = level;
    call_args.buffer = buffer;
    call_args.cb = cb;
    return dispatch_call(session, OS2_DOS_CALL_DOSQUERYPATHINFO, &call_args);
}

O2APIRET os2_dos_DosQueryHType(struct Os2DosSession *session, O2HFILE hFile, O2ULONG *pType, O2ULONG *pAttr)
{
    struct Os2DosArgs_DosQueryHType call_args;
    call_args.hFile = hFile;
    call_args.pType = pType;
    call_args.pAttr = pAttr;
    return dispatch_call(session, OS2_DOS_CALL_DOSQUERYHTYPE, &call_args);
}

void os2_dos_DosExit(struct Os2DosSession *session, O2ULONG action, O2ULONG result)
{
    struct Os2DosArgs_DosExit call_args;
    call_args.action = action;
    call_args.result = result;
    (void)dispatch_call(session, OS2_DOS_CALL_DOSEXIT, &call_args);
}

O2APIRET os2_dos_DosDeleteDir(struct Os2DosSession *session, const char *path)
{
    struct Os2DosArgs_DosDeleteDir call_args;
    call_args.path = path;
    return dispatch_call(session, OS2_DOS_CALL_DOSDELETEDIR, &call_args);
}

O2APIRET os2_dos_DosSetCurrentDir(struct Os2DosSession *session, const char *path)
{
    struct Os2DosArgs_DosSetCurrentDir call_args;
    call_args.path = path;
    return dispatch_call(session, OS2_DOS_CALL_DOSSETCURRENTDIR, &call_args);
}

O2APIRET os2_dos_DosSetFilePtr(struct Os2DosSession *session, O2HFILE hFile, O2LONG distance, O2ULONG method, O2ULONG *newpos)
{
    struct Os2DosArgs_DosSetFilePtr call_args;
    call_args.hFile = hFile;
    call_args.distance = distance;
    call_args.method = method;
    call_args.newpos = newpos;
    return dispatch_call(session, OS2_DOS_CALL_DOSSETFILEPTR, &call_args);
}

O2APIRET os2_dos_DosClose(struct Os2DosSession *session, O2HFILE hFile)
{
    struct Os2DosArgs_DosClose call_args;
    call_args.hFile = hFile;
    return dispatch_call(session, OS2_DOS_CALL_DOSCLOSE, &call_args);
}

O2APIRET os2_dos_DosCreatePipe(struct Os2DosSession *session, O2HFILE *pread, O2HFILE *pwrite, O2ULONG size)
{
    O2APIRET rc;
    struct Os2DosArgs_DosCreatePipe call_args;
    call_args.pread = pread;
    call_args.pwrite = pwrite;
    call_args.size = size;
    rc=dispatch_call(session, OS2_DOS_CALL_DOSCREATEPIPE, &call_args);
    if(!rc) { os2_dos_remember_mode(session,*pread,0x40);os2_dos_remember_mode(session,*pwrite,0x41); }
    return rc;
}

O2APIRET os2_dos_DosDupHandle(struct Os2DosSession *session, O2HFILE oldFile, O2HFILE *pnewFile)
{
    O2APIRET rc;O2ULONG mode=0;int known;
    struct Os2DosArgs_DosDupHandle call_args;
    call_args.oldFile = oldFile;
    call_args.pnewFile = pnewFile;
    known=os2_dos_DosQueryFHState(session,oldFile,&mode)==0;
    rc=dispatch_call(session, OS2_DOS_CALL_DOSDUPHANDLE, &call_args);
    if(!rc && known) os2_dos_remember_mode(session,*pnewFile,
        *pnewFile==oldFile?mode:mode&~0x80UL);
    return rc;
}

O2APIRET os2_dos_DosDelete(struct Os2DosSession *session, const char *path, O2ULONG reserved)
{
    struct Os2DosArgs_DosDelete call_args;
    call_args.path = path;
    call_args.reserved = reserved;
    return dispatch_call(session, OS2_DOS_CALL_DOSDELETE, &call_args);
}

O2APIRET os2_dos_DosCreateDir(struct Os2DosSession *session, const char *path, void *pEA, O2ULONG reserved)
{
    struct Os2DosArgs_DosCreateDir call_args;
    call_args.path = path;
    call_args.pEA = pEA;
    call_args.reserved = reserved;
    return dispatch_call(session, OS2_DOS_CALL_DOSCREATEDIR, &call_args);
}

O2APIRET os2_dos_DosMove(struct Os2DosSession *session, const char *oldPath, const char *newPath)
{
    struct Os2DosArgs_DosMove call_args;
    call_args.oldPath = oldPath;
    call_args.newPath = newPath;
    return dispatch_call(session, OS2_DOS_CALL_DOSMOVE, &call_args);
}

O2APIRET os2_dos_DosSetFileSize(struct Os2DosSession *session, O2HFILE hFile, O2ULONG size)
{
    struct Os2DosArgs_DosSetFileSize call_args;
    call_args.hFile = hFile;
    call_args.size = size;
    return dispatch_call(session, OS2_DOS_CALL_DOSSETFILESIZE, &call_args);
}

O2APIRET os2_dos_DosOpen(struct Os2DosSession *session, const char *path, O2HFILE *phFile, O2ULONG *pAction, O2ULONG cbFile, O2ULONG attr, O2ULONG openFlags, O2ULONG openMode, void *pEA, O2ULONG reserved)
{
    struct Os2DosArgs_DosOpen call_args;
    call_args.path = path;
    call_args.phFile = phFile;
    call_args.pAction = pAction;
    call_args.cbFile = cbFile;
    call_args.attr = attr;
    call_args.openFlags = openFlags;
    call_args.openMode = openMode;
    call_args.pEA = pEA;
    call_args.reserved = reserved;
    return dispatch_call(session, OS2_DOS_CALL_DOSOPEN, &call_args);
}

O2APIRET os2_dos_DosQueryCurrentDir(struct Os2DosSession *session, O2ULONG diskNum, char *buffer, O2ULONG *pcb)
{
    struct Os2DosArgs_DosQueryCurrentDir call_args;
    call_args.diskNum = diskNum;
    call_args.buffer = buffer;
    call_args.pcb = pcb;
    return dispatch_call(session, OS2_DOS_CALL_DOSQUERYCURRENTDIR, &call_args);
}

O2APIRET os2_dos_DosQueryCurrentDisk(struct Os2DosSession *session, O2ULONG *pdisk, O2ULONG *plogical)
{
    struct Os2DosArgs_DosQueryCurrentDisk call_args;
    call_args.pdisk = pdisk;
    call_args.plogical = plogical;
    return dispatch_call(session, OS2_DOS_CALL_DOSQUERYCURRENTDISK, &call_args);
}

O2APIRET os2_dos_DosQueryFileInfo(struct Os2DosSession *session, O2HFILE hFile, O2ULONG level, void *buffer, O2ULONG cb)
{
    struct Os2DosArgs_DosQueryFileInfo call_args;
    call_args.hFile = hFile;
    call_args.level = level;
    call_args.buffer = buffer;
    call_args.cb = cb;
    return dispatch_call(session, OS2_DOS_CALL_DOSQUERYFILEINFO, &call_args);
}

O2APIRET os2_dos_DosRead(struct Os2DosSession *session, O2HFILE hFile, void *buffer, O2ULONG count, O2ULONG *actual)
{
    struct Os2DosArgs_DosRead call_args;
    call_args.hFile = hFile;
    call_args.buffer = buffer;
    call_args.count = count;
    call_args.actual = actual;
    return dispatch_call(session, OS2_DOS_CALL_DOSREAD, &call_args);
}

O2APIRET os2_dos_DosWrite(struct Os2DosSession *session, O2HFILE hFile, const void *buffer, O2ULONG count, O2ULONG *actual)
{
    struct Os2DosArgs_DosWrite call_args;
    call_args.hFile = hFile;
    call_args.buffer = buffer;
    call_args.count = count;
    call_args.actual = actual;
    return dispatch_call(session, OS2_DOS_CALL_DOSWRITE, &call_args);
}

O2APIRET os2_dos_DosExecPgm(struct Os2DosSession *session, char *objectName, O2LONG objectNameLen, O2ULONG execFlag, const char *args, const char *env, struct O2ResultCodes *results, const char *program)
{
    struct Os2DosArgs_DosExecPgm call_args;
    call_args.objectName = objectName;
    call_args.objectNameLen = objectNameLen;
    call_args.execFlag = execFlag;
    call_args.args = args;
    call_args.env = env;
    call_args.results = results;
    call_args.program = program;
    return dispatch_call(session, OS2_DOS_CALL_DOSEXECPGM, &call_args);
}

O2APIRET os2_dos_DosWaitChild(struct Os2DosSession *session, O2ULONG action, O2ULONG option, struct O2ResultCodes *results, O2ULONG *ppid, O2ULONG pid)
{
    struct Os2DosArgs_DosWaitChild call_args;
    call_args.action = action;
    call_args.option = option;
    call_args.results = results;
    call_args.ppid = ppid;
    call_args.pid = pid;
    return dispatch_call(session, OS2_DOS_CALL_DOSWAITCHILD, &call_args);
}

O2APIRET os2_dos_DosAllocMem(struct Os2DosSession *session, void **ppBase, O2ULONG size, O2ULONG flags, O2ULONG reserved)
{
    struct Os2DosArgs_DosAllocMem call_args;
    call_args.ppBase = ppBase;
    call_args.size = size;
    call_args.flags = flags;
    call_args.reserved = reserved;
    return dispatch_call(session, OS2_DOS_CALL_DOSALLOCMEM, &call_args);
}

O2APIRET os2_dos_DosFreeMem(struct Os2DosSession *session, void *base)
{
    struct Os2DosArgs_DosFreeMem call_args;
    call_args.base = base;
    return dispatch_call(session, OS2_DOS_CALL_DOSFREEMEM, &call_args);
}

O2APIRET os2_dos_DosSetMem(struct Os2DosSession *session, void *base, O2ULONG size, O2ULONG flags)
{
    struct Os2DosArgs_DosSetMem call_args;
    call_args.base = base;
    call_args.size = size;
    call_args.flags = flags;
    return dispatch_call(session, OS2_DOS_CALL_DOSSETMEM, &call_args);
}

O2APIRET os2_dos_DosGetResource(struct Os2DosSession *session,O2ULONG module,O2ULONG type,O2ULONG id,void **buffer)
{
    struct Os2DosArgs_DosGetResource args;
    args.module=module; args.type=type; args.id=id; args.buffer=buffer;
    return dispatch_call(session,OS2_DOS_CALL_DOSGETRESOURCE,&args);
}

O2APIRET os2_dos_DosFreeResource(struct Os2DosSession *session,void *buffer)
{
    struct Os2DosArgs_DosFreeResource args;
    args.buffer=buffer;
    return dispatch_call(session,OS2_DOS_CALL_DOSFREERESOURCE,&args);
}

O2APIRET os2_dos_DosQueryMem(struct Os2DosSession *session, void *base, O2ULONG *pcb, O2ULONG *pflags)
{
    struct Os2DosArgs_DosQueryMem call_args;
    call_args.base = base;
    call_args.pcb = pcb;
    call_args.pflags = pflags;
    return dispatch_call(session, OS2_DOS_CALL_DOSQUERYMEM, &call_args);
}

O2APIRET os2_dos_DosLoadModule(struct Os2DosSession *session, char *objectName, O2ULONG objectNameLen, const char *moduleName, O2ULONG *moduleHandle)
{
    struct Os2DosArgs_DosLoadModule call_args;
    call_args.objectName = objectName;
    call_args.objectNameLen = objectNameLen;
    call_args.moduleName = moduleName;
    call_args.moduleHandle = moduleHandle;
    return dispatch_call(session, OS2_DOS_CALL_DOSLOADMODULE, &call_args);
}

O2APIRET os2_dos_DosQueryModuleHandle(struct Os2DosSession *session, const char *moduleName, O2ULONG *moduleHandle)
{
    struct Os2DosArgs_DosQueryModuleHandle call_args;
    call_args.moduleName = moduleName;
    call_args.moduleHandle = moduleHandle;
    return dispatch_call(session, OS2_DOS_CALL_DOSQUERYMODULEHANDLE, &call_args);
}

O2APIRET os2_dos_DosQueryModuleName(struct Os2DosSession *session, O2ULONG moduleHandle, O2ULONG cb, char *buffer)
{
    struct Os2DosArgs_DosQueryModuleName call_args;
    call_args.moduleHandle = moduleHandle;
    call_args.cb = cb;
    call_args.buffer = buffer;
    return dispatch_call(session, OS2_DOS_CALL_DOSQUERYMODULENAME, &call_args);
}

O2APIRET os2_dos_DosQueryProcAddr(struct Os2DosSession *session, O2ULONG moduleHandle, O2ULONG ordinal, const char *procName, void **procAddress)
{
    struct Os2DosArgs_DosQueryProcAddr call_args;
    call_args.moduleHandle = moduleHandle;
    call_args.ordinal = ordinal;
    call_args.procName = procName;
    call_args.procAddress = procAddress;
    return dispatch_call(session, OS2_DOS_CALL_DOSQUERYPROCADDR, &call_args);
}

O2APIRET os2_dos_DosFreeModule(struct Os2DosSession *session, O2ULONG moduleHandle)
{
    struct Os2DosArgs_DosFreeModule call_args;
    call_args.moduleHandle = moduleHandle;
    return dispatch_call(session, OS2_DOS_CALL_DOSFREEMODULE, &call_args);
}

O2APIRET os2_dos_DosQuerySysInfo(struct Os2DosSession *session, O2ULONG first, O2ULONG last, void *buffer, O2ULONG cb)
{
    struct Os2DosArgs_DosQuerySysInfo call_args;
    call_args.first = first;
    call_args.last = last;
    call_args.buffer = buffer;
    call_args.cb = cb;
    return dispatch_call(session, OS2_DOS_CALL_DOSQUERYSYSINFO, &call_args);
}

static void nls_read_countrycode(const void *countrycode,
                                 O2ULONG *country, O2ULONG *codepage)
{
    const unsigned char *p;
    O2ULONG c;
    O2ULONG cp;

    *country = 0UL;
    *codepage = 0UL;
    if (countrycode == NULL)
        return;
    p = (const unsigned char *)countrycode;
    c = (O2ULONG)p[0] | ((O2ULONG)p[1] << 8) |
        ((O2ULONG)p[2] << 16) | ((O2ULONG)p[3] << 24);
    cp = (O2ULONG)p[4] | ((O2ULONG)p[5] << 8) |
         ((O2ULONG)p[6] << 16) | ((O2ULONG)p[7] << 24);
    *country = c;
    *codepage = cp;
}

O2APIRET os2_dos_DosSetProcessCp(struct Os2DosSession *session,
                                  O2ULONG codepage)
{
    if (session == NULL)
        return O2_ERROR_INVALID_PARAMETER;
    return (O2APIRET)os2_nls_set_process_cp(&session->nls,
                                             (uint32_t)codepage);
}

O2APIRET os2_dos_DosQueryCp(struct Os2DosSession *session, O2ULONG cb,
                            O2ULONG *codepages, O2ULONG *actual)
{
    if (session == NULL)
        return O2_ERROR_INVALID_PARAMETER;
    return (O2APIRET)os2_nls_query_cp(&session->nls, (uint32_t)cb,
                                       (uint32_t *)codepages,
                                       (uint32_t *)actual);
}

O2APIRET os2_dos_DosQueryCtryInfo(struct Os2DosSession *session, O2ULONG cb,
                                  const void *countrycode, void *countryinfo,
                                  O2ULONG *actual)
{
    O2ULONG country;
    O2ULONG codepage;
    if (session == NULL || actual == NULL || countryinfo == NULL)
        return O2_ERROR_INVALID_PARAMETER;
    nls_read_countrycode(countrycode, &country, &codepage);
    return (O2APIRET)os2_nls_query_country_info(
        &session->nls, (uint32_t)country, (uint32_t)codepage,
        (uint32_t)cb, (unsigned char *)countryinfo, (uint32_t *)actual);
}

O2APIRET os2_dos_DosQueryDBCSEnv(struct Os2DosSession *session, O2ULONG cb,
                                 const void *countrycode, void *buffer)
{
    O2ULONG country;
    O2ULONG codepage;
    if (session == NULL || buffer == NULL || cb == 0UL)
        return O2_ERROR_INVALID_PARAMETER;
    nls_read_countrycode(countrycode, &country, &codepage);
    return (O2APIRET)os2_nls_query_dbcs_env(
        &session->nls, (uint32_t)country, (uint32_t)codepage,
        (uint32_t)cb, (unsigned char *)buffer);
}

O2APIRET os2_dos_DosMapCase(struct Os2DosSession *session, O2ULONG cb,
                            const void *countrycode, void *buffer)
{
    O2ULONG country;
    O2ULONG codepage;
    if (session == NULL || (cb != 0UL && buffer == NULL))
        return O2_ERROR_INVALID_PARAMETER;
    nls_read_countrycode(countrycode, &country, &codepage);
    return (O2APIRET)os2_nls_map_case(&session->nls,
                                      (uint32_t)country,
                                      (uint32_t)codepage,
                                      (unsigned char *)buffer,
                                      (uint32_t)cb);
}

O2APIRET os2_dos_DosSetPriority(struct Os2DosSession *session, O2ULONG scope, O2ULONG prtyClass, O2LONG delta, O2ULONG porTid)
{
    struct Os2DosArgs_DosSetPriority call_args;
    call_args.scope = scope;
    call_args.prtyClass = prtyClass;
    call_args.delta = delta;
    call_args.porTid = porTid;
    return dispatch_call(session, OS2_DOS_CALL_DOSSETPRIORITY, &call_args);
}
