#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "os2_vio.h"

static unsigned short validate_session(const struct Os2VioSession *session)
{
    if (session == NULL || session->backend == NULL)
        return OS2_VIO_ERROR_INVALID_PARAMETER;
    return OS2_VIO_NO_ERROR;
}

static unsigned short validate_hvio(unsigned short hvio)
{
    if (hvio != 0U)
        return OS2_VIO_ERROR_INVALID_VIO_HANDLE;
    return OS2_VIO_NO_ERROR;
}

static unsigned short allocate_cells(struct Os2VioSession *session,
                                     unsigned short rows,
                                     unsigned short columns)
{
    size_t count;
    size_t bytes;
    struct Os2VioCell *cells;

    if (rows == 0U || columns == 0U)
        return OS2_VIO_ERROR_INVALID_FUNCTION;
    count = (size_t)rows * (size_t)columns;
    if (count / (size_t)columns != (size_t)rows)
        return OS2_VIO_ERROR_NOT_ENOUGH_MEMORY;
    bytes = count * sizeof(*cells);
    if (bytes / sizeof(*cells) != count)
        return OS2_VIO_ERROR_NOT_ENOUGH_MEMORY;

    cells = (struct Os2VioCell *)realloc(session->cells, bytes);
    if (cells == NULL)
        return OS2_VIO_ERROR_NOT_ENOUGH_MEMORY;
    session->cells = cells;
    session->cell_count = (unsigned long)count;
    session->rows = rows;
    session->columns = columns;
    session->cells_valid = 0;
    return OS2_VIO_NO_ERROR;
}

static unsigned short snapshot_cells(struct Os2VioSession *session)
{
    unsigned short rc;

    if (session->backend->read_cells == NULL)
        return OS2_VIO_ERROR_INVALID_FUNCTION;
    rc = session->backend->read_cells(session->backend_opaque,
                                      session->cells,
                                      session->rows,
                                      session->columns);
    if (rc != OS2_VIO_NO_ERROR) {
        session->cells_valid = 0;
        return rc;
    }
    session->cells_valid = 1;
    return OS2_VIO_NO_ERROR;
}

static unsigned short refresh_size(struct Os2VioSession *session)
{
    unsigned short rows;
    unsigned short columns;
    unsigned short rc;
    int changed;

    if (session->backend->query_size == NULL)
        return OS2_VIO_ERROR_INVALID_FUNCTION;
    rows = 0U;
    columns = 0U;
    rc = session->backend->query_size(session->backend_opaque,
                                      &rows, &columns);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    if (rows == 0U || columns == 0U)
        return OS2_VIO_ERROR_INVALID_FUNCTION;

    changed = session->cells == NULL || rows != session->rows ||
              columns != session->columns;
    if (!changed)
        return OS2_VIO_NO_ERROR;

    rc = allocate_cells(session, rows, columns);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    return snapshot_cells(session);
}

static unsigned short ensure_initialized(struct Os2VioSession *session)
{
    unsigned short rc;

    rc = validate_session(session);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;

    /* R2B hot-path rule: once VIO has captured the current mode/state, do
     * not query the host console again for every OS/2 operation.  OS/2 VIO
     * owns this logical state.  VioGetMode performs the explicit resize
     * check because that is the API where callers ask about dimensions. */
    if (session->initialized)
        return OS2_VIO_NO_ERROR;

    rc = refresh_size(session);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    if (session->backend->get_cursor_pos == NULL ||
        session->backend->get_cursor_type == NULL)
        return OS2_VIO_ERROR_INVALID_FUNCTION;
    rc = session->backend->get_cursor_pos(session->backend_opaque,
                                          &session->cursor_row,
                                          &session->cursor_column);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    rc = session->backend->get_cursor_type(session->backend_opaque,
                                           &session->cursor_type);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    session->initialized = 1;
    return OS2_VIO_NO_ERROR;
}

