/* Microsoft C/386 6.00 + original 32-bit OS/2 SDK. */
#define INCL_WIN
#define INCL_DOSPROCESS
#include <os2.h>
#include <stdio.h>
#include <string.h>

static HAB hab;
static HMQ hmq;
static ULONG creates, destroys, paints, characters;
static int automatic;
static CHAR line[] = "Type a key; resize the window; close it to finish.";
static CHAR keyline[] = "Last character: ?";
static volatile ULONG worker_done;

static void fail(ULONG n)
{
    printf("pmhello FAIL line %lu\n",n);
    DosExit(EXIT_PROCESS,1);
}
#define CHECK(x) do { if (!(x)) fail((ULONG)__LINE__); } while (0)

static VOID APIENTRY worker(ULONG unused)
{
    (void)unused;
    CHECK(DosSleep(30)==0);
    worker_done=1;
    DosExit(EXIT_THREAD,0);
}

static MRESULT EXPENTRY client_proc(HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2)
{
    HPS hps;
    RECTL rect;
    switch(msg) {
    case WM_CREATE:
        ++creates;
        /* Re-enter this same guest procedure through a nested host call. */
        CHECK((ULONG)WinSendMsg(hwnd,WM_USER,MPFROMLONG(17),MPFROMLONG(25))==42UL);
        return (MRESULT)0;
    case WM_USER:
        return (MRESULT)((ULONG)mp1+(ULONG)mp2);
    case WM_PAINT:
        hps=WinBeginPaint(hwnd,(HPS)0,&rect);
        CHECK(hps!=(HPS)0);
        CHECK(WinQueryWindowRect(hwnd,&rect));
        CHECK(WinFillRect(hps,&rect,CLR_WHITE));
        CHECK(WinDrawText(hps,-1,(PCH)(characters ? keyline : line),&rect,
                         CLR_BLACK,CLR_WHITE,DT_CENTER|DT_VCENTER)>0);
        CHECK(WinEndPaint(hps));
        ++paints;
        if(automatic) {
            if(!characters) {
                /* Yield inside a window callback while the worker runs. */
                CHECK(DosSleep(50)==0);
                CHECK(worker_done==1);
                CHECK(WinPostMsg(hwnd,WM_CHAR,MPFROMLONG(KC_CHAR),MPFROMSHORT('A')));
            } else {
                CHECK(WinPostMsg(hwnd,WM_CLOSE,(MPARAM)0,(MPARAM)0));
            }
        }
        return (MRESULT)0;
    case WM_CHAR:
        if((USHORT)(ULONG)mp1 & KC_CHAR) {
            ++characters;
            keyline[16]=(char)(ULONG)mp2;
            CHECK(WinInvalidateRect(hwnd,(PRECTL)0,FALSE));
            if((USHORT)(ULONG)mp2==27)
                CHECK(WinPostMsg(hwnd,WM_CLOSE,(MPARAM)0,(MPARAM)0));
        }
        return (MRESULT)0;
    case WM_CLOSE:
        CHECK(WinPostQueueMsg(hmq,WM_QUIT,(MPARAM)0,(MPARAM)0));
        return (MRESULT)0;
    case WM_DESTROY:
        ++destroys;
        return (MRESULT)0;
    }
    return WinDefWindowProc(hwnd,msg,mp1,mp2);
}

int main(int argc, char **argv)
{
    HWND frame,client;
    QMSG msg;
    TID tid;
    ULONG flags=FCF_TITLEBAR|FCF_SYSMENU|FCF_SIZEBORDER|
                FCF_MINBUTTON|FCF_MAXBUTTON|FCF_SHELLPOSITION|FCF_TASKLIST;
    CHAR cls[]="WHP_PMHELLO";
    CHAR title[]="WHP OS/2 R10 - guest PM window";
    automatic=argc>1 && strcmp(argv[1],"--auto")==0;
    CHECK(sizeof(QMSG)==28);
    CHECK(sizeof(RECTL)==16);
    CHECK(DosCreateThread(&tid,(PFNTHREAD)worker,0,0,16384)==0);
    hab=WinInitialize(0);CHECK(hab!=(HAB)0);
    hmq=WinCreateMsgQueue(hab,0);CHECK(hmq!=(HMQ)0);
    CHECK(WinRegisterClass(hab,(PSZ)cls,client_proc,CS_SIZEREDRAW,0));
    frame=WinCreateStdWindow(HWND_DESKTOP,WS_VISIBLE,&flags,(PSZ)cls,(PSZ)title,0,
                             (HMODULE)0,0,&client);
    CHECK(frame!=(HWND)0 && client!=(HWND)0 && creates==1);
    while(WinGetMsg(hab,&msg,(HWND)0,0,0)) WinDispatchMsg(hab,&msg);
    CHECK(WinDestroyWindow(frame));
    CHECK(destroys==1 && paints>0);
    if(automatic) CHECK(characters>0 && paints>=2);
    CHECK(WinDestroyMsgQueue(hmq));
    CHECK(WinTerminate(hab));
    CHECK(DosWaitThread(&tid,0)==0 && worker_done==1);
    printf("pmhello PASS (%lu paints, %lu character messages)\n",paints,characters);
    return 0;
}
