/* Process-local QUECALLS. Data is an opaque guest address/value, never a
 * host pointer and never dereferenced/copied (Sarien passes tone values).
 * FIFO/LIFO/priority ordering; blocking reads/peeks use saved guest contexts.
 */
static struct GuestQueue *queue_from_handle(struct Runtime *rt, uint32_t handle)
{
    uint32_t raw = handle & 255u;
    struct GuestQueue *q;
    if ((handle & 0xc0000000u) != QUEUE_TAG || !raw || raw > MAX_QUEUES) return NULL;
    q = &rt->queues[raw - 1u];
    if (!q->used || q->generation != ((handle >> 8) & SYNC_GENERATION_MAX)) return NULL;
    return q;
}

static int queue_outputs_valid(const uint32_t *a, int peek)
{
    return a[1] && a[2] && a[3] && a[6] &&
           guest_range(a[1],8) && guest_range(a[2],4) &&
           guest_range(a[3],4) && guest_range(a[6],1) &&
           (!peek || (a[4] && guest_range(a[4],4)));
}

static uint32_t queue_deliver(struct Runtime *rt, struct GuestQueue *q,
                              const uint32_t *a, int peek)
{
    uint32_t index = 0, token;
    struct QueueEntry e;
    if (!queue_outputs_valid(a, peek)) return OS2_ERROR_INVALID_PARAMETER;
    token = peek ? guest_u32(rt, a[4]) : a[4];
    if (token) {
        for (index = 0; index < q->count; ++index)
            if (q->entries[index].token == token) break;
        if (index == q->count) return OS2_ERROR_QUE_ELEMENT_NOT_EXIST;
        if (peek) ++index; /* next after the previously peeked element */
    }
    if (index >= q->count) return OS2_ERROR_QUE_EMPTY;
    e = q->entries[index];
    guest_put_u32(rt, a[1], 1u);
    guest_put_u32(rt, a[1]+4, e.request);
    guest_put_u32(rt, a[2], e.length);
    guest_put_u32(rt, a[3], e.data);
    rt->ram[a[6]] = (uint8_t)e.priority;
    if (peek) guest_put_u32(rt, a[4], e.token);
    else {
        --q->count;
        memmove(&q->entries[index], &q->entries[index+1],
                (q->count-index) * sizeof(q->entries[0]));
    }
    return OS2_NO_ERROR;
}

static void queue_wake_readers(struct Runtime *rt, struct GuestQueue *q, uint32_t handle)
{
    struct GuestThread *t;
    while (q->count && (t = sync_first_waiter(rt, THREAD_WAIT_QUEUE, handle)) != NULL) {
        uint32_t rc = queue_deliver(rt, q, t->queue_args, t->queue_peek);
        fprintf(stderr,"v2: queue %08X -> TID %u rc=%u\n",handle,t->tid,rc);
        sync_ready(t, rc);
    }
}

