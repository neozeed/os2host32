#ifndef MP_PORT_H
#define MP_PORT_H
#include "mp_ui.h"
#include <stdlib.h>
#include <time.h>
struct timeval { long tv_sec, tv_usec; };
typedef unsigned long DWORD;
typedef unsigned long COLORREF;
typedef struct { long left, top, right, bottom; } RECT;
typedef void *HGLOBAL;
#define RGB(r,g,b) ((COLORREF)(r) | ((COLORREF)(g)<<8) | ((COLORREF)(b)<<16))
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#define InvalidateRect(h,r,e) mp_ui_invalidate()
unsigned long mp_ui_ticks(void);
#define GetTickCount() mp_ui_ticks()
#define KillTimer(h,i) mp_ui_stop_timer((h),(USHORT)(i))
#define SetTimer(h,i,t,p) mp_ui_start_timer((h),(USHORT)(i),(ULONG)(t))
#define MessageBox(h,t,c,f) mp_ui_message((h),(t),(c))
#define SetWindowText(h,t) mp_ui_set_title(t)
#ifndef MP_PORT_TEST
#define MB_ICONERROR MB_ERROR
#endif
#define CALLBACK
typedef long LRESULT;
typedef unsigned long WPARAM;
typedef long LPARAM;
int ShowBudgetWindowAndWait(HWND hwnd);
#endif

