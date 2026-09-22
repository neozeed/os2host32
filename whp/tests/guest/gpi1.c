/* C/386 + Beta2 SDK: 4-bpp upload/readback/copy and a visible scaled bitmap. */
#define INCL_WIN
#define INCL_GPI
#define INCL_DEV
#define INCL_DOSPROCESS
#include <os2.h>
#include <stdio.h>
#include <string.h>
#define WIDTH 67
#define HEIGHT 48
#define STRIDE 36
static HAB hab;
static HMQ hmq;
static HDC dc1,dc2;
static HPS ps1,ps2;
static HBITMAP bm1,bm2;
static ULONG paints;
static int automatic;
/* Explicit fixed header + 16 colors: Beta2 BITMAPINFO2 has no palette entry. */
static struct { BITMAPINFOHEADER2 info; RGB2 colors[16]; } table;
typedef char gpi1_palette_layout_check[(sizeof(table)==128) ? 1 : -1];
static BYTE pixels[STRIDE*HEIGHT],readback[STRIDE*HEIGHT];
static ULONG palette[16]={0x000000,0x0000ff,0x00ff00,0x00ffff,
    0xff0000,0xff00ff,0xffff00,0xffffff,0x808080,0x000080,
    0x008000,0x008080,0x800000,0x800080,0x808000,0xc0c0c0};
