#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h>
#include <stdlib.h>

#include "os2_vio_win32.h"

static unsigned short host_error(void)
{
    DWORD error;
    error = GetLastError();
    if (error == ERROR_INVALID_HANDLE)
        return OS2_VIO_ERROR_INVALID_HANDLE;
    if (error == ERROR_INVALID_PARAMETER)
        return OS2_VIO_ERROR_INVALID_PARAMETER;
    return OS2_VIO_ERROR_INVALID_FUNCTION;
}

static unsigned short output_handle(HANDLE *result)
{
    HANDLE handle;
    handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == NULL || handle == INVALID_HANDLE_VALUE)
        return OS2_VIO_ERROR_INVALID_HANDLE;
    *result = handle;
    return OS2_VIO_NO_ERROR;
}

static unsigned short screen_info(HANDLE *handle,
                                  CONSOLE_SCREEN_BUFFER_INFO *info)
{
    unsigned short rc;
    rc = output_handle(handle);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    if (!GetConsoleScreenBufferInfo(*handle, info))
        return host_error();
    return OS2_VIO_NO_ERROR;
}

static unsigned short win32_query_size(void *opaque, unsigned short *rows,
                                       unsigned short *columns)
{
    HANDLE handle;
    CONSOLE_SCREEN_BUFFER_INFO info;
    unsigned short rc;
    (void)opaque;
    if (rows == NULL || columns == NULL)
        return OS2_VIO_ERROR_INVALID_PARAMETER;
    rc = screen_info(&handle, &info);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    if (opaque != NULL) {
        struct Os2VioWin32State *state;
        state = (struct Os2VioWin32State *)opaque;
        if (state->viewport_valid &&
            (state->left != info.srWindow.Left || state->top != info.srWindow.Top))
            state->session->cells_valid = 0;
        state->left = info.srWindow.Left;
        state->top = info.srWindow.Top;
        state->viewport_valid = 1;
    }
    *rows = (unsigned short)(info.srWindow.Bottom - info.srWindow.Top + 1);
    *columns = (unsigned short)(info.srWindow.Right - info.srWindow.Left + 1);
    return OS2_VIO_NO_ERROR;
}

static unsigned short win32_get_cursor_pos(void *opaque, unsigned short *row,
                                           unsigned short *column)
{
    HANDLE handle;
    CONSOLE_SCREEN_BUFFER_INFO info;
    unsigned short rc;
    (void)opaque;
    if (row == NULL || column == NULL)
        return OS2_VIO_ERROR_INVALID_PARAMETER;
    rc = screen_info(&handle, &info);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    *row = (unsigned short)(info.dwCursorPosition.Y < info.srWindow.Top ? 0 :
        (info.dwCursorPosition.Y > info.srWindow.Bottom ?
         info.srWindow.Bottom - info.srWindow.Top :
         info.dwCursorPosition.Y - info.srWindow.Top));
    *column = (unsigned short)(info.dwCursorPosition.X < info.srWindow.Left ? 0 :
        (info.dwCursorPosition.X > info.srWindow.Right ?
         info.srWindow.Right - info.srWindow.Left :
         info.dwCursorPosition.X - info.srWindow.Left));
    return OS2_VIO_NO_ERROR;
}

static unsigned short win32_set_cursor_pos(void *opaque, unsigned short row,
                                           unsigned short column)
{
    HANDLE handle;
    COORD position;
    CONSOLE_SCREEN_BUFFER_INFO info;
    unsigned short rc;
    (void)opaque;
    rc = screen_info(&handle, &info);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    position.X = (SHORT)(info.srWindow.Left + column);
    position.Y = (SHORT)(info.srWindow.Top + row);
    if (!SetConsoleCursorPosition(handle, position))
        return host_error();
    return OS2_VIO_NO_ERROR;
}