static void tty_invalidate_cells(struct Os2VioSession *session)
{
    /* VioWrtTTY deliberately remains a host stream operation in R2B.
     * Do not read the console back after a TTY write.  The stream may have
     * interpreted CR/LF/BS, wrapped, or scrolled, so the optional cell image
     * is no longer authoritative.  Cursor position is queried lazily by
     * VioGetCurPos when a caller actually asks for it. */
    if (session != NULL && session->initialized)
        session->cells_valid = 0;
}

void os2_vio_session_init(struct Os2VioSession *session,
                          void *backend_opaque,
                          const struct Os2VioBackendOps *backend)
{
    if (session == NULL)
        return;
    memset(session, 0, sizeof(*session));
    session->backend_opaque = backend_opaque;
    session->backend = backend;
    session->default_attribute = 0x07U;
}

void os2_vio_session_destroy(struct Os2VioSession *session)
{
    if (session == NULL)
        return;
    free(session->cells);
    session->cells = NULL;
    session->cell_count = 0UL;
    session->cells_valid = 0;
    session->initialized = 0;
}

unsigned short os2_vio_get_cur_pos(struct Os2VioSession *session,
                                   unsigned short *row, unsigned short *column,
                                   unsigned short hvio)
{
    unsigned short rc;

    rc = validate_hvio(hvio);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    if (row == NULL || column == NULL)
        return OS2_VIO_ERROR_INVALID_PARAMETER;
    rc = ensure_initialized(session);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    rc = session->backend->get_cursor_pos(session->backend_opaque,
                                          &session->cursor_row,
                                          &session->cursor_column);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    *row = session->cursor_row;
    *column = session->cursor_column;
    return OS2_VIO_NO_ERROR;
}

unsigned short os2_vio_set_cur_pos(struct Os2VioSession *session,
                                   unsigned short row, unsigned short column,
                                   unsigned short hvio)
{
    unsigned short rc;

    rc = validate_hvio(hvio);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    rc = ensure_initialized(session);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    if (row >= session->rows || column >= session->columns)
        return OS2_VIO_ERROR_INVALID_PARAMETER;
    if (session->backend->set_cursor_pos == NULL)
        return OS2_VIO_ERROR_INVALID_FUNCTION;
    rc = session->backend->set_cursor_pos(session->backend_opaque, row, column);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    session->cursor_row = row;
    session->cursor_column = column;
    return OS2_VIO_NO_ERROR;
}

unsigned short os2_vio_get_cur_type(struct Os2VioSession *session,
                                    struct Os2VioCursorInfo *cursor,
                                    unsigned short hvio)
{
    unsigned short rc;

    rc = validate_hvio(hvio);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    if (cursor == NULL)
        return OS2_VIO_ERROR_INVALID_PARAMETER;
    rc = ensure_initialized(session);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    rc = session->backend->get_cursor_type(session->backend_opaque,
                                           &session->cursor_type);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    *cursor = session->cursor_type;
    return OS2_VIO_NO_ERROR;
}

unsigned short os2_vio_set_cur_type(struct Os2VioSession *session,
                                    const struct Os2VioCursorInfo *cursor,
                                    unsigned short hvio)
{
    unsigned short rc;

    rc = validate_hvio(hvio);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    if (cursor == NULL)
        return OS2_VIO_ERROR_INVALID_PARAMETER;
    rc = ensure_initialized(session);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    if (session->backend->set_cursor_type == NULL)
        return OS2_VIO_ERROR_INVALID_FUNCTION;
    rc = session->backend->set_cursor_type(session->backend_opaque, cursor);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    session->cursor_type = *cursor;
    return OS2_VIO_NO_ERROR;
}

unsigned short os2_vio_get_mode(struct Os2VioSession *session,
                                struct Os2VioModeInfo *mode,
                                unsigned short hvio)
{
    unsigned short rc;

    rc = validate_hvio(hvio);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    if (mode == NULL || mode->cb < 8U)
        return OS2_VIO_ERROR_INVALID_PARAMETER;
    rc = ensure_initialized(session);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    /* Mode queries are the one normal hot-path exception: notice a host
     * console resize here rather than polling dimensions on every VIO call. */
    rc = refresh_size(session);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    mode->fb_type = 0U;
    mode->color = 16U;
    mode->columns = session->columns;
    mode->rows = session->rows;
    /* cb can describe only the eight-byte mandatory prefix. */
    if (mode->cb >= 10U) mode->hres = 0U;
    if (mode->cb >= 12U) mode->vres = 0U;
    return OS2_VIO_NO_ERROR;
}