static uint32_t dispatch_queue(struct Runtime *rt, uint32_t ordinal, uint32_t esp)
{
    uint32_t a[8], n, i, slot, handle;
    char name[EVENT_NAME_MAX + 1];
    struct GuestQueue *q;
    struct GuestEventSem *ev;
    n = (ordinal == 9 || ordinal == 13) ? 8u :
        (ordinal == 14 ? 5u : ((ordinal == 15 || ordinal == 16) ? 3u :
        (ordinal == 12 ? 2u : 1u)));
    if (!guest_range(esp, (n+1u)*4u)) return OS2_ERROR_INVALID_PARAMETER;
    memset(a,0,sizeof(a));
    for (i=0;i<n;++i) a[i]=guest_u32(rt,esp+4u+i*4u);
    if (ordinal == 15 || ordinal == 16) {
        if (!a[0] || !guest_range(a[0],4) ||
            (ordinal==15 && (!a[1] || !guest_range(a[1],4))))
            return OS2_ERROR_INVALID_PARAMETER;
        if (!guest_copy_cstr(rt,a[2],name,(uint32_t)sizeof(name)) ||
            _strnicmp(name,"\\QUEUES\\",8)!=0 || !name[8]) return OS2_ERROR_QUE_INVALID_NAME;
        if (ordinal==16 && ((a[1] & ~7u) || (a[1] & 3u)>2u)) return OS2_ERROR_INVALID_PARAMETER;
        slot=MAX_QUEUES;
        for (i=0;i<MAX_QUEUES;++i) {
            q=&rt->queues[i];
            if (!q->used) {
                if (q->generation<SYNC_GENERATION_MAX && slot==MAX_QUEUES) slot=i;
            } else if (_stricmp(name,q->name)==0) {
                if (ordinal==16) return OS2_ERROR_QUE_DUPLICATE;
                if (q->refs==0xffffffffu) return OS2_ERROR_QUE_NO_MEMORY;
                ++q->refs;
                guest_put_u32(rt,a[0],1u);
                guest_put_u32(rt,a[1],sync_handle(QUEUE_TAG,i,q->generation));
                return OS2_NO_ERROR;
            }
        }
        if (ordinal==15) return OS2_ERROR_QUE_NAME_NOT_EXIST;
        if (slot==MAX_QUEUES) return OS2_ERROR_QUE_NO_MEMORY;
        q=&rt->queues[slot];
        ++q->generation; q->used=1; q->refs=1; q->count=0;
        q->discipline=a[1] & 3u; q->next_token=1; q->notify_event=0;
        strcpy(q->name,name);
        guest_put_u32(rt,a[0],sync_handle(QUEUE_TAG,slot,q->generation));
        return OS2_NO_ERROR;
    }
    handle=a[0];
    q=queue_from_handle(rt,handle);
    if (!q) return OS2_ERROR_QUE_INVALID_HANDLE;
    switch (ordinal) {
    case 9: case 13:
    {
        uint32_t rc;
        int peek=ordinal==13;
        if (!queue_outputs_valid(a,peek)) return OS2_ERROR_INVALID_PARAMETER;
        if (a[5]>1u) return OS2_ERROR_QUE_INVALID_WAIT;
        /* Nonzero NOWAIT notification is retained once, generation-checked.
           A zero event is accepted for plain polling. WAIT ignores hsem. */
        if (a[5] && a[7]) {
            ev=event_from_handle(rt,a[7],NULL);
            if (!ev || (q->notify_event && q->notify_event!=a[7]))
                return OS2_ERROR_INVALID_PARAMETER;
            if (!q->notify_event) {
                if (ev->refs==0xffffffffu) return OS2_ERROR_TOO_MANY_OPENS;
                q->notify_event=a[7]; ++ev->refs;
            }
        }
        rc=queue_deliver(rt,q,a,peek);
        if (rc!=OS2_ERROR_QUE_EMPTY || a[5]) return rc;
        /* Only 'first element' blocks. A missing enumeration token is not
           a promise that a future write will recreate that element. */
        if (peek ? guest_u32(rt,a[4]) : a[4]) return rc;
        memcpy(current_guest_thread(rt)->queue_args,a,sizeof(a));
        current_guest_thread(rt)->queue_peek=peek;
        sync_block(rt,THREAD_WAIT_QUEUE,handle,SEM_INDEFINITE_WAIT);
        return OS2_NO_ERROR;
    }
    case 10: q->count=0; return OS2_NO_ERROR;
    case 11:
        if (q->refs>1) { --q->refs; return OS2_NO_ERROR; }
        for (i=0;i<MAX_THREADS;++i) {
            struct GuestThread *t=&rt->threads[i];
            if (t->state==THREAD_WAIT_QUEUE && t->wait_event==handle)
                sync_ready(t,OS2_ERROR_QUE_INVALID_HANDLE);
        }
        if (q->notify_event && (ev=event_from_handle(rt,q->notify_event,NULL))!=NULL) {
            if (--ev->refs==0) {
                for (i=0;i<MAX_THREADS;++i) {
                    struct GuestThread *t=&rt->threads[i];
                    if (t->state==THREAD_WAIT_EVENT && t->wait_event==q->notify_event)
                        sync_ready(t,OS2_ERROR_INVALID_HANDLE);
                }
                ev->used=0; ev->post_count=0; ev->name[0]=0;
            }
        }
        q->used=0; q->refs=0; q->count=0; q->notify_event=0;
        return OS2_NO_ERROR;
    case 12:
        if (!a[1] || !guest_range(a[1],4)) return OS2_ERROR_INVALID_PARAMETER;
        guest_put_u32(rt,a[1],q->count);
        return OS2_NO_ERROR;
    case 14:
    {
        struct QueueEntry e;
        uint32_t pos;
        if (a[4]>15u) return OS2_ERROR_QUE_INVALID_PRIORITY;
        if (q->count==QUEUE_DEPTH || q->next_token==0) return OS2_ERROR_QUE_UNABLE_TO_ADD;
        e.token=q->next_token++; e.request=a[1]; e.length=a[2]; e.data=a[3]; e.priority=a[4];
        pos=q->count;
        if (q->discipline==1u) pos=0;
        else if (q->discipline==2u) {
            for (pos=0;pos<q->count;++pos)
                if (q->entries[pos].priority<e.priority) break;
        }
        memmove(&q->entries[pos+1],&q->entries[pos],(q->count-pos)*sizeof(e));
        q->entries[pos]=e; ++q->count;
        if (q->notify_event && (ev=event_from_handle(rt,q->notify_event,NULL))!=NULL) {
            if (ev->post_count!=0xffffffffu) ++ev->post_count;
            wake_event_waiters(rt,q->notify_event);
        }
        queue_wake_readers(rt,q,handle);
        return OS2_NO_ERROR;
    }
    default: return OS2_ERROR_INVALID_FUNCTION;
    }
}
