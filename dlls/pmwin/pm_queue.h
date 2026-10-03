/* PM queue ownership is per thread, independently of USER32's implicit queue.
 * Included by pmwin.c and the host regression with controlled Win32 calls. */

/* 716 */
O2HMQ __cdecl WinCreateMsgQueue(O2HAB hab, O2LONG cmsg)
{
    MSG m;
    DWORD tid;
    struct PMCompatThreadState *s=pm_thread_state();
    if(!s) return pm_api_error(0x1011UL); /* PMERR_HEAP_OUT_OF_MEMORY */
    if(s->queue_active) {
        pm_trace("WinCreateMsgQueue exists",(unsigned long)hab,
                 (unsigned long)GetCurrentThreadId(),0x1052UL);
        return pm_api_error(0x1052UL); /* PMERR_MSG_QUEUE_ALREADY_EXISTS */
    }
    if(cmsg<0) return pm_api_error(0x1018UL); /* PMERR_QUEUE_TOO_LARGE */
    PeekMessageA(&m,NULL,WM_USER,WM_USER,PM_NOREMOVE);
    tid=GetCurrentThreadId();
    s->queue_active=1;
    pm_trace("WinCreateMsgQueue",(unsigned long)hab,
             (unsigned long)cmsg,(unsigned long)tid);
    return (O2HMQ)tid;
}

/* 726 */
O2ULONG __cdecl WinDestroyMsgQueue(O2HMQ hmq)
{
    struct PMCompatThreadState *s=pm_thread_state();
    if(!s || !s->queue_active || !hmq || hmq!=(O2HMQ)GetCurrentThreadId())
        return pm_api_error(0x1002UL); /* PMERR_INVALID_HMQ */
    s->queue_active=0;
    s->queue_accel=0;
    pm_trace("WinDestroyMsgQueue",(unsigned long)hmq,0,0);
    return 1;
}

/* 888: release remaining per-thread queue ownership at PM termination.
 * Other HAB validation/lifetime semantics retain the current V1 behavior. */
O2ULONG __cdecl WinTerminate(O2HAB hab)
{
    struct PMCompatThreadState *s=pm_thread_state();
    if(s) { s->queue_active=0; s->queue_accel=0; }
    pm_trace("WinTerminate",(unsigned long)hab,0,0);
    return 1;
}
