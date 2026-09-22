/* C/386 fixture for Sarien's shared PS, palette probing and worker blits. */
#define INCL_WIN
#define INCL_GPI
#define INCL_DEV
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSMISC
#include <os2.h>
#include <stdio.h>
#include <string.h>
#define W 320
#define H 200
static HAB hab;
static HMQ queue;
static HWND client;
static HDC dc,memdc;
static HPS screen,memory;
static HBITMAP bitmap;
static HMTX lock;
static volatile ULONG stop,ticks,frames,paints,keys;
static int automatic;
static BYTE pixels[W*H],map[33];
static union {ULONG align;BYTE raw[16+256*4];} info;
static POINTL points[3]={{0,0},{W,H},{0,0}};
static void fail(ULONG n){printf("sarprep1 FAIL line %lu\n",n);DosExit(EXIT_PROCESS,1);}
#define CHECK(x) do {if(!(x))fail((ULONG)__LINE__);} while(0)
static void prepare(void)
{
    BITMAPINFOHEADER2 hdr;
    SIZEL size;
    LONG colors[33];
    POINTL pel;
    ULONG i;
    CHAR token[]="*";
    dc=WinOpenWindowDC(client);CHECK(dc!=(HDC)0);
    size.cx=size.cy=0;
    screen=GpiCreatePS(hab,dc,&size,PU_PELS|GPIT_MICRO|GPIA_ASSOC);
    memdc=DevOpenDC(hab,OD_MEMORY,(PSZ)token,0,(PDEVOPENDATA)0,(HDC)0);
    memory=GpiCreatePS(hab,memdc,&size,PU_PELS|GPIT_MICRO|GPIA_ASSOC);
    CHECK(screen!=(HPS)0 && memory!=(HPS)0);
    memset(&hdr,0,sizeof(hdr));hdr.cbFix=sizeof(hdr);hdr.cx=W;hdr.cy=H;hdr.cPlanes=1;hdr.cBitCount=8;
    bitmap=GpiCreateBitmap(memory,&hdr,0,(PBYTE)0,(PBITMAPINFO2)0);CHECK(bitmap!=(HBITMAP)0);
    CHECK(GpiSetBitmap(memory,bitmap)!=HBM_ERROR);
    for(i=0;i<33;i++)colors[i]=i==0?0:i==32?0xffffffL:(i&1)?0x55ffffL:0xff55ffL;
    CHECK(GpiCreateLogColorTable(memory,LCOL_PURECOLOR,LCOLF_CONSECRGB,0,33,colors));
    CHECK(GpiSetBackMix(memory,BM_OVERPAINT));
    for(i=0;i<33;i++) {
        CHECK(GpiSetColor(memory,(LONG)i));pel.x=(LONG)i;pel.y=0;
        CHECK(GpiSetPel(memory,&pel)==GPI_OK);
    }
    /* Repeated WinInitialize and the short bitmap header are intentional. */
    CHECK(WinInitialize(0)==hab);
    memset(info.raw,0,sizeof(info.raw));((PBITMAPINFOHEADER2)info.raw)->cbFix=16;
    CHECK(GpiQueryBitmapInfoHeader(bitmap,(PBITMAPINFOHEADER2)info.raw));
    CHECK(((PBITMAPINFOHEADER2)info.raw)->cbFix==16);
    CHECK(GpiQueryBitmapBits(memory,0,H,pixels,(PBITMAPINFO2)info.raw)==H);
    for(i=0;i<33;i++) {
        RGB2 *rgb=(RGB2 *)(info.raw+16);map[i]=pixels[i];
        CHECK(((ULONG)rgb[map[i]].bRed<<16 | (ULONG)rgb[map[i]].bGreen<<8 | rgb[map[i]].bBlue)==(ULONG)colors[i]);
    }
    CHECK(WinFocusChange(HWND_DESKTOP,client,0));
}
static VOID APIENTRY ticker(ULONG ignored)
{
    (void)ignored;
    while(!stop){CHECK(DosSleep(5)==0);++ticks;}
    DosExit(EXIT_THREAD,0);
}
static VOID APIENTRY painter(ULONG ignored)
{
    ULONG x,y,n,before;
    HAB workerhab;
    (void)ignored;
    workerhab=WinInitialize(0);CHECK(workerhab!=(HAB)0 && workerhab!=hab);
    before=ticks;CHECK(DosBeep(880,80)==0);CHECK(ticks>before);
    for(n=0;n<40 && !stop;n++) {
        CHECK(DosRequestMutexSem(lock,SEM_INDEFINITE_WAIT)==0);
        for(y=0;y<H;y++)for(x=0;x<W;x++)pixels[y*W+x]=map[((x/8+y/8+n)%4)==3?32:(x/8+y/8+n)%4];
        CHECK(GpiSetBitmapBits(memory,0,H,pixels,(PBITMAPINFO2)info.raw)==H);
        CHECK(GpiBitBlt(screen,memory,3,points,ROP_SRCCOPY,BBO_AND)==GPI_OK);
        CHECK(DosReleaseMutexSem(lock)==0);++frames;
        CHECK(DosSleep(20)==0);
    }
    CHECK(WinTerminate(workerhab));
    if(automatic)CHECK(WinPostMsg(client,WM_CLOSE,(MPARAM)0,(MPARAM)0));
    DosExit(EXIT_THREAD,0);
}
static MRESULT EXPENTRY procedure(HWND hwnd,USHORT msg,MPARAM a,MPARAM b)
{
    switch(msg) {
    case WM_CREATE:client=hwnd;prepare();return (MRESULT)0;
    case WM_PAINT:
        CHECK(DosRequestMutexSem(lock,SEM_INDEFINITE_WAIT)==0);
        CHECK(WinBeginPaint(hwnd,screen,(PRECTL)0)==screen);
        CHECK(GpiBitBlt(screen,memory,3,points,ROP_SRCCOPY,BBO_AND)==GPI_OK);
        CHECK(WinEndPaint(screen));++paints;
        CHECK(DosReleaseMutexSem(lock)==0);return (MRESULT)0;
    case WM_CHAR:
        if(!(SHORT1FROMMP(a)&KC_KEYUP)) {
            ++keys;
            printf("key char=%u virtual=%u\n",(unsigned)SHORT1FROMMP(b),(unsigned)SHORT2FROMMP(b));
            if(SHORT2FROMMP(b)==VK_ESC)CHECK(WinPostMsg(hwnd,WM_CLOSE,(MPARAM)0,(MPARAM)0));
        }
        return (MRESULT)0;
    case WM_CLOSE:
        stop=1;CHECK(WinPostQueueMsg(queue,WM_QUIT,(MPARAM)0,(MPARAM)0));return (MRESULT)0;
    }
    return WinDefWindowProc(hwnd,msg,a,b);
}
int main(int argc,char **argv)
{
    HWND frame;
    TID tid,timer;
    QMSG msg;
    ULONG flags=FCF_TITLEBAR|FCF_SYSMENU|FCF_MINBUTTON|FCF_TASKLIST;
    CHAR cls[]="SARPREP1",title[]="WHP R12 - Sarien integration test";
    automatic=argc>1 && strcmp(argv[1],"--auto")==0;
    hab=WinInitialize(0);CHECK(hab!=(HAB)0);
    queue=WinCreateMsgQueue(hab,0);CHECK(queue!=(HMQ)0);
    CHECK(DosCreateMutexSem((PSZ)0,&lock,0,FALSE)==0);
    CHECK(WinRegisterClass(hab,(PSZ)cls,procedure,CS_SIZEREDRAW,0));
    frame=WinCreateStdWindow(HWND_DESKTOP,WS_VISIBLE,&flags,(PSZ)cls,(PSZ)title,WS_VISIBLE,(HMODULE)0,0,&client);
    CHECK(frame!=(HWND)0);
    CHECK(WinSetWindowPos(frame,(HWND)0,60,(SHORT)(WinQuerySysValue(HWND_DESKTOP,SV_CYSCREEN)-180),
                         W+8,(SHORT)(H+WinQuerySysValue(HWND_DESKTOP,SV_CYTITLEBAR)+8),SWP_SIZE|SWP_MOVE));
    CHECK(DosCreateThread(&timer,(PFNTHREAD)ticker,0,0,16384)==0);
    CHECK(DosCreateThread(&tid,(PFNTHREAD)painter,0,0,16384)==0);
    while(WinGetMsg(hab,&msg,(HWND)0,0,0))WinDispatchMsg(hab,&msg);
    stop=1;CHECK(DosWaitThread(&tid,0)==0);CHECK(DosWaitThread(&timer,0)==0);
    CHECK(frames>0 && ticks>0);if(automatic)CHECK(frames==40);
    CHECK(GpiSetBitmap(memory,(HBITMAP)0)==bitmap);CHECK(GpiDeleteBitmap(bitmap));
    CHECK(GpiDestroyPS(memory));CHECK(GpiDestroyPS(screen));CHECK(DevCloseDC(memdc)!=(HMF)-1L);
    CHECK(DosCloseMutexSem(lock)==0);CHECK(WinDestroyWindow(frame));
    CHECK(WinDestroyMsgQueue(queue));CHECK(WinTerminate(hab));
    printf("sarprep1 PASS (%lu worker frames, %lu ticks, %lu paints, %lu keys)\n",frames,ticks,paints,keys);
    return 0;
}
