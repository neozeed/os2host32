/*
 * VIOCALLS.dll - first host implementation of the OS/2 VIO surface used by
 * reconstructed CMD.  M29A intentionally implements only the console calls
 * needed to remove direct Win32 console APIs from CMD proper.
 *
 * The exported ordinals match the recovered OS/2 CMD VIOCALLS imports.  This
 * Win32 DLL is a 32-bit host facade; it is not yet a 16:16 thunk target for
 * original NE callers.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>

#ifndef __cdecl
#define __cdecl
#endif

#define O2_NO_ERROR 0U
#define O2_ERROR_INVALID_FUNCTION 1U
#define O2_ERROR_INVALID_HANDLE 6U
#define O2_ERROR_INVALID_PARAMETER 87U

struct O2VioModeInfo {
    unsigned short cb;
    unsigned char fbType;
    unsigned char color;
    unsigned short col;
    unsigned short row;
    unsigned short hres;
    unsigned short vres;
    unsigned char fmt_ID;
    unsigned char attrib;
    unsigned long buf_addr;
    unsigned long buf_length;
    unsigned long full_length;
    unsigned long partial_length;
    char *ext_data_addr;
};

static unsigned short host_error(void)
{
    DWORD e;
    e = GetLastError();
    if (e == ERROR_INVALID_HANDLE)
        return O2_ERROR_INVALID_HANDLE;
    if (e == ERROR_INVALID_PARAMETER)
        return O2_ERROR_INVALID_PARAMETER;
    return O2_ERROR_INVALID_FUNCTION;
}

unsigned short __cdecl VioWrtTTY(const char *text, unsigned short count,
                                  unsigned short hvio)
{
    HANDLE h;
    DWORD done;
    (void)hvio;
    if (text == NULL && count != 0)
        return O2_ERROR_INVALID_PARAMETER;
    h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h == NULL || h == INVALID_HANDLE_VALUE)
        return O2_ERROR_INVALID_HANDLE;
    if (count == 0)
        return O2_NO_ERROR;
    done = 0;
    if (!WriteFile(h, text, (DWORD)count, &done, NULL))
        return host_error();
    if (done != (DWORD)count)
        return O2_ERROR_INVALID_FUNCTION;
    return O2_NO_ERROR;
}

unsigned short __cdecl VioGetCurPos(unsigned short *row, unsigned short *col,
                                     unsigned short hvio)
{
    HANDLE h;
    CONSOLE_SCREEN_BUFFER_INFO info;
    (void)hvio;
    if (row == NULL || col == NULL)
        return O2_ERROR_INVALID_PARAMETER;
    h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h == NULL || h == INVALID_HANDLE_VALUE)
        return O2_ERROR_INVALID_HANDLE;
    if (!GetConsoleScreenBufferInfo(h, &info))
        return host_error();
    *row = (unsigned short)info.dwCursorPosition.Y;
    *col = (unsigned short)info.dwCursorPosition.X;
    return O2_NO_ERROR;
}

unsigned short __cdecl VioSetCurPos(unsigned short row, unsigned short col,
                                     unsigned short hvio)
{
    HANDLE h;
    COORD pos;
    (void)hvio;
    h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h == NULL || h == INVALID_HANDLE_VALUE)
        return O2_ERROR_INVALID_HANDLE;
    pos.X = (SHORT)col;
    pos.Y = (SHORT)row;
    if (!SetConsoleCursorPosition(h, pos))
        return host_error();
    return O2_NO_ERROR;
}

unsigned short __cdecl VioGetMode(struct O2VioModeInfo *mode,
                                   unsigned short hvio)
{
    HANDLE h;
    CONSOLE_SCREEN_BUFFER_INFO info;
    (void)hvio;
    if (mode == NULL || mode->cb < 8U)
        return O2_ERROR_INVALID_PARAMETER;
    h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h == NULL || h == INVALID_HANDLE_VALUE)
        return O2_ERROR_INVALID_HANDLE;
    if (!GetConsoleScreenBufferInfo(h, &info))
        return host_error();
    mode->fbType = 0;
    mode->color = 16;
    mode->col = (unsigned short)info.dwSize.X;
    mode->row = (unsigned short)info.dwSize.Y;
    mode->hres = 0;
    mode->vres = 0;
    return O2_NO_ERROR;
}

unsigned short __cdecl VioScrollUp(unsigned short top, unsigned short left,
                                    unsigned short bottom,
                                    unsigned short right,
                                    unsigned short lines,
                                    const unsigned char *cell,
                                    unsigned short hvio)
{
    HANDLE h;
    CONSOLE_SCREEN_BUFFER_INFO info;
    SMALL_RECT src;
    COORD dest;
    CHAR_INFO fill;
    SHORT height;
    SHORT width;
    (void)hvio;

    h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h == NULL || h == INVALID_HANDLE_VALUE)
        return O2_ERROR_INVALID_HANDLE;
    if (!GetConsoleScreenBufferInfo(h, &info))
        return host_error();

    if (bottom == 0xffffU || bottom >= (unsigned short)info.dwSize.Y)
        bottom = (unsigned short)(info.dwSize.Y - 1);
    if (right == 0xffffU || right >= (unsigned short)info.dwSize.X)
        right = (unsigned short)(info.dwSize.X - 1);
    if (top > bottom || left > right)
        return O2_ERROR_INVALID_PARAMETER;

    fill.Char.AsciiChar = cell != NULL ? (char)cell[0] : ' ';
    fill.Attributes = cell != NULL ? (WORD)cell[1] : info.wAttributes;

    height = (SHORT)(bottom - top + 1U);
    width = (SHORT)(right - left + 1U);
    if (lines == 0U || lines >= (unsigned short)height) {
        COORD home;
        DWORD cells;
        DWORD done;
        home.X = (SHORT)left;
        home.Y = (SHORT)top;
        cells = (DWORD)(unsigned short)height * (DWORD)(unsigned short)width;
        if (!FillConsoleOutputCharacterA(h, fill.Char.AsciiChar, cells,
                                         home, &done))
            return host_error();
        if (!FillConsoleOutputAttribute(h, fill.Attributes, cells,
                                        home, &done))
            return host_error();
        return O2_NO_ERROR;
    }

    src.Left = (SHORT)left;
    src.Right = (SHORT)right;
    src.Top = (SHORT)(top + lines);
    src.Bottom = (SHORT)bottom;
    dest.X = (SHORT)left;
    dest.Y = (SHORT)top;
    if (!ScrollConsoleScreenBufferA(h, &src, NULL, dest, &fill))
        return host_error();
    return O2_NO_ERROR;
}
