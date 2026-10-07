/* Native personality calls may synchronously call back (WM_CREATE, SendMsg,
 * dialogs). No Tiny386 step is active when this adapter runs. The suspended
 * native frame stays on the host stack, while every guest continuation,
 * including FS and x87, is saved explicitly. The nested run loop uses the same
 * scheduler as the top-level loop; HOSTCALL threads cannot resume prematurely.
 */
static int run_guest_until(struct Runtime *,struct GuestCallback *);

static uint32_t pm_current_tid(void *opaque)
{
    struct GuestThread *t=current_guest_thread(opaque);return t?t->tid:0;
}
static int executable_address(struct Runtime *rt,uint32_t entry)
{
    struct LeImage *x;unsigned i,j;
    for(i=0;i<=MAX_MODULES;i++){
        x=i==0?rt->main_image:(rt->modules[i-1]?&rt->modules[i-1]->image:NULL);
        if(!x)continue;
        for(j=0;j<x->num_objects;j++){struct LeObject *o=&x->objects[j];
            if((o->flags&(OBJ_EXEC|OBJ_BIG))==(OBJ_EXEC|OBJ_BIG)&&entry>=o->mapped_addr&&entry-o->mapped_addr<o->size)return 1;
        }
    }
    return 0;
}
static int pm_code_address(void *opaque,uint32_t entry){return executable_address(opaque,entry);}
static uint32_t pm_oldproc_veneer(void *opaque,uint32_t slot){return hostcall_stub(opaque,HC_PM_OLDPROC,slot);}

static int invoke_guest(void *opaque,uint32_t tid,uint32_t entry,
                        const uint32_t *args,uint32_t argc,uint32_t *result)
{
    struct Runtime *rt=opaque;struct GuestThread *owner=find_thread(rt,tid),*caller=current_guest_thread(rt);
    struct GuestThread saved_owner;CPUI386_State saved_cpu,regs;struct GuestCallback frame;
    enum GuestThreadState caller_state;uint32_t bytes,i;int previous_thread,sw,ex,retry,pending;uint32_t api;
    {struct GuestCallback *f=rt->callback_top;unsigned depth=0;for(;f;f=f->previous)if(++depth>=32)return 0;}
    if(result)*result=0;
    if(!caller||!owner||rt->process_exited||!result||argc>8||!executable_address(rt,entry)||owner->callback_depth>=16)return 0;
    if(owner->state==THREAD_FREE||owner->state==THREAD_DEAD||owner->state==THREAD_SUSPENDED)return 0;
    /* Cross-thread synchronous sends during a semaphore wait need a separate
     * sent-message queue. Reject rather than erase an outstanding wait. */
    if(owner!=caller&&owner->state!=THREAD_RUNNABLE&&owner->state!=THREAD_WAIT_PM){
        fprintf(stderr,"soft386: callback TID %u is busy (state=%u)\n",tid,(unsigned)owner->state);return 0;
    }
    cpui386_get_state(rt->cpu,&saved_cpu);saved_cpu.ip=saved_cpu.next_ip;
    saved_owner=*owner;if(owner==caller)saved_owner.regs=saved_cpu;
    regs=saved_owner.regs;bytes=(argc+1u)*4u;
    if(regs.gpr[4]<owner->stack_base||regs.gpr[4]>owner->stack_base+owner->stack_size||
       regs.gpr[4]-owner->stack_base<bytes+4u)return 0;
    memset(&frame,0,sizeof(frame));frame.tid=tid;frame.argc=argc;frame.entry_sp=regs.gpr[4]-bytes;
    /* H2O diagnostic-only trace: observe, but do not alter, the first three
     * OS/2 WM_TIMER (0x24) guest callbacks while --trace-native is active. */
    if(rt->pm.trace&&argc>=2&&args&&args[1]==0x24u&&rt->timer_trace_count<3u){
        frame.cpu_trace_id=++rt->timer_trace_count;
        frame.cpu_trace_limit=512u;
        fprintf(stderr,"soft386: cb-cpu begin id=%u TID=%u entry=%08X msg=0024 limit=%u\n",frame.cpu_trace_id,tid,entry,frame.cpu_trace_limit);
    }
    frame.previous=rt->callback_top;rt->callback_top=&frame;
    if(rt->exit_processing&&!rt->exit_frame)rt->exit_frame=&frame;
    guest_put_u32(rt,frame.entry_sp,GUEST_CALLBACK_RETURN_STUB);
    for(i=0;i<argc;i++)guest_put_u32(rt,frame.entry_sp+4u+4u*i,args[i]);
    regs.gpr[4]=frame.entry_sp;regs.ip=regs.next_ip=entry;regs.flags&=~0x400u;
    previous_thread=rt->current_thread;caller_state=caller->state;
    sw=rt->switch_requested;ex=rt->current_thread_exited;retry=rt->retry_hostcall;
    pending=rt->pending_hostcall;api=rt->pending_api;
    if(owner!=caller){caller->regs=saved_cpu;caller->regs_valid=1;caller->state=THREAD_HOSTCALL;}
    rt->current_thread=(int)(owner-rt->threads);owner->state=THREAD_RUNNING;owner->callback_depth++;
    rt->switch_requested=rt->current_thread_exited=rt->retry_hostcall=0;
    if(!cpui386_set_state(rt->cpu,&regs))fatal("callback CPU restore failed");
    if(rt->trace_hc||rt->pm.trace){
        unsigned ai;fprintf(stderr,"soft386: callback enter TID=%u entry=%08X depth=%u argc=%u args=",tid,entry,owner->callback_depth,argc);
        for(ai=0;ai<argc;ai++)fprintf(stderr,"%s%08X",ai?",":"",args[ai]);
        fputc('\n',stderr);
    }
    {int rc=run_guest_until(rt,&frame);if(!frame.done&&!rt->process_exited){rt->process_exited=1;rt->process_rc=(uint32_t)(rc?rc:1);rt->process_reason="callback execution failed";}}
    *result=frame.result;
    if(frame.cpu_trace_id)fprintf(stderr,"soft386: cb-cpu end id=%u steps=%u result=%08X done=%u\n",frame.cpu_trace_id,frame.cpu_trace_steps,frame.result,frame.done?1u:0u);
    if(rt->trace_hc||rt->pm.trace)fprintf(stderr,"soft386: callback leave TID=%u entry=%08X result=%08X done=%u\n",tid,entry,frame.result,frame.done?1u:0u);
    /* Do not revive a thread that called DosExit from inside the callback. */
    if(owner->state==THREAD_DEAD){saved_owner.state=THREAD_DEAD;saved_owner.owns_stack=owner->owns_stack;}
    *owner=saved_owner;
    if(rt->exit_frame==&frame)rt->exit_frame=NULL;
    rt->callback_top=frame.previous;rt->current_thread=previous_thread;
    if(caller!=owner)caller->state=caller_state;
    if(!cpui386_set_state(rt->cpu,&saved_cpu))fatal("callback continuation restore failed");
    rt->switch_requested=sw;rt->current_thread_exited=ex;rt->retry_hostcall=retry;
    rt->pending_hostcall=pending;rt->pending_api=api;
    return frame.done&&!rt->process_exited;
}

