static void test_sarien(void)
{
 uint32_t dc,ps,screen,bm,i,hab2,at;
 assert(PCALL(763,0)==PM_HAB && PCALL(763,0)==PM_HAB);
 assert(PCALL(716,PM_HAB,0)==PM_HMQ);strcpy((char *)rt.ram+0x900,"SARTEST");strcpy((char *)rt.ram+0xa00,"Sarien test");
 assert(PCALL(926,PM_HAB,0x900,0x12000,4,0)==1);guest_put_u32(&rt,0xb00,0x813);cpu[1].Reg64=0x27000;
 assert(PCALL(908,1,0x80000000,0xb00,0x900,0xa00,0x80000000,0,0,0xb04)==0 && pm_entered);
 assert(PCALL(794,PM_CLIENT)==PM_WINDOW_DC);
 guest_put_u32(&rt,0x1000,0);guest_put_u32(&rt,0x1004,0);
 screen=GCALL(369,PM_HAB,PM_WINDOW_DC,0x1000,0x5008);assert(screen);
 dc=GCALL(610,PM_HAB,8,0,0,0,0);ps=GCALL(369,PM_HAB,dc,0x1000,0x5008);assert(ps);
 info64(0x2000,64,32,8);bm=GCALL(598,ps,0x2000,0,0,0);assert(bm && GCALL(506,ps,bm)==0);
 for(i=0;i<33;i++)guest_put_u32(&rt,0x3000+i*4,i==0?0:i==32?0xffffff:0x55ffff);
 assert(GCALL(592,ps,4,2,0,33,0x3000)==1);
 for(i=0;i<33;i++) {
  guest_put_u32(&rt,0x1100,i);guest_put_u32(&rt,0x1104,0);
  assert(GCALL(517,ps,i)==1 && GCALL(544,ps,0x1100)==1);
 }
 memset(rt.ram+0x4000,0xa5,20);guest_put_u32(&rt,0x4000,16);
 assert(GCALL(601,bm,0x4000)==1 && guest_u32(&rt,0x4010)==0xa5a5a5a5);
 assert(guest_u32(&rt,0x4000)==16 && guest_u32(&rt,0x4004)==64);
 assert(GCALL(599,ps,0,30,0x5000,0x4000)==30);
 assert(rt.ram[0x5000]==0 && gpi_bm(&rt,bm)->rgb[rt.ram[0x5001]]==0x55ffff);
 assert(gpi_bm(&rt,bm)->rgb[rt.ram[0x5020]]==0xffffff);
 /* Header remains intact when the palette starts at offset 16. */
 assert(GCALL(602,ps,0,30,0x5000,0x4000)==30);
 pm_return(0);
 assert(PCALL(829,1,21)==1080 && PCALL(829,1,30)==23);
 assert(PCALL(875,PM_FRAME,0,60,820,320,221,3)==1);
 assert(mock_pos_x==60 && mock_pos_y==39 && mock_pos_w==320 && mock_pos_h==221);
 /* A 320x200 drawing must fit after the native nonclient area is deducted. */
 setenv("WHP_OS2_SARIEN_GEOMETRY","1",1);
 assert(PCALL(875,PM_FRAME,0,60,820,320,221,3)==1);
 assert(mock_pos_w-8==320 && mock_pos_h-31==200);
 assert(mock_pos_y==1080-820-mock_pos_h);
 assert(PCALL(875,PM_FRAME,0,0,0,320,221,1)==1);
 assert(mock_pos_w==328 && mock_pos_h==231);
 /* Move-only ignores the supplied dimensions; client handles stay unaffected. */
 assert(PCALL(875,PM_FRAME,0,60,820,0,0,2)==1);
 assert(PCALL(875,PM_CLIENT,0,60,820,320,221,3)==1 && mock_pos_w==320 && mock_pos_h==221);
 mock_adjust_fail=1;assert(PCALL(875,PM_FRAME,0,0,0,320,221,1)==0);mock_adjust_fail=0;
 assert(PCALL(875,PM_FRAME,0,0,0,320,1,1)==0);
 unsetenv("WHP_OS2_SARIEN_GEOMETRY");
 assert(PCALL(875,PM_FRAME,0,60,820,320,221,3)==1 && mock_pos_w==320 && mock_pos_h==221);
 assert(PCALL(746,1,PM_CLIENT,0)==1 && GetFocus()==mock_hwnd);
 for(i=0;i<6;i++)guest_put_u32(&rt,0x7000+i*4,i==2?64:i==3?32:0);
 assert(PCALL(703,PM_CLIENT,screen,0)==screen);
 assert(GCALL(355,screen,ps,3,0x7000,0xcc,1)==1);
 assert(PCALL(738,screen)==1);
 /* Worker thread uses shared PS without main being in BeginPaint. */
 spawn();park(0);select_thread(1);hab2=PCALL(763,0);assert(hab2 && hab2!=PM_HAB && PCALL(763,0)==hab2);
 assert(GCALL(355,screen,ps,3,0x7000,0xcc,1)==1 && !rt.pm->paint_dc);
 assert(PCALL(919,PM_CLIENT,0x1000,123,0)==1);
 assert(PCALL(888,hab2)==1 && rt.pm);
 at=(uint32_t)clock_ms;assert(guest_beep(&rt,880,40)==0);
 assert(clock_ms==at && mock_beep_frequency==880 && mock_beep_duration==40);
 assert(current_guest_thread(&rt)->state==THREAD_SLEEP);park(0);select_thread(0);
 assert(rt.threads[1].state==THREAD_SLEEP);
 assert(PCALL(915,PM_HAB,0xc00,0,0,0)==1 && guest_u32(&rt,0xc08)==123);
 pm_native_proc(mock_hwnd,WM_KEYDOWN,VK_LEFT,1);
 assert(PCALL(915,PM_HAB,0xc00,0,0,0)==1 && guest_u32(&rt,0xc0c)==0x150000 && (guest_u32(&rt,0xc08)&2));
 pm_native_proc(mock_hwnd,WM_KEYUP,VK_LEFT,1);
 assert(PCALL(915,PM_HAB,0xc00,0,0,0)==1 && (guest_u32(&rt,0xc08)&0x40));
 pm_native_proc(mock_hwnd,WM_CHAR,27,1);
 assert(PCALL(915,PM_HAB,0xc00,0,0,0)==1 && guest_u32(&rt,0xc0c)==0xf001b);
 pm_native_proc(mock_hwnd,WM_CHAR,'Y',1);
 assert(PCALL(915,PM_HAB,0xc00,0,0,0)==1 && guest_u32(&rt,0xc0c)=='Y');
 assert(guest_beep(&rt,36,1)==87 && guest_beep(&rt,880,60001)==87);
 clock_ms+=40;wake_expired_waiters(&rt);assert(rt.threads[1].state==THREAD_RUNNABLE);
 select_thread(1);finish_guest_thread(&rt);select_thread(0);
 assert(PCALL(728,PM_FRAME)==0 && pm_entered);pm_return(0);
 assert(PCALL(726,PM_HMQ)==1 && PCALL(888,PM_HAB)==1 && !rt.gpi);
 puts("sarien-check PASS: logical colors, 16-byte header, shared window PS, supplied paint PS, keys, beep scheduling");
}
