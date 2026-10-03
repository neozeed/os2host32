/* R8: real DLL ordinals, synthetic OS/2 dialog resources and GDI pixel oracles.
 * No historical program or network needed. Run on native 32/64-bit Windows. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define FN(ret,name,args) typedef ret (__cdecl *name##Fn) args; static name##Fn name
#define PTR(p) ((DWORD)(ULONG_PTR)(p))
#define CHECK(c) do { ++checks; if(!(c)) { printf("FAIL line %d: %s (Win32=%lu)\n",__LINE__,#c,(unsigned long)GetLastError()); return 1; } } while(0)
#define BIND(m,n,o) do { FARPROC p=GetProcAddress(m,(LPCSTR)(ULONG_PTR)(o)); CHECK(p!=NULL); memcpy(&n,&p,sizeof(n)); } while(0)
struct Point { LONG x,y; };
struct Font { WORD length,selection; LONG match; char face[32]; WORD registry,cp; LONG height,width; WORD type,use; };
typedef DWORD (__cdecl *GuestProc)(DWORD,WORD,DWORD,DWORD);
FN(DWORD,create_ps,(DWORD,DWORD,void *,DWORD));
FN(LONG,destroy_ps,(DWORD));
FN(LONG,create_font,(DWORD,const char *,LONG,const struct Font *));
FN(LONG,set_font,(DWORD,LONG));
FN(LONG,delete_font,(DWORD,LONG));
FN(LONG,back_mix,(DWORD,LONG));
FN(LONG,back_color,(DWORD,LONG));
FN(LONG,text_color,(DWORD,LONG));
FN(LONG,draw_text,(DWORD,const struct Point *,LONG,const char *));
FN(LONG,text_box,(DWORD,LONG,const char *,LONG,struct Point *));
FN(LONG,width_table,(DWORD,LONG,LONG,LONG *));
FN(int,register_resource,(DWORD,WORD,WORD,const void *,DWORD));
FN(DWORD,load_dialog,(DWORD,DWORD,GuestProc,DWORD,WORD,void *));
FN(DWORD,destroy_window,(DWORD));
FN(DWORD,send_item,(DWORD,WORD,WORD,DWORD,DWORD));
FN(DWORD,send_msg,(DWORD,WORD,DWORD,DWORD));
FN(LONG,query_text,(DWORD,LONG,char *));
FN(DWORD,get_ps,(DWORD));
FN(DWORD,release_ps,(DWORD));
FN(DWORD,begin_paint,(DWORD,DWORD,void *));
FN(DWORD,end_paint,(DWORD));
FN(DWORD,set_cp,(DWORD));
static int checks;
static DWORD notifications,last_notification,init_seen;
static DWORD __cdecl dialog_proc(DWORD hwnd,WORD msg,DWORD mp1,DWORD mp2)
{
    (void)hwnd; (void)mp2;
    if(msg==0x3b) ++init_seen;
    if(msg==0x30) { ++notifications; last_notification=mp1; }
    return 0;
}
static void word_at(BYTE *b,unsigned off,WORD value) { memcpy(b+off,&value,2); }
static void dword_at(BYTE *b,unsigned off,DWORD value) { memcpy(b+off,&value,4); }
static int combo_test(void)
{
    BYTE resource[96]; DWORD dialog,n; HWND combo; char cls[32],text[64];
    /* 14-byte DLGTEMPLATE + two 30-byte DLGTITEMs. Class 2 is WC_COMBOBOX.
     * Same class/style as TELNETPM dialog 300/control 301; synthetic text. */
    memset(resource,0,sizeof(resource));
    word_at(resource,0,sizeof(resource));word_at(resource,4,437);word_at(resource,6,14);
    word_at(resource,14+2,1);word_at(resource,14+6,1);word_at(resource,14+8,13);
    word_at(resource,14+10,74);dword_at(resource,14+12,0x94000080UL);
    word_at(resource,14+20,226);word_at(resource,14+22,78);word_at(resource,14+24,300);
    word_at(resource,14+26,0xffff);word_at(resource,14+28,0xffff);
    word_at(resource,44+6,2);dword_at(resource,44+12,0x80030004UL);
    word_at(resource,44+16,15);word_at(resource,44+18,26);
    word_at(resource,44+20,196);word_at(resource,44+22,44);word_at(resource,44+24,301);
    word_at(resource,44+26,0xffff);word_at(resource,44+28,0xffff);
    memcpy(resource+74,"R8 Font smoke",13);
    CHECK(register_resource(0,4,300,resource,sizeof(resource)));
    dialog=load_dialog(1,1,dialog_proc,0,300,NULL);CHECK(dialog && init_seen==1);
    combo=GetDlgItem((HWND)(ULONG_PTR)dialog,301);CHECK(combo!=NULL);
    CHECK(GetClassNameA(combo,cls,sizeof(cls)) && !lstrcmpiA(cls,"COMBOBOX"));
    CHECK(IsWindowVisible(combo));
    CHECK((GetWindowLongA(combo,GWL_STYLE)&3)==CBS_DROPDOWNLIST);
    CHECK(send_item(dialog,301,0x161,0xffff,PTR("Courier New 8x16"))==0);
    CHECK(send_item(dialog,301,0x161,0xffff,PTR("Consolas 10x20"))==1);
    CHECK(send_msg(PTR(combo),0x161,0,PTR("Lucida Console 8x16"))==0);
    CHECK(send_item(dialog,301,0x160,0,0)==3);
    CHECK(send_item(dialog,301,0x164,1,1));
    CHECK(send_item(dialog,301,0x165,0xffff,0)==1);
    CHECK(query_text(PTR(combo),sizeof(text),text)==16 && !strcmp(text,"Courier New 8x16"));
    memset(text,'?',sizeof(text));
    CHECK(send_item(dialog,301,0x168,MAKELONG(2,6),PTR(text))==5 && !strcmp(text,"Conso") && text[6]=='?');
    CHECK(send_item(dialog,301,0x167,2,0)==14);
    CHECK(send_item(dialog,301,0x169,2,0x12345678));
    CHECK(send_item(dialog,301,0x16a,2,0)==0x12345678);
    n=notifications;
    SendMessageA((HWND)(ULONG_PTR)dialog,WM_COMMAND,MAKEWPARAM(301,CBN_SELCHANGE),(LPARAM)combo);
    CHECK(notifications==n+1 && last_notification==MAKELONG(301,4));
    SendMessageA((HWND)(ULONG_PTR)dialog,WM_COMMAND,MAKEWPARAM(301,CBN_DBLCLK),(LPARAM)combo);
    CHECK(last_notification==MAKELONG(301,7));
    CHECK(send_item(dialog,301,0x164,1,0));
    CHECK((LONG)send_item(dialog,301,0x165,0xffff,0)==-1);
    CHECK(send_item(dialog,301,0x163,0,0)==2);
    CHECK(send_item(dialog,301,0x16e,0,0) && !send_item(dialog,301,0x160,0,0));
    CHECK(destroy_window(dialog));
    puts("Dialog resource combo, LM messages, selected text and WM_CONTROL mapping PASS");
    return 0;
}
#define WIDTH 160
#define HEIGHT 64
struct Surface { HDC dc; HBITMAP bitmap,old; DWORD *pixels; };
static int surface(struct Surface *s)
{
    BITMAPINFO info;
    memset(&info,0,sizeof(info));info.bmiHeader.biSize=sizeof(info.bmiHeader);
    info.bmiHeader.biWidth=WIDTH;info.bmiHeader.biHeight=-HEIGHT;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    s->dc=CreateCompatibleDC(NULL);CHECK(s->dc!=NULL);
    s->bitmap=CreateDIBSection(s->dc,&info,DIB_RGB_COLORS,(void **)&s->pixels,NULL,0);
    CHECK(s->bitmap!=NULL);s->old=(HBITMAP)SelectObject(s->dc,s->bitmap);
    CHECK(s->old!=NULL);return 0;
}
static int same_pixels(const struct Surface *a,const struct Surface *b)
{
    unsigned i; GdiFlush();
    for(i=0;i<WIDTH*HEIGHT;++i) if((a->pixels[i]&0xffffff)!=(b->pixels[i]&0xffffff)) {
        printf("Pixel mismatch (%u,%u): %06lx vs %06lx\n",i%WIDTH,i/WIDTH,
            (unsigned long)(a->pixels[i]&0xffffff),(unsigned long)(b->pixels[i]&0xffffff));return 0;
    }
    return 1;
}
static void close_surface(struct Surface *s)
{
    SelectObject(s->dc,s->old);DeleteObject(s->bitmap);DeleteDC(s->dc);
}
static int render_case(DWORD ps,struct Surface *actual,struct Surface *expected,
                       const char *bytes,LONG length,const WCHAR *wide,int nch,int opaque)
{
    struct Point point,box[5]; HFONT previous; TEXTMETRICW tm; SIZE extent;
    UINT alignment; DWORD fill=WHITENESS;
    CHECK(PatBlt(actual->dc,0,0,WIDTH,HEIGHT,fill) && PatBlt(expected->dc,0,0,WIDTH,HEIGHT,fill));
    point.x=8;point.y=24;
    CHECK(back_mix(ps,opaque?2:5));CHECK(back_color(ps,-1));text_color(ps,2);
    CHECK(draw_text(ps,&point,length,bytes)==1); /* GPI_OK */
    previous=(HFONT)SelectObject(expected->dc,GetCurrentObject(actual->dc,OBJ_FONT));
    CHECK(GetTextMetricsW(expected->dc,&tm));
    SetTextColor(expected->dc,RGB(255,0,0));SetBkColor(expected->dc,RGB(0,0,0));
    SetBkMode(expected->dc,opaque?OPAQUE:TRANSPARENT);
    alignment=SetTextAlign(expected->dc,TA_LEFT|TA_TOP|TA_NOUPDATECP);
    CHECK(TextOutW(expected->dc,point.x,HEIGHT-point.y-tm.tmAscent,wide,nch));
    SetTextAlign(expected->dc,alignment);
    CHECK(same_pixels(actual,expected));
    CHECK(text_box(ps,length,bytes,5,box));CHECK(GetTextExtentPoint32W(expected->dc,wide,nch,&extent));
    CHECK(box[0].y==tm.tmAscent && box[1].y==-tm.tmDescent && box[4].x==extent.cx && box[4].y==0);
    SelectObject(expected->dc,previous);
    return 0;
}
static int font_test(void)
{
    struct Surface actual,expected; struct Font font; DWORD ps,ps2,ps3;
    HFONT initial,current; TEXTMETRICA tm; LONG width; int i;
    HWND hwnd; HDC window_dc;
    const char cp437[]={ (char)0xda,(char)0xc4,(char)0xbf,(char)0xb3,(char)0xdb,(char)0x82 };
    const WCHAR unicode437[]={0x250c,0x2500,0x2510,0x2502,0x2588,0x00e9};
    const WCHAR unicode850[]={0x0131},unicode1252[]={0x00d5};
    CHECK(!surface(&actual) && !surface(&expected));
    initial=(HFONT)GetCurrentObject(actual.dc,OBJ_FONT);
    ps=create_ps(1,PTR(actual.dc),NULL,0);CHECK(ps!=0 && GetBkMode(actual.dc)==TRANSPARENT);
    memset(&font,0,sizeof(font));font.length=sizeof(font);font.height=16;font.width=8;
    font.cp=437;strcpy(font.face,"Courier New");
    CHECK(create_font(ps,NULL,1,&font)==2); /* TELNETPM's exact selection gate */
    CHECK(GetCurrentObject(actual.dc,OBJ_FONT)==initial);
    CHECK(set_font(ps,1));CHECK(GetTextMetricsA(actual.dc,&tm));
    CHECK(tm.tmHeight==16 && tm.tmAveCharWidth==8 && !(tm.tmPitchAndFamily&TMPF_FIXED_PITCH));
    CHECK(!render_case(ps,&actual,&expected,cp437,sizeof(cp437),unicode437,6,1));
    CHECK(!render_case(ps,&actual,&expected,"    ",4,L"    ",4,1));
    CHECK(!render_case(ps,&actual,&expected,"    ",4,L"    ",4,0));
    CHECK(width_table(ps,0x82,1,&width) && width==8);
    CHECK(!width_table(ps,255,2,&width));
    CHECK(back_mix(ps,0) && GetBkMode(actual.dc)==TRANSPARENT);
    CHECK(!back_mix(ps,99) && GetBkMode(actual.dc)==TRANSPARENT);
    for(i=0;i<40;++i) {
        font.selection=(WORD)((i&1)?0x22:0); /* recreate CURRENT ID, as each color/style span does */
        CHECK(create_font(ps,NULL,1,&font)==2 && set_font(ps,1));
        CHECK(GetTextMetricsA(actual.dc,&tm));
        CHECK(!!tm.tmUnderlined==!!(i&1) && (tm.tmWeight>=FW_BOLD)==!!(i&1));
    }
    font.selection=0;font.cp=1252;
    CHECK(create_font(ps,NULL,1,&font)==2);
    CHECK(!render_case(ps,&actual,&expected,"\xd5",1,unicode1252,1,1));
    font.cp=0;CHECK(create_font(ps,NULL,1,&font)==2);
    CHECK(set_cp(850)==0);
    CHECK(!render_case(ps,&actual,&expected,"\xd5",1,unicode850,1,1));
    CHECK(set_cp(437)==0);
    CHECK(!render_case(ps,&actual,&expected,cp437,sizeof(cp437),unicode437,6,1));
    current=(HFONT)GetCurrentObject(actual.dc,OBJ_FONT);
    strcpy(font.face,"__OS2_NO_SUCH_FACE__");CHECK(create_font(ps,NULL,2,&font)==1);
    CHECK(GetCurrentObject(actual.dc,OBJ_FONT)==current);CHECK(delete_font(ps,2));
    CHECK(set_font(ps,0));CHECK(delete_font(ps,1));
    strcpy(font.face,"Courier New");CHECK(create_font(ps,NULL,0,&font)==2);
    CHECK(GetCurrentObject(actual.dc,OBJ_FONT)!=initial);
    CHECK(delete_font(ps,0) && GetCurrentObject(actual.dc,OBJ_FONT)==initial);
    CHECK(create_font(ps,NULL,1,&font)==2 && set_font(ps,1));
    CHECK(destroy_ps(ps));CHECK(GetCurrentObject(actual.dc,OBJ_FONT)==initial);
    /* HPS allocation on PMWIN paths must have the same initial background. */
    hwnd=CreateWindowA("STATIC","R8 PS",WS_OVERLAPPEDWINDOW,0,0,160,100,NULL,NULL,NULL,NULL);CHECK(hwnd!=NULL);
    ps2=get_ps(PTR(hwnd));CHECK(ps2!=0);
    window_dc=GetDC(hwnd);CHECK(window_dc!=NULL);ReleaseDC(hwnd,window_dc);
    CHECK(back_mix(ps2,2) && release_ps(ps2));
    CHECK(InvalidateRect(hwnd,NULL,TRUE));ps3=begin_paint(PTR(hwnd),0,NULL);CHECK(ps3!=0);
    CHECK(back_mix(ps3,5) && end_paint(ps3));DestroyWindow(hwnd);
    close_surface(&expected);close_surface(&actual);
    puts("Font match/replacement/lifetime, text baseline, background pixels and CP437/850/1252 PASS");
    return 0;
}
int main(void)
{
    HMODULE win=LoadLibraryA("PMWIN.dll"),gpi=LoadLibraryA("PMGPI.dll"),dos=LoadLibraryA("DOSCALLS.dll");
    CHECK(win && gpi && dos);
    BIND(gpi,create_ps,369);BIND(gpi,destroy_ps,379);BIND(gpi,create_font,368);
    BIND(gpi,set_font,513);BIND(gpi,delete_font,378);BIND(gpi,back_mix,505);
    BIND(gpi,back_color,504);BIND(gpi,text_color,517);BIND(gpi,draw_text,359);
    BIND(gpi,text_box,489);BIND(gpi,width_table,492);
    BIND(win,load_dialog,924);BIND(win,destroy_window,728);BIND(win,send_item,903);
    BIND(win,send_msg,920);BIND(win,query_text,841);BIND(win,get_ps,757);
    BIND(win,release_ps,848);BIND(win,begin_paint,703);BIND(win,end_paint,738);
    BIND(dos,set_cp,289);
    { FARPROC p=GetProcAddress(win,"OS2PM_RegisterResource");CHECK(p!=NULL);memcpy(&register_resource,&p,sizeof(p)); }
    if(combo_test() || font_test()) return 1;
    printf("R8 font and combo native smoke: %d checks PASS\n",checks);
    return 0;
}