static int return_guest_callback(struct Runtime *rt,const CPUI386_State *st)
{
    struct GuestCallback *f=rt->callback_top;struct GuestThread *t=current_guest_thread(rt);
    if(!f||!t||f->tid!=t->tid||st->next_ip!=GUEST_CALLBACK_RETURN_STUB+8u||
       (st->gpr[4]!=f->entry_sp&&st->gpr[4]!=f->entry_sp+f->argc*4u)){
        fprintf(stderr,"soft386: invalid callback return frame\n");return 0;
    }
    f->result=guest_u32(rt,st->gpr[4]);f->done=1;return 1;
}

static int initialize_module(struct Runtime *rt,struct GuestModule *g)
{
    uint32_t i,args[2],result=1,entry;
    if(g->state==4||g->state==3)return 1;
    if(g->state!=2)return 0;
    g->state=3;
    for(i=0;i<g->image.num_impmods;i++){
        struct GuestModule *dep=find_guest_module(rt,g->image.modules[i].name);
        if(dep&&dep!=g&&!initialize_module(rt,dep)){g->state=5;return 0;}
    }
    if(g->image.entry_object){
        struct LeObject *o=&g->image.objects[g->image.entry_object-1];
        if(g->image.entry_offset>=o->size)fatal("DLL initialization entry outside object");
        entry=o->mapped_addr+g->image.entry_offset;
        args[0]=g->handle;args[1]=0;
        if((g->image.module_flags&4u)&&!(g->image.module_flags&0x40000000u)){args[0]=0;args[1]=g->handle;}
        if(!invoke_guest(rt,pm_current_tid(rt),entry,args,2,&result)||!result){g->state=5;return 0;}
    }
    g->state=4;g->init_order=++rt->init_serial;return 1;
}
static int initialize_modules(struct Runtime *rt)
{
    unsigned i;for(i=0;i<MAX_MODULES;i++)if(rt->modules[i]&&!initialize_module(rt,rt->modules[i]))return 0;
    return 1;
}
static void terminate_modules(struct Runtime *rt)
{
    uint32_t order=rt->init_serial,args[2],result;unsigned i;int exited=rt->process_exited;uint32_t rc=rt->process_rc;
    struct GuestThread *t=current_guest_thread(rt);
    if(!t||t->state==THREAD_DEAD)return;
    rt->process_exited=0;
    while(order){for(i=0;i<MAX_MODULES;i++){
        struct GuestModule *g=rt->modules[i];
        if(!g||g->init_order!=order||g->term_called||!g->image.entry_object)continue;
        g->term_called=1;
        if(!(g->image.module_flags&0x40000000u))continue;
        args[0]=g->handle;args[1]=1;
        if(!invoke_guest(rt,t->tid,g->image.objects[g->image.entry_object-1].mapped_addr+g->image.entry_offset,args,2,&result))break;
    }if(rt->process_exited)break;order--;}
    rt->process_exited=exited;rt->process_rc=rc;
}

