#include <string.h>
#include "windows.h"

static int output_token;
static int hwnd_token;
static POINT cursor;
static unsigned short button_state;
static DWORD last_error;
static DWORD tick;
static unsigned int sleeps;
static unsigned int setpos_count;

#define CELL_X 8
#define CELL_Y 16

void mou_stub_reset(void)
{
    cursor.x = 3 * CELL_X + CELL_X / 2;
    cursor.y = 2 * CELL_Y + CELL_Y / 2;
    button_state = 0U;
    last_error = 0UL;
    tick = 1000UL;
    sleeps = 0U;
    setpos_count = 0U;
}

void mou_stub_set_pointer(unsigned short row, unsigned short col,
                          unsigned short buttons)
{
    cursor.x = (LONG)col * CELL_X + CELL_X / 2;
    cursor.y = (LONG)row * CELL_Y + CELL_Y / 2;
    button_state = buttons;
}

void mou_stub_get_pointer(unsigned short *row, unsigned short *col)
{
    if (row != NULL) *row = (unsigned short)(cursor.y / CELL_Y);
    if (col != NULL) *col = (unsigned short)(cursor.x / CELL_X);
}
unsigned int mou_stub_sleep_count(void) { return sleeps; }
unsigned int mou_stub_setpos_count(void) { return setpos_count; }

HANDLE GetStdHandle(DWORD which)
{
    if (which == STD_OUTPUT_HANDLE) return (HANDLE)&output_token;
    return INVALID_HANDLE_VALUE;
}
DWORD GetFileType(HANDLE handle)
{
    return handle == (HANDLE)&output_token ? FILE_TYPE_CHAR : 0UL;
}
HWND GetConsoleWindow(void) { return (HWND)&hwnd_token; }
BOOL GetCursorPos(POINT *point) { *point = cursor; return TRUE; }
BOOL ScreenToClient(HWND hwnd, POINT *point)
{ (void)hwnd; (void)point; return TRUE; }
BOOL ClientToScreen(HWND hwnd, POINT *point)
{ (void)hwnd; (void)point; return TRUE; }
BOOL GetConsoleScreenBufferInfo(HANDLE handle, CONSOLE_SCREEN_BUFFER_INFO *info)
{
    (void)handle;
    memset(info, 0, sizeof(*info));
    info->dwSize.X = 80; info->dwSize.Y = 25;
    info->srWindow.Left = 0; info->srWindow.Top = 0;
    info->srWindow.Right = 79; info->srWindow.Bottom = 24;
    return TRUE;
}
BOOL GetCurrentConsoleFont(HANDLE handle, BOOL maximum_window, CONSOLE_FONT_INFO *font)
{
    (void)handle; (void)maximum_window;
    font->nFont = 1UL;
    font->dwFontSize.X = CELL_X; font->dwFontSize.Y = CELL_Y;
    return TRUE;
}
COORD GetConsoleFontSize(HANDLE handle, DWORD font_index)
{
    COORD c;
    (void)handle; (void)font_index;
    c.X = CELL_X; c.Y = CELL_Y; return c;
}
SHORT GetAsyncKeyState(int key)
{
    unsigned short bit;
    bit = 0U;
    if (key == VK_LBUTTON) bit = 0x0001U;
    else if (key == VK_RBUTTON) bit = 0x0002U;
    else if (key == VK_MBUTTON) bit = 0x0004U;
    return (button_state & bit) != 0U ? (SHORT)0x8000 : 0;
}
int GetSystemMetrics(int index)
{ return index == SM_CMOUSEBUTTONS ? 3 : 0; }
DWORD GetTickCount(void) { return tick++; }
void Sleep(DWORD milliseconds) { (void)milliseconds; ++sleeps; }
BOOL SetCursorPos(int x, int y)
{
    cursor.x = x; cursor.y = y; ++setpos_count; return TRUE;
}
DWORD GetLastError(void) { return last_error; }
