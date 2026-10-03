/* Native Win32 smoke test. Uses our DLL exports by ordinal, no guest EXE. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "../../dlls/pmshapi/switchlist.h"

#define FN(ret,name,args) typedef ret (__cdecl *name##Fn) args; static name##Fn name
FN(DWORD,open_profile,(DWORD,const char *));
FN(BOOL,close_profile,(DWORD));
FN(BOOL,profile_size,(DWORD,const char *,const char *,DWORD *));
FN(BOOL,write_data,(DWORD,const char *,const char *,const void *,DWORD));
FN(BOOL,read_data,(DWORD,const char *,const char *,void *,DWORD *));
FN(DWORD,set_rect,(DWORD,void *,LONG,LONG,LONG,LONG));
FN(DWORD,last_error,(DWORD));
FN(LONG,text_length,(DWORD));
FN(DWORD,validate_rect,(DWORD,const void *,DWORD));
FN(DWORD,invalidate_region,(DWORD,DWORD,DWORD));
FN(DWORD,create_cursor,(DWORD,LONG,LONG,LONG,LONG,DWORD,const void *));
FN(DWORD,query_cursor,(DWORD,void *));
FN(DWORD,show_cursor,(DWORD,DWORD));
FN(DWORD,destroy_cursor,(DWORD));
FN(DWORD,create_ps,(DWORD,DWORD,void *,DWORD));
FN(LONG,destroy_ps,(DWORD));
FN(LONG,create_font,(DWORD,const char *,LONG,const void *));
FN(LONG,set_font,(DWORD,LONG));
FN(LONG,delete_font,(DWORD,LONG));
FN(LONG,back_color,(DWORD,LONG));
FN(DWORD,get_resource,(DWORD,DWORD,DWORD,void **));
FN(DWORD,free_resource,(void *));
FN(int,register_resource,(DWORD,WORD,WORD,const void *,DWORD));
FN(DWORD,add_switch,(const O2SwitchControl *));
FN(DWORD,change_switch,(DWORD,const O2SwitchControl *));
FN(DWORD,query_switch,(DWORD,O2SwitchControl *));
FN(DWORD,switch_handle,(DWORD,DWORD));
FN(DWORD,remove_switch,(DWORD));

struct O2Rect { LONG left,bottom,right,top; };
struct O2Cursor { DWORD hwnd; LONG x,y,cx,cy; DWORD flags; struct O2Rect clip; };
struct O2Font { WORD length,selection; LONG match; char face[32]; WORD registry,cp;
                LONG height,width; WORD type,use; };
#define BIND(mod,var,ord) do { FARPROC p=GetProcAddress(mod,(LPCSTR)(ULONG_PTR)ord); \
    if(!p) { printf("MISSING ordinal %u\n",(unsigned)ord); return 1; } memcpy(&var,&p,sizeof(var)); } while(0)
#define CHECK(c) do { ++checks; if(!(c)) { printf("FAIL line %d: %s (Win32 error %lu)\n",__LINE__,#c,(unsigned long)GetLastError()); return 1; } } while(0)
#include "telnetpm-r6-smoke.h"
int main(void)
{
    HMODULE sh,win,gpi,dos;
    DWORD hini,size,ps,hsw,old_hsw;
    unsigned i;
    HWND hwnd;
    HDC dc;
    struct O2Rect rect;
    struct O2Cursor cursor;
    struct O2Font font;
    struct { DWORD before; O2SwitchControl data; DWORD after; } switch_data;
    char dir[MAX_PATH],file[MAX_PATH];
    unsigned char bytes[4]={0,255,1,0},out[4];
    void *resource,*other;
    FARPROC proc;
    int checks=0;
    sh=LoadLibraryA("PMSHAPI.dll"); win=LoadLibraryA("PMWIN.dll");
    gpi=LoadLibraryA("PMGPI.dll"); dos=LoadLibraryA("DOSCALLS.dll");
    CHECK(sh && win && gpi && dos);
    BIND(sh,open_profile,102); BIND(sh,close_profile,103); BIND(sh,profile_size,101);
    BIND(sh,write_data,118); BIND(sh,read_data,117);
    BIND(sh,add_switch,120); BIND(sh,change_switch,123); BIND(sh,query_switch,124);
    BIND(sh,switch_handle,125); BIND(sh,remove_switch,129);
    BIND(win,set_rect,868); BIND(win,last_error,753); BIND(win,text_length,842);
    BIND(win,validate_rect,895); BIND(win,invalidate_region,766);
    BIND(win,create_cursor,715); BIND(win,query_cursor,812);
    BIND(win,show_cursor,880); BIND(win,destroy_cursor,725);
    BIND(gpi,create_ps,369); BIND(gpi,destroy_ps,379); BIND(gpi,create_font,368);
    BIND(gpi,set_font,513); BIND(gpi,delete_font,378); BIND(gpi,back_color,504);
    BIND(dos,get_resource,352); BIND(dos,free_resource,353);
    proc=GetProcAddress(win,"OS2PM_RegisterResource"); CHECK(proc!=NULL);
    memcpy(&register_resource,&proc,sizeof(register_resource));

    CHECK(GetTempPathA(MAX_PATH,dir)>0);
    CHECK(GetTempFileNameA(dir,"tpm",0,file)!=0);
    hini=open_profile(1,file); CHECK(hini!=0);
    CHECK(write_data(hini,"TELNETPM_TEST","binary",bytes,4));
    CHECK(profile_size(hini,"TELNETPM_TEST","binary",&size) && size==4);
    size=4; CHECK(read_data(hini,"TELNETPM_TEST","binary",out,&size) && !memcmp(out,bytes,4));
    CHECK(close_profile(hini)); CHECK(!close_profile(hini));
    CHECK(!write_data(hini,"TELNETPM_TEST","binary",bytes,4));
    /* More than the old 32-slot limit must succeed once handles are closed. */
    for(i=0;i<64;++i) { hini=open_profile(1,file); CHECK(hini && close_profile(hini)); }
    DeleteFileA(file);

    CHECK(set_rect(1,&rect,2,3,7,11));
    CHECK(rect.left==2 && rect.bottom==3 && rect.right==7 && rect.top==11);
    CHECK(!set_rect(1,NULL,0,0,0,0)); CHECK(last_error(1)==0x1208); CHECK(last_error(1)==0);
    hwnd=CreateWindowA("STATIC","TELNETPM API test",WS_OVERLAPPEDWINDOW,
                       0,0,200,120,NULL,NULL,GetModuleHandleA(NULL),NULL);
    CHECK(hwnd!=NULL); CHECK(text_length((DWORD)(ULONG_PTR)hwnd)==17);
    switch_data.before=0x12345678; switch_data.after=0xabcdef01;
    CHECK(sizeof(O2SwitchControl)==96 && offsetof(O2SwitchControl,szSwtitle)==28);
    hsw=switch_handle((DWORD)(ULONG_PTR)hwnd,0); CHECK(hsw!=0);
    CHECK(switch_handle((DWORD)(ULONG_PTR)hwnd,GetCurrentProcessId())==hsw);
    CHECK(!query_switch(hsw,&switch_data.data));
    CHECK(switch_data.before==0x12345678 && switch_data.after==0xabcdef01);
    CHECK(switch_data.data.hwnd==(DWORD)(ULONG_PTR)hwnd && switch_data.data.idProcess==GetCurrentProcessId());
    CHECK(!strcmp(switch_data.data.szSwtitle,"TELNETPM API test"));
    strcpy(switch_data.data.szSwtitle,"TELNETPM session"); switch_data.data.uchVisibility=1;
    CHECK(!change_switch(hsw,&switch_data.data)); memset(&switch_data.data,0,sizeof(switch_data.data));
    CHECK(!query_switch(hsw,&switch_data.data) && !strcmp(switch_data.data.szSwtitle,"TELNETPM session") && switch_data.data.uchVisibility==1);
    CHECK(!remove_switch(hsw) && !switch_handle((DWORD)(ULONG_PTR)hwnd,0));
    CHECK(query_switch(hsw,&switch_data.data)==0x1202 && remove_switch(hsw)==0x1202);
    old_hsw=hsw; hsw=add_switch(&switch_data.data); CHECK(hsw && hsw!=old_hsw);
    puts("PMSHAPI switch-list ordinal, layout, query/change/remove checks PASS");
    CHECK(invalidate_region((DWORD)(ULONG_PTR)hwnd,0,0));
    CHECK(validate_rect((DWORD)(ULONG_PTR)hwnd,NULL,0));
    CHECK(sizeof(cursor)==40 && sizeof(font)==56);
    CHECK(create_cursor((DWORD)(ULONG_PTR)hwnd,3,4,2,8,4,NULL));
    CHECK(query_cursor(1,&cursor) && cursor.x==3 && cursor.y==4 && cursor.cy==8);
    CHECK(show_cursor((DWORD)(ULONG_PTR)hwnd,1)); CHECK(show_cursor((DWORD)(ULONG_PTR)hwnd,1));
    CHECK(show_cursor((DWORD)(ULONG_PTR)hwnd,0)); CHECK(show_cursor((DWORD)(ULONG_PTR)hwnd,0));
    CHECK(create_cursor((DWORD)(ULONG_PTR)hwnd,5,6,0,0,0x8000,NULL));
    CHECK(query_cursor(1,&cursor) && cursor.x==5 && cursor.y==6 && cursor.cy==8);
    CHECK(destroy_cursor((DWORD)(ULONG_PTR)hwnd)); CHECK(!query_cursor(1,&cursor));

    dc=GetDC(hwnd); CHECK(dc!=NULL);
    ps=create_ps(1,(DWORD)(ULONG_PTR)dc,NULL,0); CHECK(ps!=0);
    CHECK(r6_api_smoke(win,gpi,ps)==0);
    memset(&font,0,sizeof(font)); font.length=sizeof(font); font.height=16;
    strcpy(font.face,"Courier New"); font.type=1;
    CHECK(create_font(ps,NULL,1,&font)!=0); CHECK(set_font(ps,1));
    CHECK(!set_font(ps,99)); CHECK(!delete_font(ps,1));
    CHECK(back_color(ps,-2)); CHECK(GetBkColor(dc)==RGB(255,255,255));
    CHECK(set_font(ps,0)); CHECK(delete_font(ps,1)); CHECK(!set_font(ps,1));
    CHECK(create_font(ps,NULL,1,&font)!=0); CHECK(set_font(ps,1));
    CHECK(destroy_ps(ps)); ReleaseDC(hwnd,dc); DestroyWindow(hwnd);
    CHECK(query_switch(hsw,&switch_data.data)==0x1202);

    CHECK(register_resource(0,10,42,bytes,4));
    CHECK(get_resource(1,10,42,&resource)==0 && resource!=bytes && !memcmp(resource,bytes,4));
    CHECK(get_resource(0,10,42,&other)==0 && other!=resource);
    CHECK(free_resource(resource)==0); CHECK(free_resource(resource)!=0);
    CHECK(!memcmp(other,bytes,4)); CHECK(free_resource(other)==0);
    resource=(void *)1; CHECK(get_resource(0,10,43,&resource)!=0 && !resource);
    printf("TELNETPM API smoke: %d checks PASS\n",checks);
    return 0;
}
