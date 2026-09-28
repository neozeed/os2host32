#ifndef MOU_TEST_WINDOWS_H
#define MOU_TEST_WINDOWS_H

#include <stddef.h>
#include <string.h>

typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef short SHORT;
typedef long LONG;
typedef int BOOL;
typedef void *HANDLE;
typedef void *HWND;

#define TRUE 1
#define FALSE 0
#define INVALID_HANDLE_VALUE ((HANDLE)(long)-1)
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define FILE_TYPE_CHAR 2UL
#define ERROR_INVALID_HANDLE 6UL
#define ERROR_INVALID_PARAMETER 87UL
#define VK_LBUTTON 0x01
#define VK_RBUTTON 0x02
#define VK_MBUTTON 0x04
#define SM_CMOUSEBUTTONS 43

#define ZeroMemory(p,n) memset((p),0,(n))

typedef struct _COORD { short X; short Y; } COORD;
typedef struct _SMALL_RECT { short Left; short Top; short Right; short Bottom; } SMALL_RECT;
typedef struct _CONSOLE_SCREEN_BUFFER_INFO {
    COORD dwSize;
    COORD dwCursorPosition;
    WORD wAttributes;
    SMALL_RECT srWindow;
    COORD dwMaximumWindowSize;
} CONSOLE_SCREEN_BUFFER_INFO;
typedef struct _CONSOLE_FONT_INFO { DWORD nFont; COORD dwFontSize; } CONSOLE_FONT_INFO;
typedef struct tagPOINT { LONG x; LONG y; } POINT;

HANDLE GetStdHandle(DWORD which);
DWORD GetFileType(HANDLE handle);
HWND GetConsoleWindow(void);
BOOL GetCursorPos(POINT *point);
BOOL ScreenToClient(HWND hwnd, POINT *point);
BOOL ClientToScreen(HWND hwnd, POINT *point);
BOOL GetConsoleScreenBufferInfo(HANDLE handle, CONSOLE_SCREEN_BUFFER_INFO *info);
BOOL GetCurrentConsoleFont(HANDLE handle, BOOL maximum_window, CONSOLE_FONT_INFO *font);
COORD GetConsoleFontSize(HANDLE handle, DWORD font_index);
SHORT GetAsyncKeyState(int key);
int GetSystemMetrics(int index);
DWORD GetTickCount(void);
void Sleep(DWORD milliseconds);
BOOL SetCursorPos(int x, int y);
DWORD GetLastError(void);

void mou_stub_reset(void);
void mou_stub_set_pointer(unsigned short row, unsigned short col,
                          unsigned short buttons);
void mou_stub_get_pointer(unsigned short *row, unsigned short *col);
unsigned int mou_stub_sleep_count(void);
unsigned int mou_stub_setpos_count(void);

#endif
