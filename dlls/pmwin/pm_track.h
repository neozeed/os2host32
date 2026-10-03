/* 32-bit GA TRACKINFO (76 bytes); separate from beta SDK's SHORT layout. */
#include "../../common/include/os2_track.h"
static void track_draw(HDC dc,const struct O2TrackRect *r,LONG height,LONG bx,LONG by)
{
    LONG w=r->right-r->left,h=r->top-r->bottom,y=height-r->top;
    if(w<=0 || h<=0) return;
    if(w==1 || h==1) { PatBlt(dc,r->left,y,w,h,DSTINVERT);return; }
    if(bx<1) bx=1;
    if(by<1) by=1;
    if(bx>w/2) bx=w/2;
    if(by>h/2) by=h/2;
    PatBlt(dc,r->left,y,w,by,DSTINVERT);
    if(h>by) PatBlt(dc,r->left,y+h-by,w,by,DSTINVERT);
    if(h>2*by) {
        PatBlt(dc,r->left,y+by,bx,h-2*by,DSTINVERT);
        if(w>bx) PatBlt(dc,r->right-bx,y+by,bx,h-2*by,DSTINVERT);
    }
}
O2ULONG __cdecl WinTrackRect(O2HWND hwnd,O2HPS hps,struct O2TrackInfo *t)
{
    HWND wh=native_hwnd(hwnd),capture,old_capture;HDC dc;MSG msg;RECT cr;
    POINT start,p;struct O2TrackRect current;int accepted=0,owned_dc=0,temporary=0;
    LONG dx=0,dy=0,height;DWORD pid;struct CompatPS *ps;
    if(!os2_track_valid(t)) return pm_api_error(0x1006UL);
    if(!wh || !IsWindow(wh)) return pm_api_error(0x1001UL);
    GetWindowThreadProcessId(wh,&pid);
    if(wh!=GetDesktopWindow() && pid!=GetCurrentProcessId()) return pm_api_error(0x1001UL);
    if(!GetClientRect(wh,&cr) || !GetCursorPos(&start)) return 0;
    height=cr.bottom;
    if(t->flags&0x10U) {
        start.x=(t->flags&4)?t->rect.right:t->rect.left;
        start.y=height-((t->flags&2)?t->rect.top:t->rect.bottom);
        ClientToScreen(wh,&start);SetCursorPos(start.x,start.y);
    }
    ScreenToClient(wh,&start);current=t->rect;
    ps=(struct CompatPS *)(ULONG_PTR)hps;
    if(ps && ps->magic!=PMCOMPAT_PS_MAGIC) return pm_api_error(0x1003UL);
    dc=ps?ps->dc:GetDC(wh);owned_dc=ps==NULL;if(!dc) return 0;
    capture=wh;
    if(wh==GetDesktopWindow()) {
        capture=CreateWindowExA(WS_EX_TOOLWINDOW,"STATIC","",WS_POPUP,0,0,0,0,NULL,NULL,GetModuleHandleA(NULL),NULL);
        temporary=1;
    }
    if(!capture) { if(owned_dc) ReleaseDC(wh,dc);return 0; }
    old_capture=GetCapture();SetCapture(capture);
    if(GetCapture()!=capture) { if(temporary) DestroyWindow(capture);if(owned_dc) ReleaseDC(wh,dc);return 0; }
    track_draw(dc,&current,height,t->border_x,t->border_y);
    for(;;) {
        int got=GetMessageA(&msg,NULL,0,0);
        track_draw(dc,&current,height,t->border_x,t->border_y);
        if(got<=0) { if(!got) PostQuitMessage((int)msg.wParam);break; }
        if(msg.message==WM_KEYDOWN && (msg.wParam==VK_ESCAPE || msg.wParam==VK_RETURN)) {
            accepted=msg.wParam==VK_RETURN;break;
        }
        if(msg.message==WM_RBUTTONDOWN || GetCapture()!=capture) break;
        if(msg.message==WM_LBUTTONUP) { accepted=1;break; }
        if(msg.message==WM_MOUSEMOVE) {
            GetCursorPos(&p);ScreenToClient(wh,&p);dx=p.x-start.x;dy=start.y-p.y;
        } else if(msg.message==WM_KEYDOWN) {
            LONG kx=t->key_x>0?t->key_x:1,ky=t->key_y>0?t->key_y:1;
            if(msg.wParam==VK_LEFT && dx>=INT32_MIN+kx) dx-=kx;
            if(msg.wParam==VK_RIGHT && dx<=INT32_MAX-kx) dx+=kx;
            if(msg.wParam==VK_DOWN && dy>=INT32_MIN+ky) dy-=ky;
            if(msg.wParam==VK_UP && dy<=INT32_MAX-ky) dy+=ky;
        } else { TranslateMessage(&msg);DispatchMessageA(&msg); }
        os2_track_step(t,dx,dy,&current);
        track_draw(dc,&current,height,t->border_x,t->border_y);
    }
    if(GetCapture()==capture) ReleaseCapture();
    if(old_capture && IsWindow(old_capture)) SetCapture(old_capture);
    if(temporary) DestroyWindow(capture);
    if(owned_dc) ReleaseDC(wh,dc);
    if(accepted) t->rect=current;
    return accepted?1:0;
}
