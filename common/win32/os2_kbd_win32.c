#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string.h>

#include "os2_kbd.h"
#include "os2_kbd_backend.h"
#include "os2_kbd_win32.h"

#ifndef SHIFT_PRESSED
#define SHIFT_PRESSED 0x0010U
#endif

struct Os2KbdWin32Context {
    volatile LONG ctrl_handler_installed;
    volatile LONG ctrl_input_waiting;
};

static struct Os2KbdWin32Context kbd_context;
static struct Os2KbdSession kbd_session_storage;
static int kbd_session_ready;

static Os2KbdApiRet host_error(void)
{
    DWORD e;
    e = GetLastError();
    if (e == ERROR_INVALID_HANDLE)
        return OS2_KBD_ERROR_INVALID_HANDLE;
    if (e == ERROR_INVALID_PARAMETER)
        return OS2_KBD_ERROR_INVALID_PARAMETER;
    return OS2_KBD_ERROR_INVALID_FUNCTION;
}

static unsigned short control_state(DWORD state)
{
    unsigned short fs;
    fs = 0U;
    if ((state & SHIFT_PRESSED) != 0U)
        fs |= 0x0003U;
    if ((state & LEFT_CTRL_PRESSED) != 0U)
        fs |= 0x0004U;
    if ((state & RIGHT_CTRL_PRESSED) != 0U)
        fs |= 0x0008U;
    if ((state & LEFT_ALT_PRESSED) != 0U)
        fs |= 0x0010U;
    if ((state & RIGHT_ALT_PRESSED) != 0U)
        fs |= 0x0020U;
    if ((state & CAPSLOCK_ON) != 0U)
        fs |= 0x0040U;
    if ((state & NUMLOCK_ON) != 0U)
        fs |= 0x0080U;
    if ((state & SCROLLLOCK_ON) != 0U)
        fs |= 0x0100U;
    return fs;
}

static BOOL WINAPI o2_console_ctrl_handler(DWORD type)
{
    HANDLE h;
    DWORD htype;
    DWORD wrote;
    INPUT_RECORD rec;

    if (type != CTRL_C_EVENT && type != CTRL_BREAK_EVENT)
        return FALSE;

    if (InterlockedCompareExchange(&kbd_context.ctrl_input_waiting, 0, 0) != 0) {
        h = GetStdHandle(STD_INPUT_HANDLE);
        if (h != NULL && h != INVALID_HANDLE_VALUE) {
            htype = GetFileType(h);
            if (htype == FILE_TYPE_CHAR) {
                ZeroMemory(&rec, sizeof(rec));
                rec.EventType = KEY_EVENT;
                rec.Event.KeyEvent.bKeyDown = TRUE;
                rec.Event.KeyEvent.wRepeatCount = 1;
                rec.Event.KeyEvent.wVirtualKeyCode = (WORD)'C';
                rec.Event.KeyEvent.wVirtualScanCode = 0x2eU;
                rec.Event.KeyEvent.uChar.AsciiChar = 3;
                rec.Event.KeyEvent.dwControlKeyState = LEFT_CTRL_PRESSED;
                wrote = 0;
                if (WriteConsoleInputA(h, &rec, 1, &wrote) && wrote == 1U)
                    return TRUE;
            }
        }
    }
    return TRUE;
}

static void ensure_console_ctrl_handler(void)
{
    if (InterlockedCompareExchange(&kbd_context.ctrl_handler_installed, 1, 0) == 0) {
        if (!SetConsoleCtrlHandler(o2_console_ctrl_handler, TRUE))
            InterlockedExchange(&kbd_context.ctrl_handler_installed, 0);
    }
}

static void event_from_console(const KEY_EVENT_RECORD *key,
                               struct Os2KbdHostEvent *event)
{
    memset(event, 0, sizeof(*event));
    event->ch_char = (unsigned char)key->uChar.AsciiChar;
    event->ch_scan = (unsigned char)key->wVirtualScanCode;
    event->fs_state = control_state(key->dwControlKeyState);
    event->time = (uint32_t)GetTickCount();
    event->echoable = 1;
}