static void fail(ULONG line)
{printf("gpi1 FAIL line %lu\n",line);DosExit(EXIT_PROCESS,1);}
#define CHECK(x) do {if(!(x))fail((ULONG)__LINE__);} while(0)
static void make_bitmap(void)
{
    BITMAPINFOHEADER2 header;
    SIZEL size;
    POINTL points[4],pel;
    RGB2 *rgb;
    ULONG x,y,c;
    CHAR token[]="*";
    CHECK(sizeof(BITMAPINFOHEADER2)==64 && sizeof(RGB2)==4);
    CHECK(sizeof(table)==64+16*4);
    memset(&table,0,sizeof(table));
    table.info.cbFix=64;table.info.cx=WIDTH;table.info.cy=HEIGHT;
    table.info.cPlanes=1;table.info.cBitCount=4;
    rgb=(RGB2 *)((PBYTE)&table+64);
    for(x=0;x<16;x++) {
        rgb[x].bRed=(BYTE)(palette[x]>>16);
        rgb[x].bGreen=(BYTE)(palette[x]>>8);rgb[x].bBlue=(BYTE)palette[x];
    }
    memset(pixels,0,sizeof(pixels));
    for(y=0;y<HEIGHT;y++)for(x=0;x<WIDTH;x++) {
        c=y<HEIGHT/2 ? (x<WIDTH/2 ? 1:6) : (x<WIDTH/2 ? 4:2);
        if((x<4 && y>=HEIGHT-12) || (x<12 && y>=HEIGHT-4))c=7;
        pixels[y*STRIDE+x/2]|=(BYTE)(c<<((x&1)?0:4));
    }
    dc1=DevOpenDC(hab,OD_MEMORY,(PSZ)token,0,(PDEVOPENDATA)0,(HDC)0);
    dc2=DevOpenDC(hab,OD_MEMORY,(PSZ)token,0,(PDEVOPENDATA)0,(HDC)0);
    CHECK(dc1!=(HDC)0 && dc2!=(HDC)0);
    size.cx=WIDTH;size.cy=HEIGHT;
    ps1=GpiCreatePS(hab,dc1,&size,PU_PELS|GPIF_LONG|GPIT_MICRO|GPIA_ASSOC);
    ps2=GpiCreatePS(hab,dc2,&size,PU_PELS|GPIF_LONG|GPIT_MICRO|GPIA_ASSOC);
    CHECK(ps1!=(HPS)0 && ps2!=(HPS)0);
    memcpy(&header,&table.info,sizeof(header));
    bm1=GpiCreateBitmap(ps1,&header,CBM_INIT,pixels,(PBITMAPINFO2)&table);
    bm2=GpiCreateBitmap(ps2,&header,0,(PBYTE)0,(PBITMAPINFO2)0);
    CHECK(bm1!=(HBITMAP)0 && bm2!=(HBITMAP)0);
    CHECK(GpiSetBitmap(ps1,bm1)!=(HBITMAP)-1L);
    CHECK(GpiSetBitmap(ps2,bm2)!=(HBITMAP)-1L);
    CHECK(GpiSetBitmapBits(ps2,0,HEIGHT,pixels,(PBITMAPINFO2)&table)==HEIGHT);
    /* RGB-mode SetPel modifies the bottom-left pixel in the source. */
    CHECK(GpiCreateLogColorTable(ps1,LCOL_RESET,LCOLF_RGB,0,0,(PLONG)0));
    CHECK(GpiSetColor(ps1,0xff0000L));
    CHECK(GpiSetBackMix(ps1,BM_LEAVEALONE));
    pel.x=pel.y=0;CHECK(GpiSetPel(ps1,&pel)==GPI_OK);
    pixels[0]=(BYTE)((pixels[0]&15)|0x40);
    memset(readback,0,sizeof(readback));
    memset(rgb,0,16*sizeof(RGB2));
    CHECK(GpiQueryBitmapBits(ps1,0,HEIGHT,readback,(PBITMAPINFO2)&table)==HEIGHT);
    CHECK(memcmp(pixels,readback,sizeof(pixels))==0);
    for(x=0;x<16;x++)CHECK(((ULONG)rgb[x].bRed<<16 |
        (ULONG)rgb[x].bGreen<<8 | rgb[x].bBlue)==palette[x]);
    memset(&header,0,sizeof(header));header.cbFix=sizeof(header);
    CHECK(GpiQueryBitmapInfoHeader(bm1,&header));
    CHECK(header.cx==WIDTH && header.cy==HEIGHT && header.cBitCount==4);
    points[0].x=points[0].y=points[2].x=points[2].y=0;
    points[1].x=points[3].x=WIDTH;points[1].y=points[3].y=HEIGHT;
    CHECK(GpiBitBlt(ps2,ps1,4,points,ROP_SRCCOPY,BBO_IGNORE)==GPI_OK);
    CHECK(GpiQueryBitmapBits(ps2,0,HEIGHT,readback,(PBITMAPINFO2)&table)==HEIGHT);
    CHECK(memcmp(pixels,readback,sizeof(pixels))==0);
    /* Selected bitmaps must not be deleted or selected into another PS. */
    CHECK(!GpiDeleteBitmap(bm1));
    CHECK(GpiSetBitmap(ps2,bm1)==HBM_ERROR);
}
static MRESULT EXPENTRY client_proc(HWND hwnd,USHORT msg,MPARAM mp1,MPARAM mp2)
{
    HPS paint;
    RECTL rect;
    POINTL points[4];
    switch(msg) {
    case WM_PAINT:
        paint=WinBeginPaint(hwnd,(HPS)0,&rect);CHECK(paint!=(HPS)0);
        CHECK(WinQueryWindowRect(hwnd,&rect));
        CHECK(WinFillRect(paint,&rect,CLR_WHITE));
        if(rect.xRight>0 && rect.yTop>0) {
            points[0].x=points[0].y=points[2].x=points[2].y=0;
            points[1].x=rect.xRight;points[1].y=rect.yTop;
            points[3].x=WIDTH;points[3].y=HEIGHT;
            CHECK(GpiBitBlt(paint,ps2,4,points,ROP_SRCCOPY,BBO_IGNORE)==GPI_OK);
            ++paints;
        }
        CHECK(WinEndPaint(paint));
        if(automatic && paints)CHECK(WinPostMsg(hwnd,WM_CLOSE,(MPARAM)0,(MPARAM)0));
        return (MRESULT)0;
    case WM_CHAR:
        if(((USHORT)(ULONG)mp1&KC_CHAR) && (USHORT)(ULONG)mp2==27)
            CHECK(WinPostMsg(hwnd,WM_CLOSE,(MPARAM)0,(MPARAM)0));
        return (MRESULT)0;
    case WM_CLOSE:
        CHECK(WinPostQueueMsg(hmq,WM_QUIT,(MPARAM)0,(MPARAM)0));return (MRESULT)0;
    }
    return WinDefWindowProc(hwnd,msg,mp1,mp2);
}
int main(int argc,char **argv)
{
    HWND frame,client;
    QMSG msg;
    ULONG flags=FCF_TITLEBAR|FCF_SYSMENU|FCF_SIZEBORDER|FCF_MINBUTTON|
                FCF_MAXBUTTON|FCF_SHELLPOSITION|FCF_TASKLIST;
    CHAR cls[]="WHP_GPI1",title[]="WHP OS/2 R11 - PMGPI bitmap test";
    automatic=argc>1 && strcmp(argv[1],"--auto")==0;
    hab=WinInitialize(0);CHECK(hab!=(HAB)0);
    hmq=WinCreateMsgQueue(hab,0);CHECK(hmq!=(HMQ)0);
    make_bitmap();
    CHECK(WinRegisterClass(hab,(PSZ)cls,client_proc,CS_SIZEREDRAW,0));
    frame=WinCreateStdWindow(HWND_DESKTOP,WS_VISIBLE,&flags,(PSZ)cls,(PSZ)title,0,(HMODULE)0,0,&client);
    CHECK(frame!=(HWND)0);
    while(WinGetMsg(hab,&msg,(HWND)0,0,0))WinDispatchMsg(hab,&msg);
    CHECK(WinDestroyWindow(frame));CHECK(paints>0);
    CHECK(GpiSetBitmap(ps1,(HBITMAP)0)==bm1);
    CHECK(GpiSetBitmap(ps2,(HBITMAP)0)==bm2);
    CHECK(GpiDeleteBitmap(bm1));CHECK(GpiDeleteBitmap(bm2));
    CHECK(GpiDestroyPS(ps1));CHECK(GpiDestroyPS(ps2));
    CHECK(DevCloseDC(dc1)!=(HMF)-1L);CHECK(DevCloseDC(dc2)!=(HMF)-1L);
    CHECK(WinDestroyMsgQueue(hmq));CHECK(WinTerminate(hab));
    printf("gpi1 PASS (%lu paints; bitmap readback, copy and cleanup)\n",paints);
    return 0;
}