static unsigned short clipped_count(struct Os2VioSession *session,
                                    unsigned short row, unsigned short column,
                                    unsigned short count,
                                    unsigned short *result)
{
    unsigned long available;

    if (row >= session->rows || column >= session->columns)
        return OS2_VIO_ERROR_INVALID_PARAMETER;
    available = ((unsigned long)session->rows - (unsigned long)row) *
                (unsigned long)session->columns;
    available -= (unsigned long)column;
    if ((unsigned long)count > available)
        count = (unsigned short)available;
    *result = count;
    return OS2_VIO_NO_ERROR;
}

static void shadow_write_chars(struct Os2VioSession *session,
                               const char *text, unsigned short count,
                               unsigned short row, unsigned short column)
{
    unsigned long index;
    unsigned short i;

    if (!session->cells_valid)
        return;
    index = (unsigned long)row * (unsigned long)session->columns +
            (unsigned long)column;
    for (i = 0U; i < count; ++i)
        session->cells[index + i].character = (unsigned char)text[i];
}

static void shadow_write_chars_attr(struct Os2VioSession *session,
                                    const char *text, unsigned short count,
                                    unsigned short row, unsigned short column,
                                    unsigned char attribute)
{
    unsigned long index;
    unsigned short i;

    if (!session->cells_valid)
        return;
    index = (unsigned long)row * (unsigned long)session->columns +
            (unsigned long)column;
    for (i = 0U; i < count; ++i) {
        session->cells[index + i].character = (unsigned char)text[i];
        session->cells[index + i].attribute = attribute;
    }
}

unsigned short os2_vio_write_char_str(struct Os2VioSession *session,
                                      const char *text, unsigned short count,
                                      unsigned short row, unsigned short column,
                                      unsigned short hvio)
{
    unsigned short rc;
    unsigned short write_count;

    rc = validate_hvio(hvio);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    if (text == NULL && count != 0U)
        return OS2_VIO_ERROR_INVALID_PARAMETER;
    rc = ensure_initialized(session);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    rc = clipped_count(session, row, column, count, &write_count);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    if (write_count == 0U)
        return OS2_VIO_NO_ERROR;
    if (session->backend->write_chars_at == NULL)
        return OS2_VIO_ERROR_INVALID_FUNCTION;
    rc = session->backend->write_chars_at(session->backend_opaque,
                                          text, write_count, row, column);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    shadow_write_chars(session, text, write_count, row, column);
    return OS2_VIO_NO_ERROR;
}

unsigned short os2_vio_write_char_str_att(struct Os2VioSession *session,
                                          const char *text,
                                          unsigned short count,
                                          unsigned short row,
                                          unsigned short column,
                                          const unsigned char *attribute,
                                          unsigned short hvio)
{
    unsigned short rc;
    unsigned short write_count;

    rc = validate_hvio(hvio);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    if ((text == NULL || attribute == NULL) && count != 0U)
        return OS2_VIO_ERROR_INVALID_PARAMETER;
    rc = ensure_initialized(session);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    rc = clipped_count(session, row, column, count, &write_count);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    if (write_count == 0U)
        return OS2_VIO_NO_ERROR;
    if (session->backend->write_chars_attr_at == NULL)
        return OS2_VIO_ERROR_INVALID_FUNCTION;
    rc = session->backend->write_chars_attr_at(session->backend_opaque,
                                               text, write_count,
                                               row, column, *attribute);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    shadow_write_chars_attr(session, text, write_count, row, column,
                            *attribute);
    return OS2_VIO_NO_ERROR;
}


static unsigned short ensure_cells_valid(struct Os2VioSession *session)
{
    if (session->cells_valid)
        return OS2_VIO_NO_ERROR;
    return snapshot_cells(session);
}

