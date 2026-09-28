#include <string.h>

#include "windows.h"
#include "win32_vio_stub.h"

#define STUB_ROWS 5
#define STUB_COLS 10
#define STUB_CELLS (STUB_ROWS * STUB_COLS)

static unsigned char stub_chars[STUB_CELLS];
static WORD stub_attrs[STUB_CELLS];
static COORD stub_cursor;
static CONSOLE_CURSOR_INFO stub_cursor_info;
static DWORD stub_error;
static int output_token;

static int valid_handle(HANDLE handle)
{
    return handle == (HANDLE)&output_token;
}

static unsigned long index_of(SHORT row, SHORT column)
{
    return (unsigned long)(unsigned short)row * STUB_COLS +
           (unsigned long)(unsigned short)column;
}

void win32_vio_stub_reset(void)
{
    unsigned long i;
    for (i = 0UL; i < STUB_CELLS; ++i) {
        stub_chars[i] = (unsigned char)'.';
        stub_attrs[i] = 0x07U;
    }
    stub_cursor.X = 0;
    stub_cursor.Y = 0;
    stub_cursor_info.dwSize = 25U;
    stub_cursor_info.bVisible = TRUE;
    stub_error = 0U;
}

unsigned char win32_vio_stub_char(unsigned short row, unsigned short column)
{
    return stub_chars[(unsigned long)row * STUB_COLS + column];
}

unsigned short win32_vio_stub_attr(unsigned short row, unsigned short column)
{
    return stub_attrs[(unsigned long)row * STUB_COLS + column];
}

unsigned short win32_vio_stub_cursor_row(void)
{
    return (unsigned short)stub_cursor.Y;
}

unsigned short win32_vio_stub_cursor_column(void)
{
    return (unsigned short)stub_cursor.X;
}

unsigned long win32_vio_stub_cursor_size(void)
{
    return (unsigned long)stub_cursor_info.dwSize;
}

int win32_vio_stub_cursor_visible(void)
{
    return stub_cursor_info.bVisible != FALSE;
}

DWORD GetLastError(void)
{
    return stub_error;
}

HANDLE GetStdHandle(DWORD which)
{
    if (which != STD_OUTPUT_HANDLE) {
        stub_error = ERROR_INVALID_PARAMETER;
        return INVALID_HANDLE_VALUE;
    }
    return (HANDLE)&output_token;
}

BOOL GetConsoleScreenBufferInfo(HANDLE handle,
                                CONSOLE_SCREEN_BUFFER_INFO *info)
{
    if (!valid_handle(handle) || info == NULL) {
        stub_error = ERROR_INVALID_HANDLE;
        return FALSE;
    }
    info->dwSize.X = STUB_COLS;
    info->dwSize.Y = STUB_ROWS;
    info->dwCursorPosition = stub_cursor;
    info->wAttributes = 0x07U;
    info->srWindow.Left = 0;
    info->srWindow.Top = 0;
    info->srWindow.Right = STUB_COLS - 1;
    info->srWindow.Bottom = STUB_ROWS - 1;
    info->dwMaximumWindowSize = info->dwSize;
    return TRUE;
}

BOOL GetConsoleCursorInfo(HANDLE handle, CONSOLE_CURSOR_INFO *info)
{
    if (!valid_handle(handle) || info == NULL) {
        stub_error = ERROR_INVALID_HANDLE;
        return FALSE;
    }
    *info = stub_cursor_info;
    return TRUE;
}

BOOL SetConsoleCursorInfo(HANDLE handle, const CONSOLE_CURSOR_INFO *info)
{
    if (!valid_handle(handle) || info == NULL || info->dwSize < 1U ||
        info->dwSize > 100U) {
        stub_error = ERROR_INVALID_PARAMETER;
        return FALSE;
    }
    stub_cursor_info = *info;
    return TRUE;
}

BOOL SetConsoleCursorPosition(HANDLE handle, COORD position)
{
    if (!valid_handle(handle) || position.X < 0 || position.Y < 0 ||
        position.X >= STUB_COLS || position.Y >= STUB_ROWS) {
        stub_error = ERROR_INVALID_PARAMETER;
        return FALSE;
    }
    stub_cursor = position;
    return TRUE;
}

BOOL ReadConsoleOutputCharacterA(HANDLE handle, char *text, DWORD count,
                                 COORD position, DWORD *done)
{
    unsigned long index;
    DWORD i;
    if (!valid_handle(handle) || text == NULL || done == NULL ||
        position.X < 0 || position.Y < 0 ||
        position.X >= STUB_COLS || position.Y >= STUB_ROWS) {
        stub_error = ERROR_INVALID_PARAMETER;
        return FALSE;
    }
    index = index_of(position.Y, position.X);
    *done = 0U;
    for (i = 0U; i < count && index + i < STUB_CELLS; ++i) {
        text[i] = (char)stub_chars[index + i];
        ++*done;
    }
    return TRUE;
}

BOOL ReadConsoleOutputAttribute(HANDLE handle, WORD *attribute, DWORD count,
                                COORD position, DWORD *done)
{
    unsigned long index;
    DWORD i;
    if (!valid_handle(handle) || attribute == NULL || done == NULL ||
        position.X < 0 || position.Y < 0 ||
        position.X >= STUB_COLS || position.Y >= STUB_ROWS) {
        stub_error = ERROR_INVALID_PARAMETER;
        return FALSE;
    }
    index = index_of(position.Y, position.X);
    *done = 0U;
    for (i = 0U; i < count && index + i < STUB_CELLS; ++i) {
        attribute[i] = stub_attrs[index + i];
        ++*done;
    }
    return TRUE;
}

