/* Normal-exit callbacks stay in guest memory and run on the exiting thread.
 * IBM ordering: ascending priority, newest registration first for ties. */
static uint32_t guest_exit_list(struct Runtime *rt,uint32_t order,uint32_t entry)
{
    unsigned i,free_slot=64,op=order&255u;
    if(order&0xffff0000u||op<1||op>3)return 87;
    if(op==3){
        if(!rt->exit_processing||!rt->callback_top||rt->callback_top!=rt->exit_frame||rt->callback_top->tid!=pm_current_tid(rt))return 87;
        rt->callback_top->done=1;rt->callback_top->result=0;return 0;
    }
    if(!entry||!executable_address(rt,entry))return 87;
    for(i=0;i<64;i++){
        if(rt->exitlist[i].entry==entry&&op==2){rt->exitlist[i].entry=0;return 0;}
        if(!rt->exitlist[i].entry&&free_slot==64)free_slot=i;
    }
    if(op==2)return 87;if(free_slot==64)return 8;
    rt->exitlist[free_slot].entry=entry;rt->exitlist[free_slot].priority=(order>>8)&255u;
    rt->exitlist[free_slot].serial=++rt->exit_serial;return 0;
}
static void terminate_exit_list(struct Runtime *rt)
{
    unsigned i,k,steps=0;uint32_t arg=0,result,code=rt->process_rc;CPUI386_State st;
    struct GuestThread *t=current_guest_thread(rt);
    if(!t||t->state==THREAD_DEAD)return;
    /* Peer threads no longer execute during normal process termination. */
    for(i=0;i<MAX_THREADS;i++)if(&rt->threads[i]!=t&&rt->threads[i].state!=THREAD_FREE)rt->threads[i].state=THREAD_DEAD;
    for(i=0;i<MAX_MUTEXES;i++)if(rt->mutexes[i].used&&rt->mutexes[i].owner)rt->mutexes[i].owner=t->tid;
    rt->process_exited=0;rt->exit_processing=1;
    for(;;){
        k=64;for(i=0;i<64;i++)if(rt->exitlist[i].entry&&(k==64||rt->exitlist[i].priority<rt->exitlist[k].priority||
            (rt->exitlist[i].priority==rt->exitlist[k].priority&&rt->exitlist[i].serial>rt->exitlist[k].serial)))k=i;
        if(k==64)break;if(++steps>256){code=1;break;}
        {uint32_t entry=rt->exitlist[k].entry;rt->exitlist[k].entry=0;
            cpui386_get_state(rt->cpu,&st);st.gpr[4]=t->stack_base+t->stack_size-20;
            if(!cpui386_set_state(rt->cpu,&st)||!invoke_guest(rt,t->tid,entry,&arg,1,&result)){if(rt->process_rc)code=rt->process_rc;else code=1;break;}}
    }
    rt->exit_processing=0;rt->exit_frame=NULL;rt->process_exited=1;rt->process_rc=code;
}
static uint32_t guest_exception_chain(struct Runtime *rt,unsigned ordinal,uint32_t record)
{
    uint32_t head=info_tib_address((unsigned)rt->current_thread),p;unsigned guard;
    if(!record||!guest_range(record,8))return 87;
    if(ordinal==354){
        if(!executable_address(rt,guest_u32(rt,record+4)))return 87;
        p=guest_u32(rt,head);
        for(guard=0;p&&p!=UINT32_MAX&&guard<128;guard++){
            if(p==record||!guest_range(p,8))return 87;p=guest_u32(rt,p);
        }
        if(guard==128)return 87;
        guest_put_u32(rt,record,guest_u32(rt,head));guest_put_u32(rt,head,record);return 0;
    }
    for(guard=0;guard<128;guard++){
        p=guest_u32(rt,head);if(p==record){guest_put_u32(rt,head,guest_u32(rt,record));return 0;}
        if(!p||p==UINT32_MAX||!guest_range(p,8))return 87;head=p;
    }return 87;
}