static void shadow_write_attrs(struct Os2VioSession *session,
                               unsigned char attribute, unsigned short count,
                               unsigned short row, unsigned short column)
{
    unsigned long index;
    unsigned short i;

    if (!session->cells_valid)
        return;
    index = (unsigned long)row * (unsigned long)session->columns +
            (unsigned long)column;
    for (i = 0U; i < count; ++i)
        session->cells[index + i].attribute = attribute;
}

static void shadow_write_cell(struct Os2VioSession *session,
                              unsigned char character,
                              unsigned char attribute, unsigned short count,
                              unsigned short row, unsigned short column)
{
    unsigned long index;
    unsigned short i;

    if (!session->cells_valid)
        return;
    index = (unsigned long)row * (unsigned long)session->columns +
            (unsigned long)column;
    for (i = 0U; i < count; ++i) {
        session->cells[index + i].character = character;
        session->cells[index + i].attribute = attribute;
    }
}

unsigned short os2_vio_read_cell_str(struct Os2VioSession *session,
                                     unsigned char *cells,
                                     unsigned short *byte_count,
                                     unsigned short row,
                                     unsigned short column,
                                     unsigned short hvio)
{
    unsigned short rc;
    unsigned short requested;
    unsigned short cell_count;
    unsigned short actual_cells;
    unsigned short i;
    unsigned long index;

    rc = validate_hvio(hvio);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    if (byte_count == NULL)
        return OS2_VIO_ERROR_INVALID_PARAMETER;
    requested = (unsigned short)(*byte_count & 0xfffeU);
    if (cells == NULL && requested != 0U)
        return OS2_VIO_ERROR_INVALID_PARAMETER;
    rc = ensure_initialized(session);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    rc = clipped_count(session, row, column,
                       (unsigned short)(requested / 2U), &cell_count);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    actual_cells = cell_count;
    if (actual_cells == 0U) {
        *byte_count = 0U;
        return OS2_VIO_NO_ERROR;
    }
    rc = ensure_cells_valid(session);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    index = (unsigned long)row * (unsigned long)session->columns +
            (unsigned long)column;
    for (i = 0U; i < actual_cells; ++i) {
        cells[(unsigned short)(i * 2U)] =
            session->cells[index + i].character;
        cells[(unsigned short)(i * 2U + 1U)] =
            session->cells[index + i].attribute;
    }
    *byte_count = (unsigned short)(actual_cells * 2U);
    return OS2_VIO_NO_ERROR;
}

unsigned short os2_vio_write_n_attr(struct Os2VioSession *session,
                                    const unsigned char *attribute,
                                    unsigned short count,
                                    unsigned short row,
                                    unsigned short column,
                                    unsigned short hvio)
{
    unsigned short rc;
    unsigned short write_count;

    rc = validate_hvio(hvio);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    if (attribute == NULL && count != 0U)
        return OS2_VIO_ERROR_INVALID_PARAMETER;
    rc = ensure_initialized(session);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    rc = clipped_count(session, row, column, count, &write_count);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    if (write_count == 0U)
        return OS2_VIO_NO_ERROR;
    if (session->backend->write_attrs_at == NULL)
        return OS2_VIO_ERROR_INVALID_FUNCTION;
    rc = session->backend->write_attrs_at(session->backend_opaque, *attribute,
                                          write_count, row, column);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    shadow_write_attrs(session, *attribute, write_count, row, column);
    return OS2_VIO_NO_ERROR;
}

unsigned short os2_vio_write_n_cell(struct Os2VioSession *session,
                                    const unsigned char *cell,
                                    unsigned short count,
                                    unsigned short row,
                                    unsigned short column,
                                    unsigned short hvio)
{
    unsigned short rc;
    unsigned short write_count;

    rc = validate_hvio(hvio);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    if (cell == NULL && count != 0U)
        return OS2_VIO_ERROR_INVALID_PARAMETER;
    rc = ensure_initialized(session);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    rc = clipped_count(session, row, column, count, &write_count);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    if (write_count == 0U)
        return OS2_VIO_NO_ERROR;
    if (session->backend->write_cell_at == NULL)
        return OS2_VIO_ERROR_INVALID_FUNCTION;
    rc = session->backend->write_cell_at(session->backend_opaque, cell[0],
                                         cell[1], write_count, row, column);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    shadow_write_cell(session, cell[0], cell[1], write_count, row, column);
    return OS2_VIO_NO_ERROR;
}

