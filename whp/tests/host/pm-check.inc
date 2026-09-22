static int pm_entered;
static uint32_t pcall(uint32_t ordinal,const uint32_t *args,size_t count)
{
 size_t i;
 for(i=0;i<count;i++)guest_put_u32(&rt,0x804+(uint32_t)i*4,args[i]);
 pm_entered=0;
 return dispatch_pm(&rt,ordinal,0x800,0x11002,&pm_entered);
}
#define PCALL(n,...) pcall(n,(const uint32_t[]){__VA_ARGS__},sizeof((const uint32_t[]){__VA_ARGS__})/4)
static void pm_return(uint32_t value)
{
 uint32_t sp=current_guest_thread(&rt)->callbacks->entry_sp;
 guest_put_u32(&rt,sp,value);
 assert(return_guest_callback(&rt,sp,GUEST_CALLBACK_RETURN_STUB+6)==0);
}
static void test_pm(void)
{
 uint32_t i;
 RECT rect;
 cpu[1].Reg64=0x27000;
 assert(PCALL(763,1)==0);
 assert(PCALL(763,0)==PM_HAB);
 assert(PCALL(716,PM_HAB,0)==PM_HMQ);
 strcpy((char *)rt.ram+0x900,"PMTEST");
 strcpy((char *)rt.ram+0xa00,"Test title");
 assert(PCALL(926,PM_HAB,0x900,0x12000,4,0)==1);
 guest_put_u32(&rt,0xb00,0xc3b);
 assert(PCALL(908,1,0x80000000,0xb00,0x900,0xa00,0,0,0,0xb04)==0);
 assert(pm_entered && current_guest_thread(&rt)->callback_depth==1);
 assert(guest_u32(&rt,0xb04)==PM_CLIENT && !mock_shows);
 assert(guest_u32(&rt,(uint32_t)cpu[1].Reg64+8)==1); /* WM_CREATE */
 assert(PCALL(728,PM_FRAME)==0 && !pm_entered); /* cannot destroy mid-create */
 assert(PCALL(920,PM_CLIENT,0x1000,17,25)==0 && pm_entered);
 assert(current_guest_thread(&rt)->callback_depth==2);
 pm_return(42);assert(cpu[3].Reg64==42);
 pm_return(0);assert(cpu[3].Reg64==PM_FRAME && mock_shows==1);
 /* Exact legacy QMSG width and zeroed USHORT padding, followed by guards. */
 memset(rt.ram+0xc00,0xcc,32);
 assert(PCALL(919,PM_CLIENT,0x7a,1,65)==1);
 assert(PCALL(915,PM_HAB,0xc00,0,0,0)==1);
 assert(guest_u32(&rt,0xc00)==PM_CLIENT && guest_u32(&rt,0xc04)==0x7a);
 assert(guest_u32(&rt,0xc08)==1 && guest_u32(&rt,0xc0c)==65);
 assert(guest_u32(&rt,0xc14)==0 && guest_u32(&rt,0xc1c)==0xcccccccc);
 assert(PCALL(912,PM_HAB,0xc00)==0 && pm_entered);
 pm_return(0x12345678);assert(cpu[3].Reg64==0x12345678);
 assert(PCALL(915,PM_HAB,RAM_SIZE-27,0,0,0)==0);
 assert(current_guest_thread(&rt)->state==THREAD_RUNNING);
 /* Native translation never enters guest code; bounded paint coalescing. */
 pm_native_proc(mock_hwnd,WM_CHAR,'Z',0);
 assert(!current_guest_thread(&rt)->callbacks);
 assert(PCALL(915,PM_HAB,0xc00,0,0,0)==1 && guest_u32(&rt,0xc0c)=='Z');
 for(i=0;i<200;i++)pm_native_proc(mock_hwnd,WM_PAINT,0,0);
 assert(rt.pm->count==1);
 assert(PCALL(915,PM_HAB,0xc00,0,0,0)==1 && guest_u32(&rt,0xc04)==0x23);
 /* Idle GetMsg wakes through the native message wait, not a deadlock. */
 assert(PCALL(915,PM_HAB,0xc00,0,0,0)==0);
 assert(pm_waiting(&rt));park(0);
 assert(schedule_next_thread(&rt)==0 && mock_waits==1);
 assert(cpu[3].Reg64==1 && guest_u32(&rt,0xc04)==0x29);
 /* Sleeping guest peer runs while PM owner is blocked. */
 spawn();select_thread(1);assert(guest_sleep(&rt,25)==0);park(0);select_thread(0);
 assert(PCALL(915,PM_HAB,0xc00,0,0,0)==0);park(0);
 assert(schedule_next_thread(&rt)==0 && rt.current_thread==1 && pm_waiting(&rt));
 pm_native_proc(mock_hwnd,WM_CHAR,'A',0);pm_pump(&rt);
 assert(rt.threads[0].state==THREAD_RUNNABLE && rt.threads[0].regs[3].Reg64==1);
 finish_guest_thread(&rt);select_thread(0);
 /* Paint and bottom-left rectangle conversion, invalid pointers and HPS. */
 assert(PCALL(703,PM_CLIENT,0,0xd00)==PM_HPS);
 assert(PCALL(703,PM_CLIENT,0,0xd00)==0);
 assert(PCALL(840,PM_CLIENT,0xd00)==1);
 guest_put_u32(&rt,0xd00,10);guest_put_u32(&rt,0xd04,20);
 guest_put_u32(&rt,0xd08,100);guest_put_u32(&rt,0xd0c,80);
 assert(pm_rect(&rt,0xd00,&rect) && rect.top==280 && rect.bottom==340);
 assert(PCALL(743,PM_HPS,0xd00,0xfffffffe)==1);
 assert(PCALL(913,PM_HPS,0xffff,0xa00,0xd00,0xffffffff,0xfffffffe,0x200)==10);
 assert(PCALL(913,PM_HPS,0xfffe,0xa00,0xd00,0,0,0)==0);
 assert(PCALL(913,PM_HPS,0xffff,0xa00,0xd00,0,0,1)==0);
 assert(PCALL(738,PM_HPS)==1 && !rt.pm->paint_dc);
 assert(PCALL(765,PM_CLIENT,0,0)==1);pm_pump(&rt);
 assert(PCALL(915,PM_HAB,0xc00,0,0,0)==1);
 /* Full queue refuses posts without corrupting pending messages. */
 for(i=0;i<PM_CAPACITY;i++)assert(PCALL(919,PM_CLIENT,0x1000,i,0)==1);
 assert(PCALL(919,PM_CLIENT,0x1000,999,0)==0);
 for(i=0;i<PM_CAPACITY;i++) {
   assert(PCALL(915,PM_HAB,0xc00,0,0,0)==1);
   assert(guest_u32(&rt,0xc08)==i);
 }
 assert(PCALL(911,PM_CLIENT,0x29,0,0)==1);
 assert(PCALL(915,PM_HAB,0xc00,0,0,0)==0 && guest_u32(&rt,0xc04)==0x2a);
 assert(PCALL(728,PM_FRAME)==0 && pm_entered);pm_return(0);
 assert(cpu[3].Reg64==1 && !mock_live && !rt.pm->window);
 /* Guest WM_CREATE rejection unwinds its frame and clears client output. */
 assert(PCALL(908,1,0x80000000,0xb00,0x900,0xa00,0,0,0,0xb04)==0 && pm_entered);
 pm_return(1);assert(cpu[3].Reg64==0 && guest_u32(&rt,0xb04)==0 && !mock_live);
 assert(PCALL(726,PM_HMQ)==1);
 assert(PCALL(888,PM_HAB)==1 && !rt.pm);
 puts("pm-check PASS: guest callbacks, QMSG ABI, native queue wake, guest sleep, paint, cleanup");
}
