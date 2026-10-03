/* R9: run against the real PMWIN ordinals on native Windows. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

#define PTR(p) ((DWORD)(ULONG_PTR)(p))
#define CHECK(c) do { ++checks; if(!(c)) { printf("FAIL line %d: %s (Win32=%lu)\n",__LINE__,#c,(unsigned long)GetLastError()); return 1; } } while(0)
#define BIND(n,o) do { FARPROC p=GetProcAddress(module,(LPCSTR)(ULONG_PTR)(o)); CHECK(p!=NULL); memcpy(&n,&p,sizeof(n)); } while(0)
struct Swp { DWORD flags; LONG cy,cx,y,x; DWORD behind,hwnd,reserved1,reserved2; };
struct Rect { LONG left,bottom,right,top; };
static DWORD (__cdecl *set_pos)(DWORD,DWORD,LONG,LONG,LONG,LONG,DWORD);
static DWORD (__cdecl *query_pos)(DWORD,struct Swp *);
static DWORD (__cdecl *query_rect)(DWORD,struct Rect *);
static DWORD (__cdecl *begin_paint)(DWORD,DWORD,struct Rect *);
static DWORD (__cdecl *end_paint)(DWORD);
static int checks;

static int child_test(HWND parent,DWORD style)
{
    HWND child; RECT rect,before; struct Rect pmrect; struct Swp swp;
    DWORD ps; int i;
    const LONG heights[]={288,384,192}; /* 24 rows: 8x12, 8x16, 6x8 */
    const LONG widths[]={640,640,480};
    child=CreateWindowA("STATIC","",WS_CHILD|WS_VISIBLE|style,0,0,10,10,parent,NULL,NULL,NULL);
    CHECK(child!=NULL);
    for(i=0;i<3;++i) {
        CHECK(set_pos(PTR(child),0,13,17,widths[i],heights[i],3));
        CHECK(GetWindowRect(child,&rect));
        CHECK(rect.right-rect.left==widths[i] && rect.bottom-rect.top==heights[i]);
        CHECK(query_pos(PTR(child),&swp));
        CHECK(swp.cx==widths[i] && swp.cy==heights[i] && swp.x==13 && swp.y==17);
        CHECK(query_rect(PTR(child),&pmrect));
        CHECK(GetClientRect(child,&rect));
        CHECK(pmrect.right==rect.right && pmrect.top==rect.bottom);
        if(!style) CHECK(pmrect.right==widths[i] && pmrect.top==heights[i]);
        /* A move-only request ignores even nonsensical cx/cy. */
        CHECK(set_pos(PTR(child),0,23,27,-123,-456,2));
        CHECK(query_pos(PTR(child),&swp));
        CHECK(swp.cx==widths[i] && swp.cy==heights[i] && swp.x==23 && swp.y==27);
        CHECK(GetWindowRect(child,&before));
        /* Size-only must preserve the native position, ignoring x/y. */
        CHECK(set_pos(PTR(child),0,-999,-999,widths[i],heights[i],1));
        CHECK(GetWindowRect(child,&rect));
        CHECK(rect.left==before.left && rect.top==before.top);
        if(!style) {
            CHECK(InvalidateRect(child,NULL,TRUE));
            ps=begin_paint(PTR(child),0,&pmrect);CHECK(ps!=0);
            CHECK(pmrect.left==0 && pmrect.bottom==0);
            CHECK(pmrect.right==widths[i] && pmrect.top==heights[i]);
            CHECK(end_paint(ps));
        }
    }
    CHECK(DestroyWindow(child));return 0;
}

int main(void)
{
    HMODULE module; HWND parent;
    module=LoadLibraryA("PMWIN.dll");CHECK(module!=NULL);
    BIND(set_pos,875);BIND(query_pos,837);BIND(query_rect,840);
    BIND(begin_paint,703);BIND(end_paint,738);
    parent=CreateWindowA("STATIC","R9 geometry smoke",WS_POPUP|WS_VISIBLE,0,0,840,480,NULL,NULL,NULL,NULL);
    CHECK(parent!=NULL);
    if(child_test(parent,0) || child_test(parent,WS_BORDER)) return 1;
    CHECK(DestroyWindow(parent));
    printf("R9 child extents, 24-row paint area, borders and move/size flags: %d checks PASS\n",checks);
    return 0;
}