static Os2KbdApiRet win32_read_event(void *opaque, int wait,
                                     struct Os2KbdHostEvent *event,
                                     int *present)
{
    struct Os2KbdWin32Context *context;
    HANDLE h;
    DWORD type;
    DWORD got;
    INPUT_RECORD rec;
    unsigned char c;

    context = (struct Os2KbdWin32Context *)opaque;
    if (event == NULL || present == NULL)
        return OS2_KBD_ERROR_INVALID_PARAMETER;
    *present = 0;
    memset(event, 0, sizeof(*event));
    ensure_console_ctrl_handler();

    h = GetStdHandle(STD_INPUT_HANDLE);
    if (h == NULL || h == INVALID_HANDLE_VALUE)
        return OS2_KBD_ERROR_INVALID_HANDLE;
    type = GetFileType(h);

    if (type == FILE_TYPE_CHAR) {
        for (;;) {
            if (!wait) {
                DWORD pending;
                pending = 0;
                if (!GetNumberOfConsoleInputEvents(h, &pending))
                    return host_error();
                if (pending == 0U)
                    return OS2_KBD_NO_ERROR;
            }
            got = 0;
            InterlockedExchange(&context->ctrl_input_waiting, 1);
            if (!ReadConsoleInputA(h, &rec, 1, &got)) {
                InterlockedExchange(&context->ctrl_input_waiting, 0);
                return host_error();
            }
            InterlockedExchange(&context->ctrl_input_waiting, 0);
            if (got == 0U)
                return OS2_KBD_NO_ERROR;
            if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown)
                continue;
            event_from_console(&rec.Event.KeyEvent, event);
            *present = 1;
            return OS2_KBD_NO_ERROR;
        }
    }

    if (!wait && type == FILE_TYPE_PIPE) {
        DWORD avail;
        avail = 0;
        if (!PeekNamedPipe(h, NULL, 0, NULL, &avail, NULL))
            return host_error();
        if (avail == 0U)
            return OS2_KBD_NO_ERROR;
    }
    got = 0;
    c = 0;
    if (!ReadFile(h, &c, 1, &got, NULL))
        return host_error();
    if (got != 0U) {
        event->ch_char = c;
        event->time = (uint32_t)GetTickCount();
        event->echoable = 0;
        *present = 1;
    }
    return OS2_KBD_NO_ERROR;
}

static Os2KbdApiRet win32_peek_event(void *opaque,
                                     struct Os2KbdHostEvent *event,
                                     int *present)
{
    HANDLE h;
    DWORD type;
    DWORD got;
    INPUT_RECORD rec;
    unsigned char c;
    (void)opaque;

    if (event == NULL || present == NULL)
        return OS2_KBD_ERROR_INVALID_PARAMETER;
    *present = 0;
    memset(event, 0, sizeof(*event));

    h = GetStdHandle(STD_INPUT_HANDLE);
    if (h == NULL || h == INVALID_HANDLE_VALUE)
        return OS2_KBD_ERROR_INVALID_HANDLE;
    type = GetFileType(h);
    if (type == FILE_TYPE_CHAR) {
        got = 0;
        if (!PeekConsoleInputA(h, &rec, 1, &got))
            return host_error();
        if (got != 0U && rec.EventType == KEY_EVENT &&
            rec.Event.KeyEvent.bKeyDown) {
            event_from_console(&rec.Event.KeyEvent, event);
            *present = 1;
        }
        return OS2_KBD_NO_ERROR;
    }
    if (type == FILE_TYPE_PIPE) {
        DWORD avail;
        avail = 0;
        c = 0;
        got = 0;
        if (!PeekNamedPipe(h, &c, 1, &got, &avail, NULL))
            return host_error();
        if (got != 0U) {
            event->ch_char = c;
            event->time = (uint32_t)GetTickCount();
            event->echoable = 0;
            *present = 1;
        }
    }
    return OS2_KBD_NO_ERROR;
}

static Os2KbdApiRet win32_flush(void *opaque)
{
    HANDLE h;
    DWORD type;
    (void)opaque;
    h = GetStdHandle(STD_INPUT_HANDLE);
    if (h == NULL || h == INVALID_HANDLE_VALUE)
        return OS2_KBD_ERROR_INVALID_HANDLE;
    type = GetFileType(h);
    if (type == FILE_TYPE_CHAR && !FlushConsoleInputBuffer(h))
        return host_error();
    return OS2_KBD_NO_ERROR;
}

static Os2KbdApiRet win32_echo_bytes(void *opaque, const char *bytes,
                                     unsigned short count)
{
    HANDLE h;
    DWORD type;
    DWORD wrote;
    (void)opaque;
    if (bytes == NULL && count != 0U)
        return OS2_KBD_ERROR_INVALID_PARAMETER;
    h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h == NULL || h == INVALID_HANDLE_VALUE)
        return OS2_KBD_NO_ERROR;
    type = GetFileType(h);
    if (type != FILE_TYPE_CHAR)
        return OS2_KBD_NO_ERROR;
    wrote = 0;
    if (!WriteConsoleA(h, bytes, (DWORD)count, &wrote, NULL))
        return host_error();
    return OS2_KBD_NO_ERROR;
}

static unsigned long win32_milliseconds(void *opaque)
{
    (void)opaque;
    return (unsigned long)GetTickCount();
}

static const struct Os2KbdBackendOps win32_backend = {
    win32_read_event,
    win32_peek_event,
    win32_flush,
    win32_echo_bytes,
    win32_milliseconds
};

struct Os2KbdSession *os2_kbd_win32_session(void)
{
    if (!kbd_session_ready) {
        memset(&kbd_context, 0, sizeof(kbd_context));
        os2_kbd_session_init(&kbd_session_storage, &kbd_context, &win32_backend);
        kbd_session_ready = 1;
    }
    return &kbd_session_storage;
}
