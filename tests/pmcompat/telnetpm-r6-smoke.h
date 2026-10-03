/* R6 native Windows coverage: real PM class callbacks, keyboard dispatch,
 * resource exports, GDI enumeration and guest FONTMETRICS buffers. */
#include <stdlib.h>
#include "../../dlls/pmgpi/font_query.h"
typedef DWORD (__cdecl *R6Proc)(DWORD,WORD,DWORD,DWORD);
FN(DWORD,r6_register_class,(DWORD,const char *,R6Proc,DWORD,DWORD));
FN(DWORD,r6_create_window,(DWORD,DWORD,DWORD *,const char *,const char *,DWORD,DWORD,DWORD,DWORD *));
FN(DWORD,r6_destroy_window,(DWORD));
FN(DWORD,r6_dispatch,(DWORD,void *));
FN(DWORD,r6_load_accel,(DWORD,DWORD,DWORD));
FN(DWORD,r6_create_accel,(DWORD,const void *));
FN(DWORD,r6_query_accel,(DWORD,DWORD));
FN(DWORD,r6_set_accel,(DWORD,DWORD,DWORD));
FN(DWORD,r6_copy_accel,(DWORD,void *,DWORD));
FN(DWORD,r6_destroy_accel,(DWORD));
FN(LONG,r6_load_message,(DWORD,DWORD,DWORD,LONG,char *));
FN(LONG,r6_query_fonts,(DWORD,DWORD,const char *,LONG *,LONG,void *));
struct R6Qmsg { DWORD hwnd; WORD msg,reserved; DWORD mp1,mp2,time; LONG x,y; };
static DWORD r6_commands,r6_last_command,r6_last_window,r6_chars;
static DWORD __cdecl r6_window_proc(DWORD window,WORD msg,DWORD mp1,DWORD mp2)
{
    (void)mp2;
    if(msg==0x20) { ++r6_commands;r6_last_command=mp1;r6_last_window=window; }
    if(msg==0x7a) ++r6_chars;
    return 0;
}
static int r6_api_smoke(HMODULE win,HMODULE gpi,DWORD ps)
{
    WORD table1[]={2,437,2,0x20,401,1,'x',402};
    WORD table2[]={1,437,2,0x20,403};
    const unsigned char messages[]={0xb5,1,6,'h','e','l','l','o',0,1,0,4,'b','y','e',0};
    struct R6Qmsg msg; O2FONTMETRICS one,*metrics;
    DWORD h1,h2,explicit_table,window1,window2,client,flags;
    LONG total,count,remaining; unsigned i,fixed=0;
    unsigned char copied[32],saved_keys[256],keys[256],*allocation;
    char text[8]; int checks=0;
    BIND(win,r6_register_class,926);BIND(win,r6_create_window,908);BIND(win,r6_destroy_window,728);
    BIND(win,r6_dispatch,912);BIND(win,r6_load_accel,776);BIND(win,r6_create_accel,713);
    BIND(win,r6_query_accel,798);BIND(win,r6_set_accel,850);BIND(win,r6_copy_accel,709);BIND(win,r6_destroy_accel,723);
    BIND(win,r6_load_message,779);BIND(gpi,r6_query_fonts,586);
    CHECK(r6_query_accel(1,0)==0 && r6_set_accel(1,0,0));
    CHECK(register_resource(0,8,77,table1,sizeof(table1)));
    CHECK(register_resource(0,8,78,table2,sizeof(table2)));
    explicit_table=r6_load_accel(1,0,77);CHECK(explicit_table!=0);
    memset(copied,0xaa,sizeof(copied));
    CHECK(r6_copy_accel(explicit_table,NULL,0)==sizeof(table1));
    CHECK(r6_copy_accel(explicit_table,copied,sizeof(table1))==sizeof(table1) && !memcmp(copied,table1,sizeof(table1)) && copied[sizeof(table1)]==0xaa);
    CHECK(r6_destroy_accel(explicit_table) && !r6_destroy_accel(explicit_table));
    explicit_table=r6_create_accel(1,table1);CHECK(explicit_table!=0);
    CHECK(!r6_load_accel(1,0,999));
    CHECK(r6_register_class(1,"OS2HOST32_R6_ACCEL_SMOKE",r6_window_proc,0,0));
    flags=0x8001; /* FCF_ACCELTABLE | FCF_TITLEBAR, hidden until message loop */
    window1=r6_create_window(1,0,&flags,"OS2HOST32_R6_ACCEL_SMOKE","R6 one",0,0,77,&client);
    CHECK(window1!=0 && client==window1);
    window2=r6_create_window(1,0,&flags,"OS2HOST32_R6_ACCEL_SMOKE","R6 two",0,0,78,&client);
    CHECK(window2!=0 && window2!=window1);
    h1=r6_query_accel(1,window1);h2=r6_query_accel(1,window2);CHECK(h1 && h2 && h1!=h2);
    CHECK(GetKeyboardState(saved_keys));memset(keys,0,sizeof(keys));CHECK(SetKeyboardState(keys));
    memset(&msg,0,sizeof(msg));msg.hwnd=window1;msg.msg=WM_KEYDOWN;msg.mp1=VK_F1;msg.mp2=0x003b0001;
    r6_dispatch(1,&msg);CHECK(r6_commands==1 && r6_last_command==401 && r6_last_window==window1 && r6_chars==0);
    msg.hwnd=window2;r6_dispatch(1,&msg);CHECK(r6_commands==2 && r6_last_command==403 && r6_last_window==window2);
    CHECK(r6_set_accel(1,0,window2) && !r6_query_accel(1,window2));
    r6_dispatch(1,&msg);CHECK(r6_commands==2 && r6_chars==1);
    CHECK(r6_set_accel(1,explicit_table,0));r6_dispatch(1,&msg);CHECK(r6_commands==3 && r6_last_command==401);
    CHECK(r6_set_accel(1,0,0));msg.hwnd=window1;msg.msg=WM_CHAR;msg.mp1='x';msg.mp2=0x002d0001;
    r6_dispatch(1,&msg);CHECK(r6_commands==4 && r6_last_command==402);
    CHECK(SetKeyboardState(saved_keys));
    CHECK(r6_destroy_window(window1) && r6_destroy_window(window2));
    CHECK(!r6_copy_accel(h1,NULL,0) && !r6_copy_accel(h2,NULL,0));
    CHECK(r6_destroy_accel(explicit_table));
    CHECK(register_resource(0,10,11,messages,sizeof(messages)));
    memset(text,'?',sizeof(text));CHECK(r6_load_message(1,0,160,6,text)==5 && !strcmp(text,"hello") && text[6]=='?');
    CHECK(r6_load_message(1,1,162,3,text)==2 && !strcmp(text,"by"));
    CHECK(!r6_load_message(1,0,161,8,text) && !text[0]);
    CHECK(!r6_load_message(1,0,999,8,text) && !text[0]);
    count=0;total=r6_query_fonts(ps,1,NULL,&count,0,NULL);CHECK(total>0 && count==0);
    count=1;remaining=r6_query_fonts(ps,1,NULL,&count,sizeof(one),&one);
    CHECK(remaining==total-1 && count==1 && one.szFacename[0] && one.lAveCharWidth>0);
    allocation=(unsigned char *)malloc((size_t)total*sizeof(*metrics)+8);CHECK(allocation!=NULL);
    memset(allocation,0xa5,(size_t)total*sizeof(*metrics)+8);metrics=(O2FONTMETRICS *)(allocation+4);
    count=total;remaining=r6_query_fonts(ps,1,NULL,&count,sizeof(*metrics),metrics);
    CHECK(remaining==0 && count==total && allocation[0]==0xa5 && allocation[4+(size_t)total*sizeof(*metrics)]==0xa5);
    for(i=0;i<(unsigned)count;++i) if((metrics[i].fsType&1) && !metrics[i].fsSelection) ++fixed;
    printf("GpiQueryFonts: %ld records, %u regular fixed-pitch fonts, stride=%u\n",(long)count,fixed,(unsigned)sizeof(*metrics));
    CHECK(fixed>0);free(allocation);
    count=1;CHECK(r6_query_fonts(ps,1,"__OS2HOST32_NO_SUCH_FONT__",&count,sizeof(one),&one)==0 && count==0);
    count=1;CHECK(r6_query_fonts(ps,1,NULL,&count,0,&one)==-1);
    printf("R6 accelerator/message/font API smoke: %d checks PASS\n",checks);return 0;
}
