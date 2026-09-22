/* R8 scheduler/mutex core. Included after event and register helpers. */
static void sync_ready(struct GuestThread *t, uint32_t rc)
{
    t->wait_event = 0;
    t->wait_deadline_ms = 0;
    t->wait_order = 0;
    set_saved_thread_rc(t, rc);
    t->state = THREAD_RUNNABLE;
}

static void sync_block(struct Runtime *rt, enum GuestThreadState state,
                       uint32_t handle, uint32_t timeout)
{
    struct GuestThread *t = current_guest_thread(rt);
    t->state = state;
    t->wait_event = handle;
    t->wait_deadline_ms = timeout == SEM_INDEFINITE_WAIT ? 0 :
                         GetTickCount64() + (uint64_t)timeout;
    t->wait_order = ++rt->wait_serial;
    rt->switch_requested = 1;
    fprintf(stderr,"v2: TID %u blocks: %s handle=%08X timeout=%u\n",
            t->tid,thread_state_name(state),handle,timeout);
}

static struct GuestThread *sync_first_waiter(struct Runtime *rt,
                            enum GuestThreadState state, uint32_t handle)
{
    uint32_t i;
    struct GuestThread *best = NULL;
    for (i = 0; i < MAX_THREADS; ++i) {
        struct GuestThread *t = &rt->threads[i];
        if (t->state == state && t->wait_event == handle &&
            (!best || t->wait_order < best->wait_order)) best = t;
    }
    return best;
}

static uint32_t sync_handle(uint32_t tag, uint32_t slot, uint32_t generation)
{
    return tag | (generation << 8) | (slot + 1u);
}

static struct GuestMutex *mutex_from_handle(struct Runtime *rt, uint32_t handle)
{
    uint32_t raw = handle & 255u;
    struct GuestMutex *m;
    if ((handle & 0xc0000000u) != MUTEX_TAG || !raw || raw > MAX_MUTEXES)
        return NULL;
    m = &rt->mutexes[raw - 1u];
    if (!m->used || m->generation != ((handle >> 8) & SYNC_GENERATION_MAX))
        return NULL;
    return m;
}

static void mutex_handoff(struct Runtime *rt, struct GuestMutex *m, uint32_t handle)
{
    struct GuestThread *t = sync_first_waiter(rt, THREAD_WAIT_MUTEX, handle);
    if (!t) return;
    fprintf(stderr,"v2: mutex %08X -> TID %u%s\n",handle,t->tid,
            m->abandoned ? " (owner died)" : "");
    m->owner = t->tid;
    m->count = 1;
    sync_ready(t, m->abandoned ? OS2_ERROR_SEM_OWNER_DIED : OS2_NO_ERROR);
    m->abandoned = 0;
}

static void mutex_owner_exit(struct Runtime *rt, uint32_t tid)
{
    uint32_t i;
    for (i = 0; i < MAX_MUTEXES; ++i) {
        struct GuestMutex *m = &rt->mutexes[i];
        if (!m->used || m->owner != tid) continue;
        m->owner = m->count = 0;
        m->abandoned = 1;
        mutex_handoff(rt, m, sync_handle(MUTEX_TAG, i, m->generation));
    }
}