static unsigned short win32_get_cursor_type(void *opaque,
                                             struct Os2VioCursorInfo *cursor)
{
    HANDLE handle;
    CONSOLE_CURSOR_INFO win_cursor;
    unsigned long height;
    unsigned long start;
    unsigned short rc;
    (void)opaque;
    if (cursor == NULL)
        return OS2_VIO_ERROR_INVALID_PARAMETER;
    rc = output_handle(&handle);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    if (!GetConsoleCursorInfo(handle, &win_cursor))
        return host_error();

    /* Win32 reports cursor height as a percentage.  OS/2 reports start/end
     * scan lines.  Use a stable 16-scan-line text-cell approximation. */
    height = ((unsigned long)win_cursor.dwSize * 16UL + 99UL) / 100UL;
    if (height < 1UL)
        height = 1UL;
    if (height > 16UL)
        height = 16UL;
    start = 16UL - height;
    cursor->y_start = (unsigned short)start;
    cursor->c_end = 15U;
    cursor->cx = 0U;
    cursor->attr = win_cursor.bVisible ? 0U : 0xffffU;
    return OS2_VIO_NO_ERROR;
}

static unsigned short win32_set_cursor_type(
    void *opaque, const struct Os2VioCursorInfo *cursor)
{
    HANDLE handle;
    CONSOLE_CURSOR_INFO win_cursor;
    unsigned long height;
    unsigned long percentage;
    unsigned short rc;
    (void)opaque;
    if (cursor == NULL)
        return OS2_VIO_ERROR_INVALID_PARAMETER;
    rc = output_handle(&handle);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    if (!GetConsoleCursorInfo(handle, &win_cursor))
        return host_error();

    win_cursor.bVisible = cursor->attr == 0xffffU ? FALSE : TRUE;
    if (cursor->c_end >= cursor->y_start && cursor->c_end < 16U) {
        height = (unsigned long)(cursor->c_end - cursor->y_start + 1U);
        percentage = (height * 100UL + 15UL) / 16UL;
        if (percentage < 1UL)
            percentage = 1UL;
        if (percentage > 100UL)
            percentage = 100UL;
        win_cursor.dwSize = (DWORD)percentage;
    }
    if (!SetConsoleCursorInfo(handle, &win_cursor))
        return host_error();
    return OS2_VIO_NO_ERROR;
}


static unsigned short win32_read_cells(void *opaque, struct Os2VioCell *cells,
                                       unsigned short rows,
                                       unsigned short columns)
{
    HANDLE handle;
    COORD position;
    DWORD done;
    unsigned short row;
    unsigned short column;
    unsigned short rc;
    char *characters;
    WORD *attributes;
    unsigned long base;
    CONSOLE_SCREEN_BUFFER_INFO info;
    (void)opaque;

    if (cells == NULL || rows == 0U || columns == 0U)
        return OS2_VIO_ERROR_INVALID_PARAMETER;
    rc = screen_info(&handle, &info);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    if (rows > info.srWindow.Bottom - info.srWindow.Top + 1 ||
        columns > info.srWindow.Right - info.srWindow.Left + 1)
        return OS2_VIO_ERROR_INVALID_PARAMETER;

    characters = (char *)malloc((size_t)columns);
    attributes = (WORD *)malloc((size_t)columns * sizeof(*attributes));
    if (characters == NULL || attributes == NULL) {
        free(characters);
        free(attributes);
        return OS2_VIO_ERROR_NOT_ENOUGH_MEMORY;
    }

    for (row = 0U; row < rows; ++row) {
        position.X = info.srWindow.Left;
        position.Y = (SHORT)(info.srWindow.Top + row);
        done = 0;
        if (!ReadConsoleOutputCharacterA(handle, characters, (DWORD)columns,
                                         position, &done) ||
            done != (DWORD)columns) {
            free(characters);
            free(attributes);
            return host_error();
        }
        done = 0;
        if (!ReadConsoleOutputAttribute(handle, attributes, (DWORD)columns,
                                        position, &done) ||
            done != (DWORD)columns) {
            free(characters);
            free(attributes);
            return host_error();
        }
        base = (unsigned long)row * (unsigned long)columns;
        for (column = 0U; column < columns; ++column) {
            cells[base + column].character =
                (unsigned char)characters[column];
            cells[base + column].attribute =
                (unsigned char)(attributes[column] & 0xffU);
        }
    }

    free(characters);
    free(attributes);
    return OS2_VIO_NO_ERROR;
}

