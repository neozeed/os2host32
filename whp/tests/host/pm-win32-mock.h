/* UI test double: NOT a Windows/WHP execution test. */
typedef void *HWND;
typedef void *HDC;
typedef void *HBRUSH;
typedef uint32_t UINT;
typedef int32_t LONG;
typedef intptr_t LONG_PTR;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM;
typedef intptr_t LRESULT;
typedef uint32_t COLORREF;
typedef struct {LONG left,top,right,bottom;} RECT;
typedef struct {void *lpCreateParams;} CREATESTRUCTA;
typedef struct {int unused;} PAINTSTRUCT;
typedef struct {UINT message;WPARAM wParam;LPARAM lParam;} MSG;
typedef struct {UINT style;LRESULT (*lpfnWndProc)(HWND,UINT,WPARAM,LPARAM);void *hInstance;const char *lpszClassName;void *hCursor;} WNDCLASSA;
#define CALLBACK
#define TRUE 1
#define FALSE 0
#define GWL_STYLE (-16)
#define GWL_EXSTYLE (-20)
#define GWLP_USERDATA (-21)
#define WM_NCCREATE 0x81
#define WM_NCDESTROY 0x82
#define WM_PAINT 15
#define WM_ERASEBKGND 20
#define WM_SIZE 5
#define WM_CLOSE 16
#define WM_CHAR 0x102
#define WM_QUIT 18
#define PM_REMOVE 1
#define SW_SHOW 5
#define CS_HREDRAW 2
#define CS_VREDRAW 1
#define WS_CAPTION 0xc00000u
#define WS_SYSMENU 0x80000u
#define WS_THICKFRAME 0x40000u
#define WS_MINIMIZEBOX 0x20000u
#define WS_MAXIMIZEBOX 0x10000u
#define CW_USEDEFAULT ((int)0x80000000u)
#define IDC_ARROW ((const char *)32512)
#define ERROR_CLASS_ALREADY_EXISTS 1410
#define DT_SINGLELINE 0x20
#define DT_NOPREFIX 0x800
#define DT_CENTER 1
#define DT_RIGHT 2
#define DT_VCENTER 4
#define DT_BOTTOM 8
#define TRANSPARENT 1
#define INFINITE 0xffffffffu
#define QS_ALLINPUT 0x4ff
#define MWMO_INPUTAVAILABLE 4
#define WAIT_FAILED 0xffffffffu
#define RGB(r,g,b) ((COLORREF)((r)|((g)<<8)|((b)<<16)))
static int mock_adjust_fail;
static LONG_PTR mock_user;
static WNDCLASSA mock_class;
static MSG mock_msg;
static int mock_pending,mock_live,mock_shows,mock_waits;
static HWND mock_hwnd=(void *)(uintptr_t)1;
static LONG_PTR GetWindowLongPtrA(HWND h,int n){(void)h;return n==GWL_STYLE?WS_CAPTION:n==GWL_EXSTYLE?0:mock_user;}
static LONG_PTR SetWindowLongPtrA(HWND h,int n,LONG_PTR p){(void)h;(void)n;mock_user=p;return 0;}
static LRESULT DefWindowProcA(HWND h,UINT m,WPARAM w,LPARAM l){(void)h;(void)m;(void)w;(void)l;return 0;}
static HDC BeginPaint(HWND h,PAINTSTRUCT *p){(void)p;return h;}
static int EndPaint(HWND h,PAINTSTRUCT *p){(void)h;(void)p;return 1;}
static int PeekMessageA(MSG *m,HWND h,UINT a,UINT b,UINT f){(void)h;(void)a;(void)b;(void)f;if(!mock_pending)return 0;*m=mock_msg;mock_pending=0;return 1;}
static int TranslateMessage(const MSG *m){(void)m;return 1;}
static LRESULT DispatchMessageA(const MSG *m){return mock_class.lpfnWndProc(mock_hwnd,m->message,m->wParam,m->lParam);}
static int ReleaseDC(HWND h,HDC d){(void)h;(void)d;return 1;}
static int DestroyWindow(HWND h){mock_live=0;mock_class.lpfnWndProc(h,WM_NCDESTROY,0,0);return 1;}
static int ShowWindow(HWND h,int s){(void)s;assert(h && mock_live);++mock_shows;return 1;}
static int GetClientRect(HWND h,RECT *r){(void)h;r->left=r->top=0;r->right=640;r->bottom=360;return 1;}
static void *GetModuleHandleA(const char *s){(void)s;return mock_hwnd;}
static void *LoadCursorA(void *a,const char *b){(void)a;(void)b;return mock_hwnd;}
static int RegisterClassA(const WNDCLASSA *w){mock_class=*w;return 1;}
static DWORD GetLastError(void){return 0;}
static HWND CreateWindowExA(DWORD ex,const char *cl,const char *title,DWORD style,int x,int y,int w,int h,HWND parent,void *menu,void *instance,void *param)
{
 CREATESTRUCTA cs; (void)ex;(void)cl;(void)title;(void)style;(void)x;(void)y;(void)w;(void)h;(void)parent;(void)menu;(void)instance;
 cs.lpCreateParams=param;mock_live=1;mock_class.lpfnWndProc(mock_hwnd,WM_NCCREATE,0,(LPARAM)&cs);return mock_hwnd;
}
static HDC GetDC(HWND h){return h;}
static HBRUSH CreateSolidBrush(COLORREF c){(void)c;return mock_hwnd;}
static int FillRect(HDC h,const RECT *r,HBRUSH b){(void)h;(void)r;(void)b;return 1;}
static int DeleteObject(void *o){(void)o;return 1;}
static COLORREF SetTextColor(HDC h,COLORREF c){(void)h;return c;}
static int SetBkMode(HDC h,int mode){(void)h;return mode;}
static int DrawTextA(HDC h,const char *t,int n,RECT *r,UINT f){(void)h;(void)t;(void)n;(void)r;(void)f;return 16;}
static int InvalidateRect(HWND h,const RECT *r,int erase){(void)h;(void)r;(void)erase;mock_msg.message=WM_PAINT;mock_pending=1;return 1;}
static DWORD MsgWaitForMultipleObjectsEx(DWORD n,const void *h,DWORD ms,DWORD mask,DWORD flags)
{
 (void)n;(void)h;(void)mask;(void)flags;++mock_waits;
 if(ms!=INFINITE)clock_ms+=ms;
 else {mock_msg.message=WM_CLOSE;mock_pending=1;}
 return 0;
}