static uint32_t dispatch_modules(struct Runtime *rt,uint32_t ord,uint32_t a,uint32_t b,uint32_t c,uint32_t d)
{
    struct GuestModule *g;struct LeImage *x;char name[4096];uint32_t h,id,addr=0;
    if(ord==318){ /* DosLoadModule: error buffer, capacity, name, HMODULE out */
        if(!d||!guest_range(d,4)||(b&&(!a||!guest_range(a,b)))||!guest_copy_cstr(rt,c,name,sizeof(name)))return 87;
        guest_put_u32(rt,d,0);if(b)rt->ram[a]=0;
        id=system_module_id(name);
        if(id)h=0x100u+(id>>24);
        else{
            g=load_guest_module(rt,name);
            if(!g){if(b){size_t n=strlen(name);if(n>=b)n=b-1;memcpy(rt->ram+a,name,n);rt->ram[a+n]=0;}return 2;}
            if(!initialize_module(rt,g))return 295;
            g->refs++;h=g->handle;
        }guest_put_u32(rt,d,h);return 0;
    }
    if(ord==319){
        if(!b||!guest_range(b,4)||!guest_copy_cstr(rt,a,name,sizeof(name)))return 87;
        id=system_module_id(name);g=find_guest_module(rt,name);
        h=id?0x100u+(id>>24):(g&&g->state!=5?g->handle:0);
        if(!h&&module_equal(name,rt->main_path))h=1;
        if(!h)return 126;guest_put_u32(rt,b,h);return 0;
    }
    g=module_handle(rt,a);x=a==1?rt->main_image:(g&&g->state!=5?&g->image:NULL);
    if(ord==320){
        const char *path=a==1?rt->main_path:(g?g->path:NULL);
        if(!path)return 6;if(!c||!guest_range(c,b)||!b)return 87;
        if(strlen(path)+1>b)return 111;memcpy(rt->ram+c,path,strlen(path)+1);return 0;
    }
    if(ord==321){
        if(!d||!guest_range(d,4)||(c&&!guest_copy_cstr(rt,c,name,sizeof(name))))return 87;
        if(a>0x100u&&a<=0x112u&&a!=0x110u){id=(a-0x100u)<<24;
            if(c){if(id==HC_SO32DLL||id==HC_TCP32DLL)b=soft386_net_named_ordinal(id==HC_TCP32DLL,name);
                else if(id==HC_QUECALLS)b=soft386_queue_named_ordinal(name);
                else if(id==HC_PMWP)b=!strcmp(name,"PMWPOrdinal203")?203:0;
                else{if(id<HC_PMWIN||id>HC_HELPMGR)return 127;b=soft386_pm_named_ordinal((id-HC_PMWIN)>>24,name);}}
            if(b)addr=hostcall_stub(rt,id,b);
        }else if(x)addr=c?export_name(x,name):export_ordinal(x,b);
        else return 6;
        if(!addr)return 127;guest_put_u32(rt,d,addr);return 0;
    }
    if(ord==322)return g?50u:6u; /* Physical unload/lifecycle invalidation deferred. */
    if(ord==353)return guest_range(a,1)?0:87; /* image resources remain mapped */
    if(ord==352||ord==572){uint32_t p,n,i;
        if(!a)x=rt->main_image;
        if(!x)return 6;if(!d||!guest_range(d,4))return 87;
        p=x->le+rd32(x->file+x->le+0x50);n=rd32(x->file+x->le+0x54);
        for(i=0;i<n;i++,p+=14)if(rd16(x->file+p)==b&&rd16(x->file+p+2)==c){
            addr=ord==572?rd32(x->file+p+4):x->objects[rd16(x->file+p+8)-1].mapped_addr+rd32(x->file+p+10);
            guest_put_u32(rt,d,addr);return 0;
        }return 1814;
    }
    return 1;
}