/* A VIO span wraps at the visible width, not the backing buffer width. */
static unsigned short write_span(const char *text, unsigned char character,
                                 unsigned char attribute, unsigned short count,
                                 unsigned short row, unsigned short column,
                                 int chars, int attrs, int repeat)
{
    HANDLE handle;
    CONSOLE_SCREEN_BUFFER_INFO info;
    COORD position;
    DWORD done;
    DWORD chunk;
    unsigned short rc;
    unsigned short width;
    rc = screen_info(&handle, &info);
    if (rc != OS2_VIO_NO_ERROR) return rc;
    width = (unsigned short)(info.srWindow.Right - info.srWindow.Left + 1);
    if (column >= width || row > info.srWindow.Bottom - info.srWindow.Top)
        return OS2_VIO_ERROR_INVALID_PARAMETER;
    while (count) {
        if (row > info.srWindow.Bottom - info.srWindow.Top)
            return OS2_VIO_ERROR_INVALID_PARAMETER;
        chunk = width - column;
        if (chunk > count) chunk = count;
        position.X = (SHORT)(info.srWindow.Left + column);
        position.Y = (SHORT)(info.srWindow.Top + row);
        if (chars) {
            done = 0;
            if (repeat) {
                if (!FillConsoleOutputCharacterA(handle, (char)character,
                                                  chunk, position, &done))
                    return host_error();
            } else if (!WriteConsoleOutputCharacterA(handle, text, chunk,
                                                       position, &done))
                return host_error();
            if (done != chunk) return OS2_VIO_ERROR_INVALID_FUNCTION;
        }
        if (attrs) {
            done = 0;
            if (!FillConsoleOutputAttribute(handle,
                (WORD)os2_vio_win32_attribute(attribute), chunk, position, &done))
                return host_error();
            if (done != chunk) return OS2_VIO_ERROR_INVALID_FUNCTION;
        }
        if (text) text += chunk;
        count = (unsigned short)(count - chunk);
        column = 0;
        ++row;
    }
    return OS2_VIO_NO_ERROR;
}

static unsigned short win32_write_chars_at(void *opaque, const char *text,
    unsigned short count, unsigned short row, unsigned short column)
{
    (void)opaque;
    return write_span(text, 0, 0, count, row, column, 1, 0, 0);
}

static unsigned short win32_write_chars_attr_at(void *opaque, const char *text,
    unsigned short count, unsigned short row, unsigned short column,
    unsigned char attribute)
{
    (void)opaque;
    return write_span(text, 0, attribute, count, row, column, 1, 1, 0);
}

static unsigned short win32_write_attrs_at(void *opaque,
    unsigned char attribute, unsigned short count, unsigned short row,
    unsigned short column)
{
    (void)opaque;
    return write_span(NULL, 0, attribute, count, row, column, 0, 1, 0);
}

static unsigned short win32_write_cell_at(void *opaque,
    unsigned char character, unsigned char attribute, unsigned short count,
    unsigned short row, unsigned short column)
{
    (void)opaque;
    return write_span(NULL, character, attribute, count, row, column, 1, 1, 1);
}

