/* R10: one PM owner thread, queue, class and standard frame/client pair.
 * Native WndProc NEVER enters guest code. It queues translated PM messages.
 * Guest callbacks execute only in the normal WHP run loop.
 */
#define PM_HAB 0x51000001u
#define PM_HMQ 0x52000001u
#define PM_FRAME 0x50000001u
#define PM_CLIENT 0x50000002u
#define PM_WINDOW_DC 0x55000001u
#define PM_HPS 0x53000001u
#define PM_CAPACITY 128u
struct PMMessage { uint32_t hwnd,msg,mp1,mp2,time; };
struct GuestPM {
    uint32_t tid,proc,head,count,client_out,paint_hps;
    uint32_t initialized[MAX_THREADS];
    int queue,creating,visible,destroying,fault;
    char class_name[128];
    HWND window;
    HDC paint_dc;
    struct PMMessage messages[PM_CAPACITY];
};
static int pm_enqueue(struct Runtime *rt,uint32_t hwnd,uint32_t msg,uint32_t a,uint32_t b)
{
    struct GuestPM *p=rt->pm;
    struct PMMessage *m;
    uint32_t i;
    if (!p || !p->queue) return 0;
    if (msg==0x23 || msg==7 || msg==0x29) {
        for(i=0;i<p->count;i++) {
            m=&p->messages[(p->head+i)%PM_CAPACITY];
            if(m->hwnd==hwnd && m->msg==msg) {m->mp1=a;m->mp2=b;return 1;}
        }
    }
    if(p->count==PM_CAPACITY) return 0;
    m=&p->messages[(p->head+p->count++)%PM_CAPACITY];
    m->hwnd=hwnd;m->msg=msg;m->mp1=a;m->mp2=b;m->time=(uint32_t)GetTickCount64();
    return 1;
}
/* OS/2 VK values differ from Win32. Printable/control characters arrive
   through WM_CHAR; only non-character keys are translated at KEYDOWN/UP. */
static uint32_t pm_virtual_key(WPARAM key)
{
    if(key>=VK_F1 && key<=VK_F12)return 0x20u+(uint32_t)(key-VK_F1);
    switch(key) {
    case VK_LEFT:return 0x15;case VK_UP:return 0x16;
    case VK_RIGHT:return 0x17;case VK_DOWN:return 0x18;
    case VK_HOME:return 0x11;case VK_END:return 0x12;
    case VK_PRIOR:return 0x13;case VK_NEXT:return 0x14;
    case VK_INSERT:return 0x1a;case VK_DELETE:return 0x1b;
    default:return 0;
    }
}
static uint32_t pm_hab_for(struct GuestPM *p,uint32_t tid)
{
    return tid==p->tid ? PM_HAB : 0x54000000u|tid;
}
static LRESULT CALLBACK pm_native_proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp)
{
    struct Runtime *rt=(struct Runtime *)GetWindowLongPtrA(hwnd,GWLP_USERDATA);
    uint32_t pmmsg=0,a=0,b=0;
    if(msg==WM_NCCREATE) {
        rt=(struct Runtime *)((CREATESTRUCTA *)lp)->lpCreateParams;
        SetWindowLongPtrA(hwnd,GWLP_USERDATA,(LONG_PTR)rt);
        return DefWindowProcA(hwnd,msg,wp,lp);
    }
    if(!rt || !rt->pm) return DefWindowProcA(hwnd,msg,wp,lp);
    switch(msg) {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(hwnd,&ps);EndPaint(hwnd,&ps);
        pmmsg=0x23;break;
    }
    case WM_ERASEBKGND: return 1;
    case WM_SIZE: pmmsg=7;b=(uint32_t)lp;break;
    case WM_CLOSE: pmmsg=0x29;break;
    case WM_KEYDOWN: case WM_KEYUP:
        b=pm_virtual_key(wp);if(!b)return DefWindowProcA(hwnd,msg,wp,lp);
        b<<=16;a=2u|((msg==WM_KEYUP)?0x40u:0u)|((uint32_t)lp&255u)<<16;
        a|=((uint32_t)lp&0xff0000u)<<8;pmmsg=0x7a;break;
    case WM_CHAR:
        pmmsg=0x7a;a=1u|(((uint32_t)lp&255u)<<16);b=(uint32_t)wp&0xffffu;
        if(wp==27){a|=2;b|=0x0fu<<16;}
        else if(wp==13){a|=2;b|=0x1eu<<16;}
        else if(wp==8){a|=2;b|=5u<<16;}
        break;
    case WM_NCDESTROY:
        rt->pm->window=NULL;
        SetWindowLongPtrA(hwnd,GWLP_USERDATA,0);
        return DefWindowProcA(hwnd,msg,wp,lp);
    default: return DefWindowProcA(hwnd,msg,wp,lp);
    }
    if(pmmsg==0x7a) {
        if(GetKeyState(VK_SHIFT)<0)a|=8;
        if(GetKeyState(VK_CONTROL)<0)a|=0x10;
        if(GetKeyState(VK_MENU)<0)a|=0x20;
    }
    if(pmmsg && !rt->pm->destroying && !pm_enqueue(rt,PM_CLIENT,pmmsg,a,b))
        rt->pm->fault=1;
    return 0;
}
/* Beta2 QMSG: HWND+0, USHORT msg+4, padding+6, mp1+8, mp2+12,
 * time+16, POINTL+20. No host MSG/HWND layout is exposed. */
