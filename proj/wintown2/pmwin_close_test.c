#include <assert.h>
#include <stdio.h>
typedef long LRESULT;
typedef unsigned long DWORD;
#define WM_CLOSE 16
#define O2_WM_CLOSE 41
static int mode, visible, alive, quits, destroys;
static void pm_trace(const char *s,unsigned long a,unsigned long b,unsigned long c)
{ (void)s;(void)a;(void)b;(void)c; }
static long call_guest(int hwnd,int msg,int a,int b)
{
    (void)hwnd;(void)a;(void)b; assert(msg==O2_WM_CLOSE);
    if(mode==0)visible=0;
    if(mode==1){alive=0;++destroys;}
    if(mode==2)++quits;
    return 0;
}
static long dispatch(int hwnd,int msg)
{
    switch(msg){
    case WM_CLOSE:
        pm_trace("WM_CLOSE -> guest", (unsigned long)(DWORD)hwnd, 0, 0);
        /* The guest owns close policy. It may hide this window for later
         * reuse, ignore the request, destroy it, or end its message loop.
         * A zero WM_CLOSE result is not permission for native destruction. */
        return (LRESULT)call_guest(hwnd, O2_WM_CLOSE, 0, 0);


    default:return -1;
    }
}
int main(void)
{
    for(mode=0;mode<4;++mode){
        alive=visible=1; quits=destroys=0;
        assert(dispatch(1,WM_CLOSE)==0);
        assert(alive==(mode!=1)); assert(destroys==(mode==1));
        assert(quits==(mode==2));
        if(mode==0){assert(!visible);visible=1;assert(alive&&visible);}
    }
    puts("PASS actual PMWIN close branch: hide/reopen, guest destroy, quit, ignore");
    return 0;
}
