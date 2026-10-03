/* Native Windows R7 regression: actual DLL ordinal calls and GDI pixels.
 * No historical executable, private CompatPS layout, or network required. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define FN(ret,name,args) typedef ret (__cdecl *name##Fn) args; static name##Fn name
#define CHECK(c) do { ++checks; if(!(c)) { printf("FAIL line %d: %s (Win32=%lu)\n",__LINE__,#c,(unsigned long)GetLastError()); return 1; } } while(0)
#define BIND(module,name,ordinal) do { FARPROC p=GetProcAddress(module,(LPCSTR)(ULONG_PTR)(ordinal)); CHECK(p!=NULL); memcpy(&name,&p,sizeof(name)); } while(0)

struct Point { LONG x,y; };
struct Rect { LONG left,bottom,right,top; };
struct Info2 {
    DWORD size,width,height; WORD planes,bits;
    DWORD compression,image,xres,yres,used,important;
    WORD units,reserved,recording,rendering;
    DWORD size1,size2,encoding,identifier;
    RGBQUAD colors[256];
};
struct Info1 { DWORD size; WORD width,height,planes,bits; RGBTRIPLE colors[256]; };
struct Qmsg { DWORD hwnd; WORD msg,reserved; DWORD mp1,mp2,time; LONG x,y; };
typedef DWORD (__cdecl *GuestProc)(DWORD,WORD,DWORD,DWORD);
FN(DWORD,create_ps,(DWORD,DWORD,void *,DWORD));
FN(LONG,destroy_ps,(DWORD));
FN(DWORD,create_bitmap,(DWORD,const void *,DWORD,const void *,const void *));
FN(DWORD,set_bitmap,(DWORD,DWORD));
FN(LONG,delete_bitmap,(DWORD));
FN(LONG,query_bits,(DWORD,LONG,LONG,void *,void *));
FN(LONG,set_color,(DWORD,LONG));
FN(LONG,move_to,(DWORD,const struct Point *));
FN(LONG,draw_box,(DWORD,LONG,const struct Point *,LONG,LONG));
FN(LONG,bit_blt,(DWORD,DWORD,LONG,const struct Point *,LONG,DWORD));
FN(DWORD,fill_rect,(DWORD,const struct Rect *,LONG));
FN(DWORD,register_class,(DWORD,const char *,GuestProc,DWORD,DWORD));
FN(DWORD,create_window,(DWORD,DWORD,DWORD *,const char *,const char *,DWORD,DWORD,DWORD,DWORD *));
FN(DWORD,destroy_window,(DWORD));
FN(DWORD,create_queue,(DWORD,LONG));
FN(DWORD,destroy_queue,(DWORD));
FN(DWORD,get_message,(DWORD,struct Qmsg *,DWORD,DWORD,DWORD));

static int checks;
static void init_info(struct Info2 *info,WORD bits)
{
    unsigned i,n=bits<=8?(1U<<bits):0;
    memset(info,0,sizeof(*info));
    info->size=64;info->width=8;info->height=6;info->planes=1;info->bits=bits;
    info->used=n;
    for(i=0;i<n;++i) {
        BYTE value=(BYTE)(255U*i/(n-1));
        info->colors[i].rgbRed=value;info->colors[i].rgbGreen=value;info->colors[i].rgbBlue=value;
    }
}
static COLORREF pixel(HDC dc,int x,int y) { return GetPixel(dc,x,5-y); }
static int check_drawing(WORD depth)
{
    struct Info2 info; struct Point start,end; struct Rect rect;
    BYTE zero[192]; HDC dc; DWORD ps,bitmap; int x,y,inside;
    init_info(&info,depth);memset(zero,0,sizeof(zero));
    dc=CreateCompatibleDC(NULL);CHECK(dc!=NULL);
    ps=create_ps(1,(DWORD)(ULONG_PTR)dc,NULL,0);CHECK(ps!=0);
    bitmap=create_bitmap(ps,&info,4,zero,&info);CHECK(bitmap!=0); /* CBM_INIT */
    CHECK(set_bitmap(ps,bitmap)==0);

    start.x=2;start.y=1;end.x=5;end.y=3;
    CHECK(move_to(ps,&start));set_color(ps,-2);
    CHECK(draw_box(ps,1,&end,0,0));
    for(y=0;y<6;++y) for(x=0;x<8;++x) {
        inside=x>=2 && x<=5 && y>=1 && y<=3;
        CHECK(pixel(dc,x,y)==(inside?RGB(255,255,255):RGB(0,0,0)));
    }
    CHECK(PatBlt(dc,0,0,8,6,BLACKNESS));
    end.y=4;CHECK(move_to(ps,&start));CHECK(draw_box(ps,2,&end,0,0));
    for(y=0;y<6;++y) for(x=0;x<8;++x) {
        inside=x>=2 && x<=5 && y>=1 && y<=4 && (x==2 || x==5 || y==1 || y==4);
        CHECK(pixel(dc,x,y)==(inside?RGB(255,255,255):RGB(0,0,0)));
    }
    CHECK(PatBlt(dc,0,0,8,6,BLACKNESS));
    rect.left=1;rect.bottom=0;rect.right=4;rect.top=2;
    CHECK(fill_rect(ps,&rect,-2));
    for(y=0;y<6;++y) for(x=0;x<8;++x) {
        inside=x>=1 && x<4 && y<2;
        CHECK(pixel(dc,x,y)==(inside?RGB(255,255,255):RGB(0,0,0)));
    }
    CHECK(fill_rect(ps,NULL,-2));
    for(y=0;y<6;++y) for(x=0;x<8;++x) CHECK(pixel(dc,x,y)==RGB(255,255,255));
    CHECK(PatBlt(dc,0,0,8,6,BLACKNESS));
    rect.left=-2;rect.bottom=1;rect.right=3;rect.top=4;
    CHECK(fill_rect(ps,&rect,-2));
    for(y=0;y<6;++y) for(x=0;x<8;++x) {
        inside=x<3 && y>=1 && y<4;
        CHECK(pixel(dc,x,y)==(inside?RGB(255,255,255):RGB(0,0,0)));
    }
    CHECK(set_bitmap(ps,0)==bitmap);CHECK(delete_bitmap(bitmap));
    CHECK(destroy_ps(ps));CHECK(DeleteDC(dc));
    printf("Memory bitmap fill, outline, coordinate and clipping checks: %u bpp PASS\n",(unsigned)depth);
    return 0;
}
static int check_palette(WORD depth,int legacy)
{
    struct Info2 info,target_info,returned;
    struct Info1 old;
    struct Point points[3];
    HDC dc,target_dc;DWORD ps,bitmap,target_ps,target_bitmap;
    RGBQUAD palette[256];BYTE bits[192],readback[192],zero[192];
    DWORD stride=((8UL*depth+31)/32)*4;
    const void *input;
    unsigned i,n=1U<<depth;COLORREF color=RGB(42,180,90);
    init_info(&info,depth);init_info(&target_info,32);
    info.colors[1].rgbRed=42;info.colors[1].rgbGreen=180;info.colors[1].rgbBlue=90;
    memset(&old,0,sizeof(old));old.size=12;old.width=8;old.height=6;old.planes=1;old.bits=depth;
    for(i=0;i<n;++i) {
        old.colors[i].rgbtRed=info.colors[i].rgbRed;
        old.colors[i].rgbtGreen=info.colors[i].rgbGreen;
        old.colors[i].rgbtBlue=info.colors[i].rgbBlue;
    }
    input=legacy?(const void *)&old:(const void *)&info;
    memset(bits,depth==1?0xff:depth==4?0x11:1,sizeof(bits));memset(zero,0,sizeof(zero));
    dc=CreateCompatibleDC(NULL);target_dc=CreateCompatibleDC(NULL);CHECK(dc && target_dc);
    ps=create_ps(1,(DWORD)(ULONG_PTR)dc,NULL,0);target_ps=create_ps(1,(DWORD)(ULONG_PTR)target_dc,NULL,0);CHECK(ps && target_ps);
    bitmap=create_bitmap(ps,input,4,bits,input);target_bitmap=create_bitmap(target_ps,&target_info,4,zero,NULL);CHECK(bitmap && target_bitmap);
    CHECK(set_bitmap(ps,bitmap)==0 && set_bitmap(target_ps,target_bitmap)==0);
    CHECK(GetDIBColorTable(dc,0,n,palette)==n);
    CHECK(palette[1].rgbRed==42 && palette[1].rgbGreen==180 && palette[1].rgbBlue==90);
    CHECK(GetPixel(dc,0,0)==color);
    memset(&returned,0,sizeof(returned));returned.size=64;
    CHECK(query_bits(ps,0,6,readback,&returned)==6 && !memcmp(readback,bits,stride*6));
    CHECK(returned.colors[1].rgbRed==42 && returned.colors[1].rgbGreen==180 && returned.colors[1].rgbBlue==90);
    points[0].x=0;points[0].y=0;points[1].x=8;points[1].y=6;points[2].x=0;points[2].y=0;
    /* Monochrome-to-colour blits have separate GDI text/background rules.
       Check indexed-colour transfer here; 1bpp palette publication above. */
    if(depth>1) {
        CHECK(bit_blt(target_ps,ps,3,points,0xcc,0));
        CHECK(GetPixel(target_dc,0,0)==color && GetPixel(target_dc,7,5)==color);
    }
    CHECK(set_bitmap(ps,0)==bitmap && set_bitmap(target_ps,0)==target_bitmap);
    CHECK(delete_bitmap(bitmap) && delete_bitmap(target_bitmap));
    CHECK(destroy_ps(ps) && destroy_ps(target_ps));CHECK(DeleteDC(dc) && DeleteDC(target_dc));
    printf("Palette retention/publication%s: %u bpp, %s header PASS\n",depth>1?" and blit":"",(unsigned)depth,legacy?"12-byte":"64-byte");
    return 0;
}
static DWORD __cdecl window_proc(DWORD hwnd,WORD msg,DWORD mp1,DWORD mp2)
{
    (void)hwnd;(void)msg;(void)mp1;(void)mp2;return 0;
}
static int check_windows(void)
{
    DWORD flags=1,queue,a,b,c,client;struct Qmsg msg;
    const UINT wake=WM_APP+0x101;
    queue=create_queue(1,0);CHECK(queue!=0);
    CHECK(register_class(1,"OS2HOST32_PM_MERGE_TEST",window_proc,0,0));
    a=create_window(1,0x80000000UL,&flags,"OS2HOST32_PM_MERGE_TEST","R7 first",0,0,0,&client);
    b=create_window(1,0x80000000UL,&flags,"OS2HOST32_PM_MERGE_TEST","R7 second",0,0,0,&client);
    c=create_window(1,0x80000000UL,&flags,"OS2HOST32_PM_MERGE_TEST","R7 destroyed before show",0,0,0,&client);
    CHECK(a && b && c && a!=b && b!=c);
    CHECK(!IsWindowVisible((HWND)(ULONG_PTR)a) && !IsWindowVisible((HWND)(ULONG_PTR)b));
    CHECK(destroy_window(c));
    CHECK(PostThreadMessageA(GetCurrentThreadId(),wake,0,0));
    CHECK(get_message(1,&msg,0,wake,wake));
    CHECK(IsWindowVisible((HWND)(ULONG_PTR)a) && IsWindowVisible((HWND)(ULONG_PTR)b));
    c=create_window(1,0x80000000UL,&flags,"OS2HOST32_PM_MERGE_TEST","R7 later window",0,0,0,&client);
    CHECK(c && !IsWindowVisible((HWND)(ULONG_PTR)c));
    CHECK(PostThreadMessageA(GetCurrentThreadId(),wake,0,0));CHECK(get_message(1,&msg,0,wake,wake));
    CHECK(IsWindowVisible((HWND)(ULONG_PTR)c));
    CHECK(destroy_window(c) && destroy_window(b) && destroy_window(a));CHECK(destroy_queue(queue));
    puts("Multiple deferred windows and destroyed pending window PASS");return 0;
}
int main(void)
{
    HMODULE gpi,win;unsigned i;const WORD depths[]={1,4,8,24,32};
    gpi=LoadLibraryA("PMGPI.dll");win=LoadLibraryA("PMWIN.dll");CHECK(gpi && win);
    CHECK(offsetof(struct Info2,colors)==64 && offsetof(struct Info1,colors)==12);
    BIND(gpi,create_ps,369);BIND(gpi,destroy_ps,379);
    BIND(gpi,create_bitmap,598);BIND(gpi,set_bitmap,506);BIND(gpi,delete_bitmap,371);
    BIND(gpi,query_bits,599);BIND(gpi,set_color,517);BIND(gpi,move_to,404);
    BIND(gpi,draw_box,356);BIND(gpi,bit_blt,355);BIND(win,fill_rect,743);
    BIND(win,register_class,926);BIND(win,create_window,908);BIND(win,destroy_window,728);
    BIND(win,create_queue,716);BIND(win,destroy_queue,726);BIND(win,get_message,915);
    for(i=0;i<5;++i) if(check_drawing(depths[i])) return 1;
    for(i=0;i<3;++i) if(check_palette(depths[i],0) || check_palette(depths[i],1)) return 1;
    if(check_windows()) return 1;
    printf("R7 merged PM smoke: %d checks PASS\n",checks);return 0;
}