static unsigned short fill_rectangle(HANDLE handle, unsigned short top,
                                     unsigned short left, unsigned short bottom,
                                     unsigned short right,
                                     const unsigned char cell[2])
{
    COORD position;
    DWORD width;
    DWORD done;
    unsigned short row;
    WORD attribute;

    width = (DWORD)(right - left + 1U);
    attribute = (WORD)os2_vio_win32_attribute(cell[1]);
    for (row = top; row <= bottom; ++row) {
        position.X = (SHORT)left;
        position.Y = (SHORT)row;
        done = 0;
        if (!FillConsoleOutputCharacterA(handle, (char)cell[0], width,
                                         position, &done))
            return host_error();
        if (done != width)
            return OS2_VIO_ERROR_INVALID_FUNCTION;
        done = 0;
        if (!FillConsoleOutputAttribute(handle, attribute, width,
                                        position, &done))
            return host_error();
        if (done != width)
            return OS2_VIO_ERROR_INVALID_FUNCTION;
        if (row == 0xffffU)
            break;
    }
    return OS2_VIO_NO_ERROR;
}

static unsigned short win32_scroll_up(void *opaque, unsigned short top,
                                      unsigned short left,
                                      unsigned short bottom,
                                      unsigned short right,
                                      unsigned short lines,
                                      const unsigned char cell[2])
{
    HANDLE handle;
    SMALL_RECT source;
    SMALL_RECT clip;
    CONSOLE_SCREEN_BUFFER_INFO info;
    COORD destination;
    CHAR_INFO fill;
    unsigned short height;
    unsigned short rc;
    (void)opaque;
    if (cell == NULL)
        return OS2_VIO_ERROR_INVALID_PARAMETER;
    rc = output_handle(&handle);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    rc = screen_info(&handle, &info);
    if (rc != OS2_VIO_NO_ERROR) return rc;
    if (bottom > info.srWindow.Bottom - info.srWindow.Top ||
        right > info.srWindow.Right - info.srWindow.Left)
        return OS2_VIO_ERROR_INVALID_PARAMETER;
    top = (unsigned short)(top + info.srWindow.Top);
    bottom = (unsigned short)(bottom + info.srWindow.Top);
    left = (unsigned short)(left + info.srWindow.Left);
    right = (unsigned short)(right + info.srWindow.Left);
    clip.Top = (SHORT)top;
    clip.Bottom = (SHORT)bottom;
    clip.Left = (SHORT)left;
    clip.Right = (SHORT)right;
    height = (unsigned short)(bottom - top + 1U);
    if (lines >= height)
        return fill_rectangle(handle, top, left, bottom, right, cell);

    source.Left = (SHORT)left;
    source.Right = (SHORT)right;
    source.Top = (SHORT)(top + lines);
    source.Bottom = (SHORT)bottom;
    destination.X = (SHORT)left;
    destination.Y = (SHORT)top;
    fill.Char.AsciiChar = (char)cell[0];
    fill.Attributes = (WORD)os2_vio_win32_attribute(cell[1]);
    if (!ScrollConsoleScreenBufferA(handle, &source, &clip, destination, &fill))
        return host_error();
    return OS2_VIO_NO_ERROR;
}

static unsigned short win32_write_tty(void *opaque, const char *text,
                                      unsigned short count)
{
    HANDLE handle;
    DWORD done;
    unsigned short rc;
    (void)opaque;
    rc = output_handle(&handle);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    done = 0;
    if (!WriteFile(handle, text, (DWORD)count, &done, NULL))
        return host_error();
    if (done != (DWORD)count)
        return OS2_VIO_ERROR_INVALID_FUNCTION;
    return OS2_VIO_NO_ERROR;
}

static const struct Os2VioBackendOps win32_backend = {
    win32_query_size,
    win32_get_cursor_pos,
    win32_set_cursor_pos,
    win32_get_cursor_type,
    win32_set_cursor_type,
    win32_read_cells,
    win32_write_chars_at,
    win32_write_chars_attr_at,
    win32_write_attrs_at,
    win32_write_cell_at,
    win32_scroll_up,
    win32_write_tty
};

void os2_vio_win32_session_init(struct Os2VioSession *session,
                               struct Os2VioWin32State *state)
{
    state->session = session;
    state->viewport_valid = 0;
    os2_vio_session_init(session, state, &win32_backend);
}
