static uint32_t xcall(uint32_t ord,const uint32_t *a)
{uint32_t i;for(i=0;i<(ord==283?7u:5u);i++)guest_put_u32(&rt,0x1804+i*4,a[i]);return dispatch_process(&rt,ord,0x1800);}
#define XCALL(ord,...) xcall(ord,(uint32_t[]){__VA_ARGS__})
static void finish_mock_child(uint32_t slot,uint32_t code)
{
 struct ChildProcess *c=&children[slot];((struct MockProcess *)c->process)->done=1;
 ((struct MockProcess *)c->process)->exitcode=code;c->packet->normal_exit=1;c->packet->exit_rc=code;
}
static void test_process(void)
{
 struct ChildPacket *p;uint32_t a[7]={0,0,0,0x3000,0x3100,0x3200,0x3300},pid;DWORD ms;
 reset();memcpy(rt.ram+0x3000,"alternate name\0one \"two three\"\0",32);
 memcpy(rt.ram+0x3100,"TEST=child\0EMPTY=\0\0",18);strcpy((char *)rt.ram+0x3300,"child.exe");
 p=calloc(1,sizeof(*p));assert(p);assert(process_make_packet(&rt,a,p)==0);
 assert(!strcmp(p->data+p->program_len,"TEST=child"));
 assert(!strcmp(p->data+p->program_len+p->env_len,"alternate name"));
 assert(!strcmp(p->data+p->program_len+p->env_len+15,"one \"two three\""));
 child_boot=p;child_startup(rt.ram,&a[0],&a[1],&a[2]);child_boot=NULL;
 assert(!strcmp((char*)rt.ram+a[0],"TEST=child"));free(p);
 /* Sync blocks only its caller until startup AND exit, preserving result 259. */
 assert(XCALL(283,0,0,0,0x3000,0x3100,0x3200,0x3300)==0);
 assert(rt.threads[0].state==THREAD_WAIT_PROCESS&&children[0].packet->parent_pid==77);
 spawn();park(0);select_thread(1);assert(next_wait_timeout(&rt,&ms)&&ms==10);
 clock_ms+=10;wake_expired_waiters(&rt);assert(rt.threads[0].state==THREAD_WAIT_PROCESS);
 children[0].packet->ready=1;clock_ms+=10;wake_expired_waiters(&rt);assert(rt.threads[0].state==THREAD_WAIT_PROCESS);
 finish_mock_child(0,259);clock_ms+=10;wake_expired_waiters(&rt);
 assert(rt.threads[0].state==THREAD_RUNNABLE&&guest_u32(&rt,0x3200)==0&&guest_u32(&rt,0x3204)==259&&!children[0].process);
 select_thread(0);
 /* Async-result returns PID after startup; nowait distinguishes still running. */
 assert(XCALL(283,0,0,2,0x3000,0x3100,0x3200,0x3300)==0);
 children[0].packet->ready=1;clock_ms+=10;wake_expired_waiters(&rt);select_thread(0);
 pid=guest_u32(&rt,0x3200);assert(pid==children[0].pid&&guest_u32(&rt,0x3204)==0);
 assert(XCALL(280,0,1,0x3400,0x3410,pid)==129);
 assert(XCALL(280,0,1,0x3400,0x3410,pid+1)==128);
 assert(XCALL(280,0,0,0x3400,0x3410,pid)==0&&rt.threads[0].state==THREAD_WAIT_PROCESS);
 finish_mock_child(0,37);clock_ms+=10;wake_expired_waiters(&rt);select_thread(0);
 assert(guest_u32(&rt,0x3404)==37&&guest_u32(&rt,0x3410)==pid);
 assert(XCALL(280,0,1,0x3400,0x3410,pid)==128);
 /* Failure before entry is an Exec error, not a normal guest exit. */
 assert(XCALL(283,0x3500,32,0,0x3000,0x3100,0x3200,0x3300)==0);
 ((struct MockProcess *)children[0].process)->done=1;
 clock_ms+=10;wake_expired_waiters(&rt);assert(rt.threads[0].regs[3].Reg64==31);
 assert(!strcmp((char*)rt.ram+0x3500,"child.exe"));select_thread(0);
 assert(XCALL(283,0,0,4,0x3000,0x3100,0x3200,0x3300)==1);
 assert(XCALL(283,0,0,0,RAM_SIZE-1,0x3100,0x3200,0x3300)==87);
 assert(XCALL(280,0,1,RAM_SIZE-4,0x3410,0)==87);
 process_cleanup();puts("process-check PASS: packet/argv/env, sync yield, async PID, wait/nowait, reap once, exit 259, pre-entry failure and bounds");
}
