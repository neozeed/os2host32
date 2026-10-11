#ifndef MP_UI_H
#define MP_UI_H

#include <stdio.h>

/*
 * Small seam between the original simulation and the PM front end.
 * Defining MP_PORT_TEST gives a deliberately tiny hosted declaration set for
 * C89 syntax checks; an actual OS/2 build always uses the supplied OS2.H.
 */
#ifdef MP_PORT_TEST
typedef unsigned char BYTE;
typedef unsigned short USHORT;
typedef unsigned long ULONG;
typedef void *HWND;
typedef void *HPS;
typedef void *HDC;
typedef void *HBITMAP;
typedef void *HAB;
typedef void *HMQ;
typedef unsigned int UINT;
typedef int BOOL;
#else
#define INCL_32
#define INCL_WIN
#define INCL_WINWINDOWMGR
#define INCL_WINTIMER
#define INCL_GPI
#define INCL_GPIBITMAPS
#define INCL_DEV
#include <os2.h>
#endif

/* The 1990 beta header set has no NULLHANDLE helper.  All PM handles are
 * LHANDLE (void far *), so this is a correctly typed null handle. */
#ifndef NULLHANDLE
#ifdef MP_PORT_TEST
#define NULLHANDLE 0
#else
#define NULLHANDLE ((LHANDLE)0)
#endif
#endif

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

extern HWND hwndMain;

void mp_ui_message(HWND hwnd, const char *text, const char *caption);
void mp_ui_invalidate(void);
void mp_ui_set_title(const char *title);
USHORT mp_ui_start_timer(HWND hwnd, USHORT id, ULONG milliseconds);
void mp_ui_stop_timer(HWND hwnd, USHORT id);
void mp_debug_string(const char *text);

/* These were diagnostics/format helpers from the NT experiment, not APIs. */
#define wsprintf sprintf
#define OutputDebugString mp_debug_string

#endif