static void shadow_scroll_up(struct Os2VioSession *session,
                             unsigned short top, unsigned short left,
                             unsigned short bottom, unsigned short right,
                             unsigned short lines,
                             const unsigned char cell[2])
{
    unsigned short row;
    unsigned short column;
    unsigned short source_row;
    unsigned long dst;
    unsigned long src;

    if (!session->cells_valid)
        return;
    for (row = top; row <= bottom; ++row) {
        source_row = (unsigned short)(row + lines);
        for (column = left; column <= right; ++column) {
            dst = (unsigned long)row * (unsigned long)session->columns +
                  (unsigned long)column;
            if (source_row <= bottom) {
                src = (unsigned long)source_row *
                      (unsigned long)session->columns +
                      (unsigned long)column;
                session->cells[dst] = session->cells[src];
            } else {
                session->cells[dst].character = cell[0];
                session->cells[dst].attribute = cell[1];
            }
        }
    }
}

unsigned short os2_vio_scroll_up(struct Os2VioSession *session,
                                 unsigned short top, unsigned short left,
                                 unsigned short bottom, unsigned short right,
                                 unsigned short lines,
                                 const unsigned char *cell,
                                 unsigned short hvio)
{
    unsigned short rc;
    unsigned short height;
    unsigned char local_cell[2];

    rc = validate_hvio(hvio);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    rc = ensure_initialized(session);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;

    if (bottom == 0xffffU || bottom >= session->rows)
        bottom = (unsigned short)(session->rows - 1U);
    if (right == 0xffffU || right >= session->columns)
        right = (unsigned short)(session->columns - 1U);
    if (top > bottom || left > right)
        return OS2_VIO_ERROR_INVALID_PARAMETER;

    if (cell != NULL) {
        local_cell[0] = cell[0];
        local_cell[1] = cell[1];
    } else {
        local_cell[0] = (unsigned char)' ';
        local_cell[1] = session->default_attribute;
    }

    height = (unsigned short)(bottom - top + 1U);
    /* Preserve the pre-R2 host behavior for a zero count; Life and CMD use
     * 0xffff for the historical full-region clear idiom. */
    if (lines == 0U || lines >= height)
        lines = height;
    if (session->backend->scroll_up == NULL)
        return OS2_VIO_ERROR_INVALID_FUNCTION;
    rc = session->backend->scroll_up(session->backend_opaque,
                                     top, left, bottom, right, lines,
                                     local_cell);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    shadow_scroll_up(session, top, left, bottom, right, lines, local_cell);
    return OS2_VIO_NO_ERROR;
}

unsigned short os2_vio_write_tty(struct Os2VioSession *session,
                                 const char *text, unsigned short count,
                                 unsigned short hvio)
{
    unsigned short rc;

    rc = validate_hvio(hvio);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    if (text == NULL && count != 0U)
        return OS2_VIO_ERROR_INVALID_PARAMETER;
    rc = validate_session(session);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;
    if (count == 0U)
        return OS2_VIO_NO_ERROR;
    if (session->backend->write_tty == NULL)
        return OS2_VIO_ERROR_INVALID_FUNCTION;
    rc = session->backend->write_tty(session->backend_opaque, text, count);
    if (rc != OS2_VIO_NO_ERROR)
        return rc;

    /* TTY remains a host stream operation.  Never resnapshot the complete
     * console on this hot path: CMD and many text programs emit tiny writes.
     * Mark the optional cell image dirty; cursor position is queried lazily
     * if a caller subsequently asks for it. */
    tty_invalidate_cells(session);
    return OS2_VIO_NO_ERROR;
}
