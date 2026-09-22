/* Same-thread guest calls use heap-backed continuations, not recursive host
 * run loops. The scheduler may switch threads with any continuation pending.
 * Caller-cleanup 32-bit stack arguments, integer return; no FPU state yet. */
static void cancel_guest_callbacks(struct GuestThread *t)
{
    while (t->callbacks) {
        struct GuestCallback *f=t->callbacks;
        t->callbacks=f->previous;
        free(f);
    }
    t->callback_depth=0;
}

static uint32_t begin_guest_callback(struct Runtime *rt, uint32_t entry,
            const uint32_t *args, uint32_t argc, uint32_t result_out,
            uint32_t resume_rip)
{
    struct GuestThread *t=current_guest_thread(rt);
    struct GuestCallback *f;
    WHV_REGISTER_VALUE regs[THREAD_REG_COUNT];
    uint32_t sp,bytes,i;
    HRESULT hr;
    if (!t || t->state!=THREAD_RUNNING || !entry || !guest_range(entry,1) ||
        argc>8u || (result_out && !guest_range(result_out,4)))
        return OS2_ERROR_INVALID_PARAMETER;
    if (t->callback_depth>=16u) return OS2_ERROR_NOT_ENOUGH_MEMORY;
    hr=WHvGetVirtualProcessorRegisters(rt->partition,0,thread_reg_names,THREAD_REG_COUNT,regs);
    if (FAILED(hr)) return OS2_ERROR_INVALID_FUNCTION;
    sp=(uint32_t)regs[1].Reg64;
    bytes=(argc+1u)*4u;
    /* Four additional bytes are needed by PUSH EAX in the return sentinel. */
    if (sp<t->stack_base || sp>t->stack_base+t->stack_size ||
        sp-t->stack_base<bytes+4u || !guest_range(sp-bytes-4u,bytes+4u))
        return OS2_ERROR_INVALID_PARAMETER;
    f=(struct GuestCallback *)calloc(1,sizeof(*f));
    if (!f) return OS2_ERROR_NOT_ENOUGH_MEMORY;
    memcpy(f->caller,regs,sizeof(regs));
    f->caller[0].Reg64=resume_rip;
    f->result_out=result_out;
    f->entry_sp=sp-bytes;
    f->previous=t->callbacks;
    guest_put_u32(rt,f->entry_sp,GUEST_CALLBACK_RETURN_STUB);
    for (i=0;i<argc;++i) guest_put_u32(rt,f->entry_sp+4u+i*4u,args[i]);
    regs[0].Reg64=entry; regs[1].Reg64=f->entry_sp;
    /* Direction flag must be clear at a C ABI boundary; restored on return. */
    regs[2].Reg64 &= ~0x400ULL;
    hr=WHvSetVirtualProcessorRegisters(rt->partition,0,thread_reg_names,THREAD_REG_COUNT,regs);
    if (FAILED(hr)) { free(f); return OS2_ERROR_INVALID_FUNCTION; }
    t->callbacks=f; ++t->callback_depth;
    return OS2_NO_ERROR;
}

static uint32_t return_guest_callback(struct Runtime *rt, uint32_t esp, uint32_t rip)
{
    struct GuestThread *t=current_guest_thread(rt);
    struct GuestCallback *f=t ? t->callbacks : NULL;
    uint32_t result;
    HRESULT hr;
    if (!f || rip!=GUEST_CALLBACK_RETURN_STUB+6u || esp!=f->entry_sp ||
        !guest_range(esp,4)) return OS2_ERROR_INVALID_PARAMETER;
    result=guest_u32(rt,esp); /* return sentinel preserved guest EAX */
    f->caller[3].Reg64 = f->complete ? f->complete(rt,result) :
                         (f->result_out ? OS2_NO_ERROR : result);
    hr=WHvSetVirtualProcessorRegisters(rt->partition,0,thread_reg_names,THREAD_REG_COUNT,f->caller);
    if (FAILED(hr)) return OS2_ERROR_INVALID_FUNCTION;
    if (f->result_out) guest_put_u32(rt,f->result_out,result);
    t->callbacks=f->previous; --t->callback_depth;
    free(f);
    return OS2_NO_ERROR;
}