BOOL WriteConsoleOutputCharacterA(HANDLE handle, const char *text,
                                  DWORD count, COORD position, DWORD *done)
{
    unsigned long index;
    DWORD i;
    if (!valid_handle(handle) || text == NULL || done == NULL ||
        position.X < 0 || position.Y < 0 ||
        position.X >= STUB_COLS || position.Y >= STUB_ROWS) {
        stub_error = ERROR_INVALID_PARAMETER;
        return FALSE;
    }
    index = index_of(position.Y, position.X);
    *done = 0U;
    for (i = 0U; i < count && index + i < STUB_CELLS; ++i) {
        stub_chars[index + i] = (unsigned char)text[i];
        ++*done;
    }
    return TRUE;
}

BOOL FillConsoleOutputAttribute(HANDLE handle, WORD attribute, DWORD count,
                                COORD position, DWORD *done)
{
    unsigned long index;
    DWORD i;
    if (!valid_handle(handle) || done == NULL || position.X < 0 ||
        position.Y < 0 || position.X >= STUB_COLS || position.Y >= STUB_ROWS) {
        stub_error = ERROR_INVALID_PARAMETER;
        return FALSE;
    }
    index = index_of(position.Y, position.X);
    *done = 0U;
    for (i = 0U; i < count && index + i < STUB_CELLS; ++i) {
        stub_attrs[index + i] = attribute;
        ++*done;
    }
    return TRUE;
}

BOOL FillConsoleOutputCharacterA(HANDLE handle, char character, DWORD count,
                                 COORD position, DWORD *done)
{
    unsigned long index;
    DWORD i;
    if (!valid_handle(handle) || done == NULL || position.X < 0 ||
        position.Y < 0 || position.X >= STUB_COLS || position.Y >= STUB_ROWS) {
        stub_error = ERROR_INVALID_PARAMETER;
        return FALSE;
    }
    index = index_of(position.Y, position.X);
    *done = 0U;
    for (i = 0U; i < count && index + i < STUB_CELLS; ++i) {
        stub_chars[index + i] = (unsigned char)character;
        ++*done;
    }
    return TRUE;
}

BOOL ScrollConsoleScreenBufferA(HANDLE handle, const SMALL_RECT *source,
                                const SMALL_RECT *clip, COORD destination,
                                const CHAR_INFO *fill)
{
    SHORT row;
    SHORT column;
    SHORT height;
    SHORT width;
    SHORT target_row;
    SHORT target_column;
    unsigned char temp_chars[STUB_CELLS];
    WORD temp_attrs[STUB_CELLS];
    unsigned long source_index;
    unsigned long target_index;
    (void)clip;

    if (!valid_handle(handle) || source == NULL || fill == NULL) {
        stub_error = ERROR_INVALID_PARAMETER;
        return FALSE;
    }
    memcpy(temp_chars, stub_chars, sizeof(temp_chars));
    memcpy(temp_attrs, stub_attrs, sizeof(temp_attrs));
    height = (SHORT)(source->Bottom - source->Top + 1);
    width = (SHORT)(source->Right - source->Left + 1);

    for (row = 0; row < height; ++row) {
        for (column = 0; column < width; ++column) {
            target_row = (SHORT)(destination.Y + row);
            target_column = (SHORT)(destination.X + column);
            if (target_row >= 0 && target_row < STUB_ROWS &&
                target_column >= 0 && target_column < STUB_COLS) {
                source_index = index_of((SHORT)(source->Top + row),
                                        (SHORT)(source->Left + column));
                target_index = index_of(target_row, target_column);
                stub_chars[target_index] = temp_chars[source_index];
                stub_attrs[target_index] = temp_attrs[source_index];
            }
        }
    }

    /* The VIO backend uses this API only for upward scrolling.  Fill the
     * newly exposed rows at the bottom of that same rectangle. */
    if (destination.Y < source->Top) {
        SHORT exposed;
        exposed = (SHORT)(source->Top - destination.Y);
        for (row = (SHORT)(source->Bottom - exposed + 1);
             row <= source->Bottom; ++row) {
            for (column = source->Left; column <= source->Right; ++column) {
                target_index = index_of(row, column);
                stub_chars[target_index] = (unsigned char)fill->Char.AsciiChar;
                stub_attrs[target_index] = fill->Attributes;
            }
        }
    }
    return TRUE;
}

BOOL WriteFile(HANDLE handle, const void *buffer, DWORD count, DWORD *done,
               void *overlapped)
{
    const unsigned char *text;
    DWORD i;
    unsigned long index;
    unsigned char ch;
    (void)overlapped;
    if (!valid_handle(handle) || buffer == NULL || done == NULL) {
        stub_error = ERROR_INVALID_PARAMETER;
        return FALSE;
    }
    text = (const unsigned char *)buffer;
    *done = 0U;
    for (i = 0U; i < count; ++i) {
        ch = text[i];
        if (ch == '\r') {
            stub_cursor.X = 0;
        } else if (ch == '\n') {
            if (stub_cursor.Y + 1 < STUB_ROWS)
                ++stub_cursor.Y;
        } else if (ch == '\b') {
            if (stub_cursor.X > 0)
                --stub_cursor.X;
        } else {
            index = index_of(stub_cursor.Y, stub_cursor.X);
            stub_chars[index] = ch;
            if (stub_cursor.X + 1 < STUB_COLS)
                ++stub_cursor.X;
        }
        ++*done;
    }
    return TRUE;
}
