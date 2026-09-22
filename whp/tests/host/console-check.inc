static uint32_t ccall(uint32_t id,const uint32_t *args)
{
 uint32_t i,at=0x1004;
 for(i=0;i<far16_apis[id-1].count;i++) {
  if(far16_apis[id-1].width[i]==2)file_put16(rt.ram+at,(uint16_t)args[i]);else guest_put_u32(&rt,at,args[i]);
  at+=far16_apis[id-1].width[i];
 }
 return dispatch_console(&rt,id,0x1000);
}
#define CCALL(id,...) ccall(id,(uint32_t[]){__VA_ARGS__})
static void test_console(void)
{
 DWORD timeout;uint32_t token;int ready;
 reset();assert(far16_desc("KBDCALLS",4)==5&&far16_desc("KBDCALLS",99)==-1);
 /* DOSCALLS.425 must be RET, not a veneer that overwrites the EAX pointer. */
 token=hostcall_stub(&rt,HC_DOSCALLS,425);assert(rt.ram[token]==0xc3);
 assert(CCALL(2,0,0x2200,0x2202)==0);
 assert(rd16(rt.ram+0x2200)==3&&rd16(rt.ram+0x2202)==2);
 assert(CCALL(3,0,79,24)==0&&mock_cursor.X==84&&mock_cursor.Y==34);
 assert(CCALL(3,0,80,0)==87);
 memset(rt.ram+0x2300,0xa5,16);file_put16(rt.ram+0x2300,12);
 assert(CCALL(5,0,0x2300)==0&&rd16(rt.ram+0x2304)==80&&rd16(rt.ram+0x2306)==25&&rt.ram[0x230c]==0xa5);
 file_put16(rt.ram+0x2300,10);assert(CCALL(5,0,0x2300)==87);
 assert(CCALL(4,0,3,0x2400)==0&&mock_written==3);
 assert(CCALL(4,0,3,RAM_SIZE-1)==87);
 rt.ram[0x2500]=' ';rt.ram[0x2501]=7;mock_fill=0;
 assert(CCALL(1,0,0x2500,0xffff,0xffff,0xffff,0,0)==0&&mock_fill==2000);
 assert(CCALL(6,0,1,0x2600)==0&&rt.ram[0x2602]==0);
 spawn();assert(CCALL(6,0,0,0x2600)==0&&rt.threads[0].state==THREAD_WAIT_KBD);
 park(0);assert(next_runnable_thread(&rt)==1);select_thread(1);
 assert(next_wait_timeout(&rt,&timeout)&&timeout==10);
 clock_ms+=10;wake_expired_waiters(&rt);assert(rt.threads[0].state==THREAD_WAIT_KBD);
 memset(&mock_key,0,sizeof(mock_key));mock_key.EventType=KEY_EVENT;
 mock_key.Event.KeyEvent.bKeyDown=1;mock_key.Event.KeyEvent.wRepeatCount=2;
 mock_key.Event.KeyEvent.wVirtualScanCode=0x48;mock_key.Event.KeyEvent.dwControlKeyState=SHIFT_PRESSED;
 mock_key_pending=1;clock_ms+=10;assert(wake_expired_waiters(&rt));
 assert(rt.threads[0].state==THREAD_RUNNABLE&&rt.ram[0x2601]==0x48&&rt.ram[0x2602]==0x40&&rd16(rt.ram+0x2604)==3);
 assert(console_key_read(&rt,0x2600,&ready)==0&&ready&&console_repeat==0);
 console_repeat=2;assert(CCALL(7,0)==0&&console_repeat==0);
 mock_console_available=0;assert(CCALL(6,0,0,0x2600)==6);mock_console_available=1;
 console_cleanup();assert(mock_console_mode==7);
 puts("console-check PASS: packed frames, pointer token, viewport/cursor, mode bounds, TTY, clear, key wait/yield/repeat/flush and mode restoration");
}
