/* Automates only this process's own dialogs; no user files are selected. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "os2_track.h"
struct FileDlg {
    uint32_t size,flags,user;int32_t result,error;
    uint32_t title,ok,proc,type,types,drive,drives,module;
    char path[260];uint32_t paths,count;uint16_t id;int16_t x,y,ea;
};
typedef uint32_t (__cdecl *TrackFn)(uint32_t,uint32_t,struct O2TrackInfo *);
typedef uint32_t (__cdecl *FileFn)(uint32_t,uint32_t,struct FileDlg *);
static HANDLE finished;static DWORD ui_thread;static LONG cancel_count;
static BOOL CALLBACK cancel_dialog(HWND h,LPARAM arg)
{
    char name[32];(void)arg;
    if(GetClassNameA(h,name,sizeof(name)) && !strcmp(name,"#32770") && IsWindowVisible(h)) {
        InterlockedIncrement(&cancel_count);PostMessageA(h,WM_COMMAND,IDCANCEL,0);
    }
    return TRUE;
}
static DWORD WINAPI canceller(void *arg)
{
    DWORD start=GetTickCount();(void)arg;
    while(WaitForSingleObject(finished,50)==WAIT_TIMEOUT) {
        EnumThreadWindows(ui_thread,cancel_dialog,0);
        if(GetTickCount()-start>15000) {
            puts("FAIL: own-process PM smoke timed out");fflush(stdout);ExitProcess(2);
        }
    }
    return 0;
}
#define CHECK(c) do { ++checks;if(!(c)) { printf("FAIL line %d: %s Win32=%lu\n",__LINE__,#c,(unsigned long)GetLastError());return 1; } } while(0)
int main(void)
{
    HMODULE pm,ctls;FARPROC p;TrackFn track;FileFn filedlg;HWND w;HANDLE worker;
    struct O2TrackInfo t;struct O2TrackRect before;struct FileDlg f;unsigned checks=0;DWORD result;
    char cwd[MAX_PATH],after[MAX_PATH];
    CHECK(sizeof(t)==76 && sizeof(f)==328);
    pm=LoadLibraryA("PMWIN.dll");ctls=LoadLibraryA("PMCTLS.dll");CHECK(pm && ctls);
    p=GetProcAddress(pm,(LPCSTR)(uintptr_t)890);CHECK(p!=NULL);memcpy(&track,&p,sizeof(track));
    p=GetProcAddress(ctls,(LPCSTR)(uintptr_t)4);CHECK(p!=NULL);memcpy(&filedlg,&p,sizeof(filedlg));
    w=CreateWindowExA(0,"STATIC","R11 PM smoke",WS_OVERLAPPEDWINDOW|WS_VISIBLE,80,80,360,240,NULL,NULL,NULL,NULL);CHECK(w!=NULL);
    memset(&t,0,sizeof(t));t.border_x=t.border_y=1;t.key_x=8;t.key_y=9;
    t.rect.left=t.rect.bottom=20;t.rect.right=t.rect.top=60;t.min_x=t.min_y=1;t.max_x=t.max_y=1000;t.flags=15;
    CHECK(PostMessageA(w,WM_KEYDOWN,VK_RIGHT,0) && PostMessageA(w,WM_KEYDOWN,VK_UP,0) && PostMessageA(w,WM_KEYDOWN,VK_RETURN,0));
    CHECK(track((uint32_t)(uintptr_t)w,0,&t)==1);
    CHECK(t.rect.left==28 && t.rect.bottom==29 && t.rect.right==68 && t.rect.top==69);
    before=t.rect;CHECK(PostMessageA(w,WM_KEYDOWN,VK_LEFT,0) && PostMessageA(w,WM_KEYDOWN,VK_ESCAPE,0));
    CHECK(track((uint32_t)(uintptr_t)w,0,&t)==0 && !memcmp(&before,&t.rect,sizeof(before)));
    CHECK(GetCapture()==NULL);t.flags=0x10f;CHECK(track((uint32_t)(uintptr_t)w,0,&t)==0);
    puts("WinTrackRect ordinal, keyboard move, acceptance and cancellation PASS");
    memset(&f,0,sizeof(f));f.size=sizeof(f);f.flags=0x111;
    CHECK(filedlg(1,(uint32_t)(uintptr_t)w,&f)==0 && f.error==3);
    CHECK(GetCurrentDirectoryA(sizeof(cwd),cwd)>0);
    ui_thread=GetCurrentThreadId();finished=CreateEventA(NULL,TRUE,FALSE,NULL);CHECK(finished!=NULL);
    worker=CreateThread(NULL,0,canceller,NULL,0,NULL);CHECK(worker!=NULL);
    f.flags=0x101;strcpy(f.path,"*.*");result=filedlg(1,(uint32_t)(uintptr_t)w,&f);
    SetEvent(finished);CHECK(WaitForSingleObject(worker,2000)==WAIT_OBJECT_0);CloseHandle(worker);CloseHandle(finished);
    CHECK(cancel_count>0 && result!=0 && f.result==2 && f.error==0 && f.count==0 && f.paths==0);
    CHECK(GetCurrentDirectoryA(sizeof(after),after)>0 && !strcmp(cwd,after));
    CHECK(DestroyWindow(w));puts("WinFileDlg ordinal, unsupported flags, modal cancellation and CWD preservation PASS");
    printf("R11 native PM platform: %u checks PASS\n",checks);return 0;
}