static uint32_t pm_take(struct Runtime *rt,uint32_t output)
{
    struct GuestPM *p=rt->pm;
    struct PMMessage m=p->messages[p->head];
    p->head=(p->head+1u)%PM_CAPACITY;--p->count;
    memset(rt->ram+output,0,28);
    guest_put_u32(rt,output,m.hwnd);guest_put_u32(rt,output+4,m.msg & 0xffffu);
    guest_put_u32(rt,output+8,m.mp1);guest_put_u32(rt,output+12,m.mp2);
    guest_put_u32(rt,output+16,m.time);
    return m.msg!=0x2au;
}
static int pm_waiting(struct Runtime *rt)
{
    struct GuestThread *t;
    if(!rt->pm || !rt->pm->queue) return 0;
    t=find_thread(rt,rt->pm->tid);
    return t && t->state==THREAD_WAIT_PM;
}
static void pm_pump(struct Runtime *rt)
{
    MSG msg;
    struct GuestThread *t;
    if(!rt->pm) return;
    while(PeekMessageA(&msg,NULL,0,0,PM_REMOVE)) {
        if(msg.message==WM_QUIT) {
            if(!pm_enqueue(rt,0,0x2a,0,0)) rt->pm->fault=1;
        } else { TranslateMessage(&msg);DispatchMessageA(&msg); }
    }
    if(rt->pm->fault) {
        fprintf(stderr,"v2: PM native message queue overflow\n");
        rt->process_exited=1;rt->process_rc=1;return;
    }
    t=find_thread(rt,rt->pm->tid);
    if(t && t->state==THREAD_WAIT_PM && rt->pm->count)
        sync_ready(t,pm_take(rt,t->queue_args[0]));
}
static void pm_destroy_native(struct Runtime *rt)
{
    struct GuestPM *p=rt->pm;
    if(!p) return;
    p->destroying=1;
    if(p->paint_dc) {ReleaseDC(p->window,p->paint_dc);p->paint_dc=NULL;}
    if(p->window) DestroyWindow(p->window);
    p->window=NULL;p->head=p->count=0;
}
static void pm_cleanup(struct Runtime *rt)
{
    gpi_cleanup(rt);pm_destroy_native(rt);free(rt->pm);rt->pm=NULL;
}
static uint32_t pm_created(struct Runtime *rt,uint32_t result)
{
    rt->pm->creating=0;
    if(result || !rt->pm->window) {
        guest_put_u32(rt,rt->pm->client_out,0);pm_destroy_native(rt);return 0;
    }
    if(rt->pm->visible) ShowWindow(rt->pm->window,SW_SHOW);
    return PM_FRAME;
}
static uint32_t pm_destroyed(struct Runtime *rt,uint32_t result)
{
    (void)result;pm_destroy_native(rt);return 1;
}
static uint32_t pm_call(struct Runtime *rt,uint32_t hwnd,uint32_t msg,uint32_t a,uint32_t b,
                       uint32_t resume,int *entered)
{
    uint32_t args[4],rc;
    args[0]=hwnd;args[1]=msg;args[2]=a;args[3]=b;
    rc=begin_guest_callback(rt,rt->pm->proc,args,4,0,resume);
    if(rc) {fprintf(stderr,"v2: PM callback launch failed rc=%u\n",rc);return 0;}
    *entered=1;return 0;
}
static int pm_rect(struct Runtime *rt,uint32_t address,RECT *r)
{
    RECT client;
    int64_t top,bottom;
    if(!address || !guest_range(address,16) || !rt->pm->window ||
       !GetClientRect(rt->pm->window,&client)) return 0;
    r->left=(LONG)guest_u32(rt,address);r->right=(LONG)guest_u32(rt,address+8);
    bottom=(int64_t)client.bottom-(int32_t)guest_u32(rt,address+4);
    top=(int64_t)client.bottom-(int32_t)guest_u32(rt,address+12);
    if(top<INT32_MIN || top>INT32_MAX || bottom<INT32_MIN || bottom>INT32_MAX) return 0;
    r->top=(LONG)top;r->bottom=(LONG)bottom;
    return r->right>=r->left && r->bottom>=r->top;
}
static COLORREF pm_color(uint32_t value)
{
    static const COLORREF palette[16]={
        RGB(255,255,255),RGB(0,0,255),RGB(255,0,0),RGB(255,0,255),
        RGB(0,255,0),RGB(0,255,255),RGB(255,255,0),RGB(0,0,0),
        RGB(128,128,128),RGB(0,0,128),RGB(128,0,0),RGB(128,0,128),
        RGB(0,128,0),RGB(0,128,128),RGB(128,128,0),RGB(192,192,192)};
    if(value==0xfffffffeu) return RGB(255,255,255);
    if(value>=16u) return RGB(0,0,0);
    return palette[value];
}
static uint32_t dispatch_pm(struct Runtime *rt,uint32_t ordinal,uint32_t esp,
                           uint32_t resume,int *entered)
{
    uint32_t a[9],i,n;
    struct GuestPM *p=rt->pm;
    struct GuestThread *t=current_guest_thread(rt);
    RECT r;
    n=ordinal==908 ? 9u : ((ordinal==913 || ordinal==875) ? 7u :
      ((ordinal==926 || ordinal==915) ? 5u :
      ((ordinal==911 || ordinal==919 || ordinal==920 || ordinal==902) ? 4u :
      ((ordinal==703 || ordinal==743 || ordinal==765 || ordinal==746) ? 3u :
      ((ordinal==716 || ordinal==912 || ordinal==840 || ordinal==829) ? 2u : 1u)))));
    if(!guest_range(esp,(n+1u)*4u) || !t) return 0;
    memset(a,0,sizeof(a));for(i=0;i<n;i++)a[i]=guest_u32(rt,esp+4u+i*4u);
    fprintf(stderr,"v2: PMWIN.%u(%08X,%08X,%08X,%08X)\n",ordinal,a[0],a[1],a[2],a[3]);
    if(ordinal==763) {
        if(a[0])return 0;
        if(p){p->initialized[rt->current_thread]=t->tid;return pm_hab_for(p,t->tid);}
        p=(struct GuestPM *)calloc(1,sizeof(*p));if(!p)return 0;
        rt->pm=p;p->tid=t->tid;p->initialized[rt->current_thread]=t->tid;return PM_HAB;
    }
    if(!p)return 0;
    if(t->tid!=p->tid) {
        if(ordinal==888 && a[0]==pm_hab_for(p,t->tid) && p->initialized[rt->current_thread]==t->tid) {
            p->initialized[rt->current_thread]=0;return 1;
        }
        if(ordinal==919 && p->queue && a[0]==PM_CLIENT)
            return pm_enqueue(rt,a[0],a[1]&0xffffu,a[2],a[3]);
        if(ordinal==902 && p->queue && a[0]==PM_HMQ)
            return pm_enqueue(rt,0,a[1]&0xffffu,a[2],a[3]);
        return 0;
    }
    switch(ordinal) {
    case 794: return a[0]==PM_CLIENT && p->window ? PM_WINDOW_DC : 0;
    case 829:
        if(a[0]!=1)return 0;
        switch((int16_t)a[1]) {
        case 20:return (uint32_t)GetSystemMetrics(SM_CXSCREEN);
        case 21:return (uint32_t)GetSystemMetrics(SM_CYSCREEN);
        case 4:return (uint32_t)GetSystemMetrics(SM_CXSIZEFRAME);
        case 5:return (uint32_t)GetSystemMetrics(SM_CYSIZEFRAME);
        case 26:return (uint32_t)GetSystemMetrics(SM_CXBORDER);
        case 27:return (uint32_t)GetSystemMetrics(SM_CYBORDER);
        case 30:return (uint32_t)GetSystemMetrics(SM_CYCAPTION);
        default:fprintf(stderr,"v2: unsupported WinQuerySysValue %u\n",a[1]);return 0;
        }
    case 746:
        if(a[0]!=1 || a[1]!=PM_CLIENT || !p->window || a[2])return 0;
        SetFocus(p->window);return GetFocus()==p->window;
    case 875:
    {
        UINT flags=SWP_NOZORDER|SWP_NOACTIVATE;RECT bounds;
        int x=(int16_t)a[2],y=(int16_t)a[3],w=(int16_t)a[4],h=(int16_t)a[5];
        if((a[0]!=PM_FRAME && a[0]!=PM_CLIENT) || !p->window || a[1] || (a[6]&~0xfbu))return 0;
        if(!(a[6]&1))flags|=SWP_NOSIZE;else if(w<=0 || h<=0)return 0;
        /* Sarien's legacy port specifies client width and height+title-2.
           Keep this application convention opt-in; ordinary PM frame sizes
           continue to describe the native outer rectangle. */
        if((a[6]&1) && a[0]==PM_FRAME) {
            const char *compat=getenv("WHP_OS2_SARIEN_GEOMETRY");
            if(compat && !strcmp(compat,"1")) {
                int client_h=h-GetSystemMetrics(SM_CYCAPTION)+2;
                DWORD style=(DWORD)GetWindowLongPtrA(p->window,GWL_STYLE);
                if(client_h<=0)return 0;
                bounds.left=0;bounds.top=0;bounds.right=w;bounds.bottom=client_h;
                if(!AdjustWindowRectEx(&bounds,style,FALSE,
                    (DWORD)GetWindowLongPtrA(p->window,GWL_EXSTYLE)))return 0;
                w=bounds.right-bounds.left;h=bounds.bottom-bounds.top;
                fprintf(stderr,"v2: Sarien geometry: client=%ux%d native frame=%dx%d\n",
                        (unsigned)(uint16_t)a[4],client_h,w,h);
            }
        }
        if(!(a[6]&2))flags|=SWP_NOMOVE;
        if(a[6]&8)flags|=SWP_SHOWWINDOW;
        if(a[6]&16)flags|=SWP_HIDEWINDOW;
        if(a[6]&32)flags|=SWP_NOREDRAW;
        if(a[6]&128)flags&=~SWP_NOACTIVATE;
        if(!(a[6]&1)){if(!GetWindowRect(p->window,&bounds))return 0;h=bounds.bottom-bounds.top;}
        y=GetSystemMetrics(SM_CYSCREEN)-y-h;
        return SetWindowPos(p->window,NULL,x,y,w,h,flags)!=0;
    }
    case 716: if(a[0]!=PM_HAB || p->queue)return 0;p->queue=1;return PM_HMQ;
    case 726: if(a[0]!=PM_HMQ || p->window)return 0;p->queue=0;p->count=0;return 1;
    case 888: if(a[0]!=PM_HAB || p->queue || p->window)return 0;pm_cleanup(rt);return 1;
    case 926:
        if(a[0]!=PM_HAB || !p->queue || p->proc || !a[2] || !guest_range(a[2],1) || a[4])return 0;
        if(!guest_copy_cstr(rt,a[1],p->class_name,sizeof(p->class_name)) || !p->class_name[0])return 0;
        p->proc=a[2];return 1;
    case 908:
    {
        WNDCLASSA wc;char cls[128],title[256];uint32_t flags;DWORD style=0;
        if(!p->queue || !p->proc || p->window || a[0]!=1u || !a[2] || !guest_range(a[2],4) ||
           !a[8] || !guest_range(a[8],4) || a[6] || a[7])return 0;
        if(!guest_copy_cstr(rt,a[3],cls,sizeof(cls)) || strcmp(cls,p->class_name) ||
           !guest_copy_cstr(rt,a[4],title,sizeof(title)))return 0;
        guest_put_u32(rt,a[8],0);
        flags=guest_u32(rt,a[2]);
        if(flags & ~0x00000c3bu) {fprintf(stderr,"v2: unsupported frame flags %08X\n",flags);return 0;}
        if(flags&1u)style|=WS_CAPTION;
        if(flags&2u)style|=WS_SYSMENU;
        if(flags&8u)style|=WS_THICKFRAME;
        if(flags&0x10u)style|=WS_MINIMIZEBOX;
        if(flags&0x20u)style|=WS_MAXIMIZEBOX;
        memset(&wc,0,sizeof(wc));wc.lpfnWndProc=pm_native_proc;
        wc.style=CS_HREDRAW|CS_VREDRAW;
        wc.hInstance=GetModuleHandleA(NULL);wc.lpszClassName="WHP_OS2_R10";
        wc.hCursor=LoadCursorA(NULL,IDC_ARROW);
        if(!RegisterClassA(&wc) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return 0;
        p->destroying=0;p->creating=1;p->visible=(a[1]&0x80000000u)!=0;
        p->window=CreateWindowExA(0,wc.lpszClassName,title,style,
                   CW_USEDEFAULT,CW_USEDEFAULT,640,360,NULL,NULL,wc.hInstance,rt);
        if(!p->window){p->creating=0;return 0;}
        p->client_out=a[8];guest_put_u32(rt,a[8],PM_CLIENT);
        pm_call(rt,PM_CLIENT,1,0,0,resume,entered);
        if(!*entered){guest_put_u32(rt,a[8],0);p->creating=0;pm_destroy_native(rt);return 0;}
        t->callbacks->complete=pm_created;return 0;
    }
    case 915:
        if(a[0]!=PM_HAB || !p->queue || !a[1] || !guest_range(a[1],28) || a[2] || a[3] || a[4])return 0;
        if(p->count)return pm_take(rt,a[1]);
        t->queue_args[0]=a[1];sync_block(rt,THREAD_WAIT_PM,PM_HMQ,SEM_INDEFINITE_WAIT);return 0;
    case 912:
        if(a[0]!=PM_HAB || !a[1] || !guest_range(a[1],28) || !p->window || guest_u32(rt,a[1])!=PM_CLIENT)return 0;
        return pm_call(rt,PM_CLIENT,guest_u32(rt,a[1]+4)&0xffffu,guest_u32(rt,a[1]+8),guest_u32(rt,a[1]+12),resume,entered);
    case 920:
        if(a[0]!=PM_CLIENT || !p->window)return 0;
        return pm_call(rt,a[0],a[1]&0xffffu,a[2],a[3],resume,entered);
    case 919: case 902:
        if(!p->queue || (ordinal==902 ? a[0]!=PM_HMQ : a[0]!=PM_CLIENT))return 0;
        return pm_enqueue(rt,ordinal==902 ? 0 : a[0],a[1]&0xffffu,a[2],a[3]);
    case 911:
        if(a[0]==PM_CLIENT && (a[1]&0xffffu)==0x29)return pm_enqueue(rt,0,0x2a,0,0);
        return 0;
    case 728:
        if((a[0]!=PM_FRAME && a[0]!=PM_CLIENT) || !p->window || p->destroying || p->creating)return 0;
        pm_call(rt,PM_CLIENT,2,0,0,resume,entered);
        if(*entered){p->destroying=1;t->callbacks->complete=pm_destroyed;}return 0;
    case 840:
        if(a[0]!=PM_CLIENT || !a[1] || !guest_range(a[1],16) || !p->window || !GetClientRect(p->window,&r))return 0;
        guest_put_u32(rt,a[1],0);guest_put_u32(rt,a[1]+4,0);
        guest_put_u32(rt,a[1]+8,(uint32_t)r.right);guest_put_u32(rt,a[1]+12,(uint32_t)r.bottom);return 1;
    case 703:
        if(a[0]!=PM_CLIENT || (a[1] && !gpi_window_ps(rt,a[1])) || !p->window || p->paint_dc || (a[2] && !guest_range(a[2],16)))return 0;
        p->paint_dc=GetDC(p->window);if(!p->paint_dc)return 0;
        if(a[2] && GetClientRect(p->window,&r)) {
            guest_put_u32(rt,a[2],0);guest_put_u32(rt,a[2]+4,0);
            guest_put_u32(rt,a[2]+8,(uint32_t)r.right);guest_put_u32(rt,a[2]+12,(uint32_t)r.bottom);
        }
        p->paint_hps=a[1]?a[1]:PM_HPS;return p->paint_hps;
    case 738:
        if(a[0]!=p->paint_hps || !p->paint_dc)return 0;
        ReleaseDC(p->window,p->paint_dc);p->paint_dc=NULL;p->paint_hps=0;return 1;
    case 743:
    {
        HBRUSH brush;int ok;
        if(a[0]!=PM_HPS || !p->paint_dc || !pm_rect(rt,a[1],&r))return 0;
        brush=CreateSolidBrush(pm_color(a[2]));if(!brush)return 0;
        ok=FillRect(p->paint_dc,&r,brush);DeleteObject(brush);return ok!=0;
    }
    case 913:
    {
        char text[1024];int len;UINT flags=DT_SINGLELINE|DT_NOPREFIX;
        if(a[0]!=PM_HPS || !p->paint_dc || !pm_rect(rt,a[3],&r))return 0;
        len=(int16_t)(a[1]&0xffffu); /* supplied Beta2 API uses SHORT */
        if(len==-1){if(!guest_copy_cstr(rt,a[2],text,sizeof(text)))return 0;len=(int)strlen(text);}
        else {if(len<0 || len>1023 || !a[2] || !guest_range(a[2],(uint32_t)len))return 0;memcpy(text,rt->ram+a[2],(size_t)len);text[len]=0;}
        if(a[6]&~0x8f00u)return 0;
        if(a[6]&0x100)flags|=DT_CENTER;else if(a[6]&0x200)flags|=DT_RIGHT;
        if(a[6]&0x400)flags|=DT_VCENTER;else if(a[6]&0x800)flags|=DT_BOTTOM;
        SetTextColor(p->paint_dc,pm_color(a[4]));SetBkMode(p->paint_dc,TRANSPARENT);
        if(a[6]&0x8000){HBRUSH b=CreateSolidBrush(pm_color(a[5]));if(!b)return 0;FillRect(p->paint_dc,&r,b);DeleteObject(b);}
        return DrawTextA(p->paint_dc,text,len,&r,flags)>0 ? (uint32_t)len : 0;
    }
    case 765:
        if(a[0]!=PM_CLIENT || !p->window)return 0;
        if(a[1] && !pm_rect(rt,a[1],&r))return 0;
        return InvalidateRect(p->window,a[1]?&r:NULL,a[2]!=0)!=0;
    default: fprintf(stderr,"v2: unsupported PMWIN.%u\n",ordinal);return 0;
    }
}