static uint32_t dispatch_mutex(struct Runtime *rt, uint32_t ordinal,
                              uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
{
    uint32_t i, slot, handle;
    char name[EVENT_NAME_MAX + 1];
    struct GuestMutex *m;
    struct GuestThread *cur = current_guest_thread(rt);
    if (!cur) return OS2_ERROR_INVALID_FUNCTION;
    if (ordinal == 331 || ordinal == 332) {
        if (!a2 || !guest_range(a2, 4)) return OS2_ERROR_INVALID_PARAMETER;
        name[0] = 0;
        if (a1 && !guest_copy_cstr(rt, a1, name, (uint32_t)sizeof(name)))
            return OS2_ERROR_INVALID_PARAMETER;
        if (ordinal == 331 && ((a3 & ~1u) || a4 > 1u))
            return OS2_ERROR_INVALID_PARAMETER;
        slot = MAX_MUTEXES;
        for (i = 0; i < MAX_MUTEXES; ++i) {
            m = &rt->mutexes[i];
            if (!m->used) {
                if (m->generation < SYNC_GENERATION_MAX && slot == MAX_MUTEXES) slot = i;
            } else if (name[0] && _stricmp(name, m->name) == 0) {
                if (ordinal == 331) return OS2_ERROR_DUPLICATE_NAME;
                if (m->refs == 0xffffffffu) return OS2_ERROR_TOO_MANY_OPENS;
                ++m->refs;
                guest_put_u32(rt, a2, sync_handle(MUTEX_TAG, i, m->generation));
                return OS2_NO_ERROR;
            }
        }
        if (ordinal == 332) {
            if (a1) return OS2_ERROR_SEM_NOT_FOUND;
            m = mutex_from_handle(rt, guest_u32(rt, a2));
            if (!m) return OS2_ERROR_INVALID_HANDLE;
            if (m->refs == 0xffffffffu) return OS2_ERROR_TOO_MANY_OPENS;
            ++m->refs;
            return OS2_NO_ERROR;
        }
        if (slot == MAX_MUTEXES) return OS2_ERROR_TOO_MANY_OPENS;
        m = &rt->mutexes[slot];
        ++m->generation; m->used = 1; m->refs = 1;
        m->abandoned = 0; m->owner = a4 ? cur->tid : 0; m->count = a4 ? 1 : 0;
        strcpy(m->name, name);
        guest_put_u32(rt, a2, sync_handle(MUTEX_TAG, slot, m->generation));
        return OS2_NO_ERROR;
    }
    handle = a1;
    m = mutex_from_handle(rt, handle);
    if (!m) return OS2_ERROR_INVALID_HANDLE;
    switch (ordinal) {
    case 333:
        if (m->refs > 1) { --m->refs; return OS2_NO_ERROR; }
        if (m->owner || sync_first_waiter(rt, THREAD_WAIT_MUTEX, handle))
            return OS2_ERROR_SEM_BUSY;
        m->used = 0; m->refs = 0; m->name[0] = 0;
        return OS2_NO_ERROR;
    case 334:
        if (m->owner == cur->tid) {
            if (m->count == 0xffffffffu) return OS2_ERROR_TOO_MANY_SEM_REQUESTS;
            ++m->count;
            return OS2_NO_ERROR;
        }
        if (!m->owner) {
            uint32_t rc = m->abandoned ? OS2_ERROR_SEM_OWNER_DIED : OS2_NO_ERROR;
            m->owner = cur->tid; m->count = 1; m->abandoned = 0;
            return rc;
        }
        if (!a2) return OS2_ERROR_SEM_TIMEOUT;
        sync_block(rt, THREAD_WAIT_MUTEX, handle, a2);
        return OS2_NO_ERROR;
    case 335:
        if (m->owner != cur->tid) return OS2_ERROR_NOT_OWNER;
        if (--m->count == 0) {
            m->owner = 0;
            mutex_handoff(rt, m, handle);
        }
        return OS2_NO_ERROR;
    case 336:
        if (!a2 || !a3 || !a4 || !guest_range(a2,4) ||
            !guest_range(a3,4) || !guest_range(a4,4)) return OS2_ERROR_INVALID_PARAMETER;
        guest_put_u32(rt, a2, m->owner ? 1u : 0u);
        guest_put_u32(rt, a3, m->owner);
        guest_put_u32(rt, a4, m->count);
        return OS2_NO_ERROR;
    default: return OS2_ERROR_INVALID_FUNCTION;
    }
}

static uint32_t guest_sleep(struct Runtime *rt, uint32_t ms)
{
    if (!current_guest_thread(rt)) return OS2_ERROR_INVALID_FUNCTION;
    if (ms) sync_block(rt, THREAD_SLEEP, 0, ms);
    else rt->switch_requested = 1; /* save RUNNING as RUNNABLE, round-robin */
    return OS2_NO_ERROR;
}

static void finish_guest_thread(struct Runtime *rt)
{
    struct GuestThread *t = current_guest_thread(rt);
    struct GuestAlloc *ga;
    uint32_t i;
    cancel_guest_callbacks(t);
    mutex_owner_exit(rt, t->tid);
    t->state = THREAD_DEAD;
    wake_thread_waiters(rt, t->tid);
    /* The current CPU's stack is never saved/resumed after thread exit.
       TID 1 uses the EXE's data object, not an allocator-owned stack. */
    if (t->owns_stack) {
        ga = find_alloc(rt, t->stack_base);
        if (ga) ga->used = 0;
        t->owns_stack = 0;
    }
    rt->current_thread_exited = 1;
    rt->switch_requested = 1;
    for (i = 0; i < MAX_THREADS; ++i)
        if (rt->threads[i].state != THREAD_FREE && rt->threads[i].state != THREAD_DEAD) break;
    if (i == MAX_THREADS) { rt->process_exited = 1; rt->process_rc = 0; }
}

static uint32_t guest_wait_thread(struct Runtime *rt, uint32_t a1, uint32_t a2)
{
        uint32_t target_tid;
        struct GuestThread *target;
        struct GuestThread *cur;
        fprintf(stderr, "v2: DOSCALLS.349 DosWaitThread(ptid=%08X,option=%u)\n", a1, a2);
        if (!guest_range(a1, 4) || (a2 != DCWW_WAIT && a2 != DCWW_NOWAIT))
            return OS2_ERROR_INVALID_PARAMETER;
        target_tid = guest_u32(rt, a1);
        cur = current_guest_thread(rt);
        if (!cur)
            return OS2_ERROR_INVALID_FUNCTION;

        if (target_tid == 0) {
            uint32_t i;
            target = NULL;
            for (i = 0; i < MAX_THREADS; ++i) {
                if (rt->threads[i].state == THREAD_DEAD) {
                    target = &rt->threads[i];
                    break;
                }
            }
            if (target) {
                guest_put_u32(rt, a1, target->tid);
                target->state = THREAD_FREE;
                return OS2_NO_ERROR;
            }
        } else {
            target = find_thread(rt, target_tid);
            if (!target && target_tid >= 1u && target_tid < rt->next_tid)
                return OS2_NO_ERROR; /* issued, completed, slot since recycled */
            if (!target || target == cur)
                return OS2_ERROR_INVALID_THREADID;
            if (target->state == THREAD_DEAD) {
                guest_put_u32(rt, a1, target_tid);
                target->state = THREAD_FREE;
                return OS2_NO_ERROR;
            }
        }

        if (target_tid == 0) {
            uint32_t j;
            for (j = 0; j < MAX_THREADS; ++j)
                if (&rt->threads[j] != cur && rt->threads[j].state != THREAD_FREE &&
                    rt->threads[j].state != THREAD_DEAD) break;
            if (j == MAX_THREADS) return OS2_ERROR_INVALID_THREADID;
        }
        if (a2 == DCWW_NOWAIT)
            return OS2_ERROR_THREAD_NOT_TERMINATED;

        cur->wait_tid = target_tid;
        cur->wait_ptid = a1;
        cur->state = THREAD_WAIT_THREAD;
        rt->switch_requested = 1;
        fprintf(stderr, "v2: thread %u waiting for %s%u\n", cur->tid,
               target_tid ? "TID " : "any TID", target_tid);
        return OS2_NO_ERROR;
    }