/* Native DIB metadata and pixel capture for PMGPI tests. */
typedef struct {DWORD biSize;LONG biWidth,biHeight;uint16_t biPlanes,biBitCount;DWORD biCompression,biSizeImage;LONG biXPelsPerMeter,biYPelsPerMeter;DWORD biClrUsed,biClrImportant;} BITMAPINFOHEADER;
typedef struct {BITMAPINFOHEADER bmiHeader;DWORD colors[1];} BITMAPINFO;
#define BI_RGB 0
#define COLORONCOLOR 3
#define DIB_RGB_COLORS 0
#define SRCCOPY 0xcc0020u
#define GDI_ERROR (-1)
static uint32_t mock_dib_top,mock_dib_bottom;
static int mock_dib_x,mock_dib_y,mock_dib_w,mock_dib_h;
static int SetStretchBltMode(HDC dc,int mode){(void)dc;return mode;}
static int StretchDIBits(HDC dc,int x,int y,int w,int h,int sx,int sy,int sw,int sh,const void *bits,const BITMAPINFO *bi,UINT usage,DWORD rop)
{
 const uint32_t *p=bits;(void)dc;(void)sx;(void)sy;(void)usage;
 assert(rop==SRCCOPY && bi->bmiHeader.biWidth==sw && bi->bmiHeader.biHeight==-sh);
 assert(bi->bmiHeader.biBitCount==32 && bi->bmiHeader.biSize==40);
 mock_dib_top=p[0];mock_dib_bottom=p[(sh-1)*sw];
 mock_dib_x=x;mock_dib_y=y;mock_dib_w=w;mock_dib_h=h;return sh;
}

#define WM_KEYDOWN 0x100
#define WM_KEYUP 0x101
#define VK_F1 0x70
#define VK_F12 0x7b
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_HOME 0x24
#define VK_END 0x23
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_INSERT 0x2d
#define VK_DELETE 0x2e
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_MENU 0x12
#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#define SM_CXSIZEFRAME 32
#define SM_CYSIZEFRAME 33
#define SM_CXBORDER 5
#define SM_CYBORDER 6
#define SM_CYCAPTION 4
#define SWP_NOZORDER 4u
#define SWP_NOACTIVATE 16u
#define SWP_NOSIZE 1u
#define SWP_NOMOVE 2u
#define SWP_SHOWWINDOW 64u
#define SWP_HIDEWINDOW 128u
#define SWP_NOREDRAW 8u
static HWND mock_focus;
static int mock_pos_x,mock_pos_y,mock_pos_w,mock_pos_h;
static short GetKeyState(int key){(void)key;return 0;}
static int GetSystemMetrics(int key){return key==SM_CXSCREEN?1920:key==SM_CYSCREEN?1080:key==SM_CYCAPTION?23:1;}
static HWND SetFocus(HWND h){mock_focus=h;return h;}
static HWND GetFocus(void){return mock_focus;}
static int GetWindowRect(HWND h,RECT *r){return GetClientRect(h,r);}
static int SetWindowPos(HWND h,HWND after,int x,int y,int w,int height,UINT flags)
{(void)h;(void)after;(void)flags;mock_pos_x=x;mock_pos_y=y;mock_pos_w=w;mock_pos_h=height;return 1;}
#define WINAPI
 typedef void *LPVOID;
 typedef void *HANDLE;
static DWORD mock_beep_frequency,mock_beep_duration;
static LONG InterlockedIncrement(volatile LONG *v){return ++*v;}
static LONG InterlockedDecrement(volatile LONG *v){return --*v;}
static int Beep(DWORD frequency,DWORD duration){mock_beep_frequency=frequency;mock_beep_duration=duration;return 1;}
static HANDLE CreateThread(void *a,size_t b,DWORD (*fn)(LPVOID),LPVOID arg,DWORD flags,DWORD *tid)
{(void)a;(void)b;(void)flags;(void)tid;fn(arg);return mock_hwnd;}
static int CloseHandle(HANDLE h){(void)h;return 1;}

/* Model a captioned frame with nonzero borders: 8 wide, 31 tall. */
static int AdjustWindowRectEx(RECT *r,DWORD style,int menu,DWORD exstyle)
{assert(style==WS_CAPTION && !menu && !exstyle);if(mock_adjust_fail)return 0;
 r->left-=4;r->right+=4;r->top-=27;r->bottom+=4;return 1;}
