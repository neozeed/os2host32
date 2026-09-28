#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string.h>

#include "os2_mou.h"
#include "os2_mou_backend.h"
#include "os2_mou_win32.h"

#ifndef SM_CMOUSEBUTTONS
#define SM_CMOUSEBUTTONS 43
#endif

struct Os2MouWin32Context {
    int active;
    unsigned short last_row;
    unsigned short last_col;
    unsigned short last_buttons;
    int have_sample;
};

static struct Os2MouWin32Context mou_context;
static struct Os2MouSession mou_session_storage;
static int mou_session_ready;

static Os2MouApiRet mouse_host_error(void)
{
    DWORD e;
    e = GetLastError();
    if (e == ERROR_INVALID_HANDLE)
        return OS2_MOU_ERROR_MOUSE_NO_DEVICE;
    if (e == ERROR_INVALID_PARAMETER)
        return OS2_MOU_ERROR_MOUSE_INV_PARMS;
    return OS2_MOU_ERROR_MOUSE_NO_DEVICE;
}

static unsigned short sample_buttons(void)
{
    unsigned short buttons;
    buttons = 0U;
    if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0)
        buttons |= OS2_MOU_BUTTON1;
    if ((GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0)
        buttons |= OS2_MOU_BUTTON2;
    if ((GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0)
        buttons |= OS2_MOU_BUTTON3;
    return buttons;
}

static Os2MouApiRet sample_position(unsigned short *row,
                                    unsigned short *col)
{
    HANDLE out;
    HWND hwnd;
    POINT pt;
    CONSOLE_SCREEN_BUFFER_INFO info;
    CONSOLE_FONT_INFO font;
    COORD cell;
    LONG x;
    LONG y;

    if (row == NULL || col == NULL)
        return OS2_MOU_ERROR_MOUSE_INV_PARMS;
    out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (out == NULL || out == INVALID_HANDLE_VALUE ||
        GetFileType(out) != FILE_TYPE_CHAR)
        return OS2_MOU_ERROR_MOUSE_NO_DEVICE;
    hwnd = GetConsoleWindow();
    if (hwnd == NULL)
        return OS2_MOU_ERROR_MOUSE_NO_DEVICE;
    if (!GetCursorPos(&pt) || !ScreenToClient(hwnd, &pt))
        return mouse_host_error();
    if (!GetConsoleScreenBufferInfo(out, &info))
        return mouse_host_error();
    memset(&font, 0, sizeof(font));
    if (!GetCurrentConsoleFont(out, FALSE, &font))
        return mouse_host_error();
    cell = GetConsoleFontSize(out, font.nFont);
    if (cell.X <= 0 || cell.Y <= 0)
        return OS2_MOU_ERROR_MOUSE_NO_DEVICE;

    x = pt.x;
    y = pt.y;
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    x = x / cell.X + info.srWindow.Left;
    y = y / cell.Y + info.srWindow.Top;
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (x >= info.dwSize.X)
        x = info.dwSize.X > 0 ? info.dwSize.X - 1 : 0;
    if (y >= info.dwSize.Y)
        y = info.dwSize.Y > 0 ? info.dwSize.Y - 1 : 0;
    *row = (unsigned short)y;
    *col = (unsigned short)x;
    return OS2_MOU_NO_ERROR;
}

static Os2MouApiRet win32_activate(void *opaque, unsigned short *buttons,
                                    unsigned short *mickeys,
                                    unsigned short *row,
                                    unsigned short *col)
{
    struct Os2MouWin32Context *context;
    int count;
    Os2MouApiRet rc;

    context = (struct Os2MouWin32Context *)opaque;
    if (context == NULL || buttons == NULL || mickeys == NULL ||
        row == NULL || col == NULL)
        return OS2_MOU_ERROR_MOUSE_INV_PARMS;
    rc = sample_position(row, col);
    if (rc != OS2_MOU_NO_ERROR)
        return rc;
    count = GetSystemMetrics(SM_CMOUSEBUTTONS);
    if (count <= 0)
        count = 2;
    if (count > 3)
        count = 3;
    *buttons = (unsigned short)count;
    /* Win32 exposes acceleration/speed, not the OS/2 mickeys/cm quantity.
     * Preserve a deterministic proof-level default rather than pretending
     * that SPI_GETMOUSESPEED is the same measurement. */
    *mickeys = 8U;
    context->active = 1;
    context->last_row = *row;
    context->last_col = *col;
    context->last_buttons = sample_buttons();
    context->have_sample = 1;
    return OS2_MOU_NO_ERROR;
}

static Os2MouApiRet win32_deactivate(void *opaque)
{
    struct Os2MouWin32Context *context;
    context = (struct Os2MouWin32Context *)opaque;
    if (context == NULL)
        return OS2_MOU_ERROR_MOUSE_INV_PARMS;
    context->active = 0;
    context->have_sample = 0;
    return OS2_MOU_NO_ERROR;
}

static Os2MouApiRet sample_event(struct Os2MouWin32Context *context,
                                 struct Os2MouHostEvent *event,
                                 int *present)
{
    unsigned short row;
    unsigned short col;
    unsigned short buttons;
    Os2MouApiRet rc;

    *present = 0;
    memset(event, 0, sizeof(*event));
    rc = sample_position(&row, &col);
    if (rc != OS2_MOU_NO_ERROR)
        return rc;
    buttons = sample_buttons();
    if (!context->have_sample) {
        context->last_row = row;
        context->last_col = col;
        context->last_buttons = buttons;
        context->have_sample = 1;
        return OS2_MOU_NO_ERROR;
    }
    if (row == context->last_row && col == context->last_col &&
        buttons == context->last_buttons)
        return OS2_MOU_NO_ERROR;

    event->row = row;
    event->col = col;
    event->buttons = buttons;
    event->changed_buttons = (unsigned short)(buttons ^ context->last_buttons);
    event->motion = (row != context->last_row || col != context->last_col);
    event->time = (uint32_t)GetTickCount();
    context->last_row = row;
    context->last_col = col;
    context->last_buttons = buttons;
    *present = 1;
    return OS2_MOU_NO_ERROR;
}

static Os2MouApiRet win32_read_event(void *opaque, int wait,
                                     struct Os2MouHostEvent *event,
                                     int *present)
{
    struct Os2MouWin32Context *context;
    Os2MouApiRet rc;

    context = (struct Os2MouWin32Context *)opaque;
    if (context == NULL || event == NULL || present == NULL)
        return OS2_MOU_ERROR_MOUSE_INV_PARMS;
    if (!context->active)
        return OS2_MOU_ERROR_MOUSE_NO_DEVICE;
    for (;;) {
        rc = sample_event(context, event, present);
        if (rc != OS2_MOU_NO_ERROR || *present || !wait)
            return rc;
        Sleep(1UL);
    }
}

static Os2MouApiRet win32_flush_events(void *opaque)
{
    struct Os2MouWin32Context *context;
    unsigned short row;
    unsigned short col;
    Os2MouApiRet rc;
    context = (struct Os2MouWin32Context *)opaque;
    if (context == NULL || !context->active)
        return OS2_MOU_ERROR_MOUSE_NO_DEVICE;
    rc = sample_position(&row, &col);
    if (rc != OS2_MOU_NO_ERROR)
        return rc;
    context->last_row = row;
    context->last_col = col;
    context->last_buttons = sample_buttons();
    context->have_sample = 1;
    return OS2_MOU_NO_ERROR;
}

static Os2MouApiRet win32_set_pointer_position(void *opaque,
                                                unsigned short row,
                                                unsigned short col)
{
    struct Os2MouWin32Context *context;
    HANDLE out;
    HWND hwnd;
    CONSOLE_SCREEN_BUFFER_INFO info;
    CONSOLE_FONT_INFO font;
    COORD cell;
    POINT pt;

    context = (struct Os2MouWin32Context *)opaque;
    if (context == NULL || !context->active)
        return OS2_MOU_ERROR_MOUSE_NO_DEVICE;
    out = GetStdHandle(STD_OUTPUT_HANDLE);
    hwnd = GetConsoleWindow();
    if (out == NULL || out == INVALID_HANDLE_VALUE || hwnd == NULL)
        return OS2_MOU_NO_ERROR;
    if (!GetConsoleScreenBufferInfo(out, &info))
        return OS2_MOU_NO_ERROR;
    memset(&font, 0, sizeof(font));
    if (!GetCurrentConsoleFont(out, FALSE, &font))
        return OS2_MOU_NO_ERROR;
    cell = GetConsoleFontSize(out, font.nFont);
    if (cell.X <= 0 || cell.Y <= 0)
        return OS2_MOU_NO_ERROR;
    pt.x = ((LONG)col - info.srWindow.Left) * cell.X + cell.X / 2;
    pt.y = ((LONG)row - info.srWindow.Top) * cell.Y + cell.Y / 2;
    if (ClientToScreen(hwnd, &pt))
        (void)SetCursorPos((int)pt.x, (int)pt.y);
    context->last_row = row;
    context->last_col = col;
    context->last_buttons = sample_buttons();
    context->have_sample = 1;
    return OS2_MOU_NO_ERROR;
}

static const struct Os2MouBackendOps win32_backend = {
    win32_activate,
    win32_deactivate,
    win32_read_event,
    win32_flush_events,
    win32_set_pointer_position
};

struct Os2MouSession *os2_mou_win32_session(void)
{
    if (!mou_session_ready) {
        memset(&mou_context, 0, sizeof(mou_context));
        os2_mou_session_init(&mou_session_storage, &mou_context,
                             &win32_backend);
        mou_session_ready = 1;
    }
    return &mou_session_storage;
}
