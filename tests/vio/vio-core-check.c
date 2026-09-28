#include <stdio.h>
#include <string.h>

#include "os2_vio.h"
#include "os2_vio_win32.h"

#define TEST_ROWS 5U
#define TEST_COLS 10U
#define TEST_CELLS (TEST_ROWS * TEST_COLS)

struct FakeVio {
    unsigned char chars[TEST_CELLS];
    unsigned char attrs[TEST_CELLS];
    unsigned short row;
    unsigned short column;
    struct Os2VioCursorInfo cursor;
    unsigned long tty_calls;
    unsigned long query_size_calls;
    unsigned long get_cursor_calls;
    unsigned long get_cursor_type_calls;
    unsigned long read_cells_calls;
};

static int failures;

static void expect_int(const char *name, unsigned long got, unsigned long want)
{
    if (got != want) {
        fprintf(stderr, "FAIL %s: got %lu want %lu\n", name, got, want);
        ++failures;
    }
}

static unsigned short fake_query_size(void *opaque, unsigned short *rows,
                                      unsigned short *columns)
{
    struct FakeVio *fake;
    fake = (struct FakeVio *)opaque;
    ++fake->query_size_calls;
    *rows = TEST_ROWS;
    *columns = TEST_COLS;
    return OS2_VIO_NO_ERROR;
}

static unsigned short fake_get_cursor_pos(void *opaque, unsigned short *row,
                                          unsigned short *column)
{
    struct FakeVio *fake;
    fake = (struct FakeVio *)opaque;
    ++fake->get_cursor_calls;
    *row = fake->row;
    *column = fake->column;
    return OS2_VIO_NO_ERROR;
}

static unsigned short fake_set_cursor_pos(void *opaque, unsigned short row,
                                          unsigned short column)
{
    struct FakeVio *fake;
    fake = (struct FakeVio *)opaque;
    fake->row = row;
    fake->column = column;
    return OS2_VIO_NO_ERROR;
}

static unsigned short fake_get_cursor_type(void *opaque,
                                            struct Os2VioCursorInfo *cursor)
{
    struct FakeVio *fake;
    fake = (struct FakeVio *)opaque;
    ++fake->get_cursor_type_calls;
    *cursor = fake->cursor;
    return OS2_VIO_NO_ERROR;
}

static unsigned short fake_set_cursor_type(
    void *opaque, const struct Os2VioCursorInfo *cursor)
{
    struct FakeVio *fake;
    fake = (struct FakeVio *)opaque;
    fake->cursor = *cursor;
    return OS2_VIO_NO_ERROR;
}


static unsigned short fake_read_cells(void *opaque, struct Os2VioCell *cells,
                                      unsigned short rows,
                                      unsigned short columns)
{
    struct FakeVio *fake;
    unsigned long i;
    unsigned long count;
    fake = (struct FakeVio *)opaque;
    ++fake->read_cells_calls;
    if (cells == NULL || rows != TEST_ROWS || columns != TEST_COLS)
        return OS2_VIO_ERROR_INVALID_PARAMETER;
    count = (unsigned long)rows * (unsigned long)columns;
    for (i = 0UL; i < count; ++i) {
        cells[i].character = fake->chars[i];
        cells[i].attribute = fake->attrs[i];
    }
    return OS2_VIO_NO_ERROR;
}

static unsigned short fake_write_chars_at(void *opaque, const char *text,
                                           unsigned short count,
                                           unsigned short row,
                                           unsigned short column)
{
    struct FakeVio *fake;
    unsigned long index;
    unsigned short i;
    fake = (struct FakeVio *)opaque;
    index = (unsigned long)row * TEST_COLS + column;
    for (i = 0U; i < count; ++i)
        fake->chars[index + i] = (unsigned char)text[i];
    return OS2_VIO_NO_ERROR;
}

static unsigned short fake_write_chars_attr_at(void *opaque, const char *text,
                                                unsigned short count,
                                                unsigned short row,
                                                unsigned short column,
                                                unsigned char attribute)
{
    struct FakeVio *fake;
    unsigned long index;
    unsigned short i;
    fake = (struct FakeVio *)opaque;
    index = (unsigned long)row * TEST_COLS + column;
    for (i = 0U; i < count; ++i) {
        fake->chars[index + i] = (unsigned char)text[i];
        fake->attrs[index + i] = attribute;
    }
    return OS2_VIO_NO_ERROR;
}

static unsigned short fake_write_attrs_at(void *opaque,
                                            unsigned char attribute,
                                            unsigned short count,
                                            unsigned short row,
                                            unsigned short column)
{
    struct FakeVio *fake;
    unsigned long index;
    unsigned short i;
    fake = (struct FakeVio *)opaque;
    index = (unsigned long)row * TEST_COLS + column;
    for (i = 0U; i < count; ++i)
        fake->attrs[index + i] = attribute;
    return OS2_VIO_NO_ERROR;
}

static unsigned short fake_write_cell_at(void *opaque,
                                           unsigned char character,
                                           unsigned char attribute,
                                           unsigned short count,
                                           unsigned short row,
                                           unsigned short column)
{
    struct FakeVio *fake;
    unsigned long index;
    unsigned short i;
    fake = (struct FakeVio *)opaque;
    index = (unsigned long)row * TEST_COLS + column;
    for (i = 0U; i < count; ++i) {
        fake->chars[index + i] = character;
        fake->attrs[index + i] = attribute;
    }
    return OS2_VIO_NO_ERROR;
}

static unsigned short fake_scroll_up(void *opaque, unsigned short top,
                                     unsigned short left,
                                     unsigned short bottom,
                                     unsigned short right,
                                     unsigned short lines,
                                     const unsigned char cell[2])
{
    struct FakeVio *fake;
    unsigned short row;
    unsigned short col;
    unsigned short source;
    unsigned long dst_index;
    unsigned long src_index;
    fake = (struct FakeVio *)opaque;

    for (row = top; row <= bottom; ++row) {
        source = (unsigned short)(row + lines);
        for (col = left; col <= right; ++col) {
            dst_index = (unsigned long)row * TEST_COLS + col;
            if (source <= bottom) {
                src_index = (unsigned long)source * TEST_COLS + col;
                fake->chars[dst_index] = fake->chars[src_index];
                fake->attrs[dst_index] = fake->attrs[src_index];
            } else {
                fake->chars[dst_index] = cell[0];
                fake->attrs[dst_index] = cell[1];
            }
        }
    }
    return OS2_VIO_NO_ERROR;
}

static unsigned short fake_write_tty(void *opaque, const char *text,
                                     unsigned short count)
{
    struct FakeVio *fake;
    unsigned short i;
    unsigned char ch;
    fake = (struct FakeVio *)opaque;
    ++fake->tty_calls;
    for (i = 0U; i < count; ++i) {
        ch = (unsigned char)text[i];
        if (ch == '\r') {
            fake->column = 0U;
        } else if (ch == '\n') {
            if (fake->row + 1U < TEST_ROWS)
                ++fake->row;
        } else {
            unsigned long index;
            index = (unsigned long)fake->row * TEST_COLS + fake->column;
            fake->chars[index] = ch;
            if (fake->column + 1U < TEST_COLS)
                ++fake->column;
        }
    }
    return OS2_VIO_NO_ERROR;
}

static const struct Os2VioBackendOps fake_ops = {
    fake_query_size,
    fake_get_cursor_pos,
    fake_set_cursor_pos,
    fake_get_cursor_type,
    fake_set_cursor_type,
    fake_read_cells,
    fake_write_chars_at,
    fake_write_chars_attr_at,
    fake_write_attrs_at,
    fake_write_cell_at,
    fake_scroll_up,
    fake_write_tty
};

static void init_fake(struct FakeVio *fake)
{
    unsigned long i;
    memset(fake, 0, sizeof(*fake));
    for (i = 0UL; i < TEST_CELLS; ++i) {
        fake->chars[i] = (unsigned char)'.';
        fake->attrs[i] = 0x07U;
    }
    fake->cursor.y_start = 14U;
    fake->cursor.c_end = 15U;
    fake->cursor.cx = 0U;
    fake->cursor.attr = 0U;
}

int main(void)
{
    struct FakeVio fake;
    struct Os2VioSession session;
    struct Os2VioCursorInfo cursor;
    struct Os2VioModeInfo mode;
    unsigned short row;
    unsigned short column;
    unsigned short rc;
    unsigned char cell[2];
    unsigned char attr;
    unsigned char read_cells[8];
    unsigned short read_bytes;
    char raw[3];
    unsigned long i;

    init_fake(&fake);
    os2_vio_session_init(&session, &fake, &fake_ops);

    memset(&mode, 0, sizeof(mode));
    mode.cb = (unsigned short)sizeof(mode);
    rc = os2_vio_get_mode(&session, &mode, 0U);
    expect_int("get mode rc", rc, 0U);
    expect_int("rows", mode.rows, TEST_ROWS);
    expect_int("columns", mode.columns, TEST_COLS);
    expect_int("cell count", session.cell_count, TEST_CELLS);
    expect_int("cells valid", (unsigned long)session.cells_valid, 1U);
    expect_int("initial shadow char", session.cells[0].character,
               (unsigned char)'.');
    expect_int("initial shadow attr", session.cells[0].attribute, 0x07U);

    rc = os2_vio_set_cur_pos(&session, 2U, 3U, 0U);
    expect_int("set cursor rc", rc, 0U);
    rc = os2_vio_get_cur_pos(&session, &row, &column, 0U);
    expect_int("get cursor rc", rc, 0U);
    expect_int("cursor row", row, 2U);
    expect_int("cursor column", column, 3U);

    cursor.y_start = 8U;
    cursor.c_end = 15U;
    cursor.cx = 0U;
    cursor.attr = 0xffffU;
    rc = os2_vio_set_cur_type(&session, &cursor, 0U);
    expect_int("set cursor type rc", rc, 0U);
    memset(&cursor, 0, sizeof(cursor));
    rc = os2_vio_get_cur_type(&session, &cursor, 0U);
    expect_int("get cursor type rc", rc, 0U);
    expect_int("cursor hidden", cursor.attr, 0xffffU);
    expect_int("cursor start", cursor.y_start, 8U);

    raw[0] = 1;
    raw[1] = 2;
    raw[2] = 'X';
    rc = os2_vio_write_char_str(&session, raw, 3U, 1U, 8U, 0U);
    expect_int("direct write rc", rc, 0U);
    expect_int("raw byte 1", fake.chars[18], 1U);
    expect_int("raw byte 2", fake.chars[19], 2U);
    expect_int("wrapped X", fake.chars[20], (unsigned char)'X');
    expect_int("direct write cursor row", fake.row, 2U);
    expect_int("direct write cursor col", fake.column, 3U);
    expect_int("shadow raw byte 1", session.cells[18].character, 1U);
    expect_int("shadow raw byte 2", session.cells[19].character, 2U);
    expect_int("shadow wrapped X", session.cells[20].character,
               (unsigned char)'X');
    expect_int("shadow direct attr preserved", session.cells[18].attribute,
               0x07U);

    attr = 0x1eU;
    rc = os2_vio_write_char_str_att(&session, "C", 1U, 3U, 4U,
                                    &attr, 0U);
    expect_int("attribute write rc", rc, 0U);
    expect_int("attribute char", fake.chars[34], (unsigned char)'C');
    expect_int("attribute raw", fake.attrs[34], 0x1eU);
    expect_int("shadow attribute char", session.cells[34].character,
               (unsigned char)'C');
    expect_int("shadow attribute raw", session.cells[34].attribute, 0x1eU);

    attr = 0x2aU;
    rc = os2_vio_write_n_attr(&session, &attr, 3U, 0U, 8U, 0U);
    expect_int("wrt n attr rc", rc, 0U);
    expect_int("wrt n attr 0", fake.attrs[8], 0x2aU);
    expect_int("wrt n attr wrap", fake.attrs[10], 0x2aU);
    expect_int("shadow wrt n attr", session.cells[10].attribute, 0x2aU);

    cell[0] = (unsigned char)'Z';
    cell[1] = 0x1bU;
    rc = os2_vio_write_n_cell(&session, cell, 3U, 2U, 8U, 0U);
    expect_int("wrt n cell rc", rc, 0U);
    expect_int("wrt n cell char", fake.chars[28], (unsigned char)'Z');
    expect_int("wrt n cell attr", fake.attrs[28], 0x1bU);
    expect_int("wrt n cell wrap", fake.chars[30], (unsigned char)'Z');

    read_bytes = (unsigned short)sizeof(read_cells);
    memset(read_cells, 0, sizeof(read_cells));
    rc = os2_vio_read_cell_str(&session, read_cells, &read_bytes,
                               2U, 8U, 0U);
    expect_int("read cell str rc", rc, 0U);
    expect_int("read cell str bytes", read_bytes, 8U);
    expect_int("read cell str char0", read_cells[0], (unsigned char)'Z');
    expect_int("read cell str attr0", read_cells[1], 0x1bU);
    expect_int("read cell str wrapped char", read_cells[4],
               (unsigned char)'Z');

    expect_int("win32 attr low 7", os2_vio_win32_attribute(0x1eU), 0x1eU);
    expect_int("win32 blink approximation", os2_vio_win32_attribute(0x9eU),
               0x1eU);

    rc = os2_vio_write_char_str_att(&session, "0123456789", 10U, 0U, 0U,
                                    &attr, 0U);
    expect_int("seed scroll row 0", rc, 0U);
    rc = os2_vio_write_char_str_att(&session, "abcdefghij", 10U, 1U, 0U,
                                    &attr, 0U);
    expect_int("seed scroll row 1", rc, 0U);
    cell[0] = (unsigned char)' ';
    cell[1] = 0x07U;
    rc = os2_vio_scroll_up(&session, 0U, 0U, 1U, 9U, 1U, cell, 0U);
    expect_int("partial scroll rc", rc, 0U);
    expect_int("partial scroll backend", fake.chars[0], (unsigned char)'a');
    expect_int("partial scroll shadow", session.cells[0].character,
               (unsigned char)'a');
    expect_int("partial scroll fill", session.cells[TEST_COLS].character,
               (unsigned char)' ');
    expect_int("partial scroll fill attr", session.cells[TEST_COLS].attribute,
               0x07U);

    cell[0] = (unsigned char)' ';
    cell[1] = 0x07U;
    rc = os2_vio_scroll_up(&session, 0U, 0U, 0xffffU, 0xffffU,
                           0xffffU, cell, 0U);
    expect_int("full clear rc", rc, 0U);
    for (i = 0UL; i < TEST_CELLS; ++i) {
        if (fake.chars[i] != (unsigned char)' ' || fake.attrs[i] != 0x07U) {
            fprintf(stderr, "FAIL full clear at %lu\n", i);
            ++failures;
            break;
        }
    }
    for (i = 0UL; i < TEST_CELLS; ++i) {
        if (session.cells[i].character != (unsigned char)' ' ||
            session.cells[i].attribute != 0x07U) {
            fprintf(stderr, "FAIL shadow full clear at %lu\n", i);
            ++failures;
            break;
        }
    }

    fake.row = 0U;
    fake.column = 0U;
    {
        unsigned long before_queries;
        unsigned long before_reads;
        unsigned long before_cursor;
        before_queries = fake.query_size_calls;
        before_reads = fake.read_cells_calls;
        before_cursor = fake.get_cursor_calls;
        rc = os2_vio_write_tty(&session, "A\r\nB", 4U, 0U);
        expect_int("tty rc", rc, 0U);
        expect_int("tty calls", fake.tty_calls, 1U);
        expect_int("tty no size poll", fake.query_size_calls, before_queries);
        expect_int("tty no screen snapshot", fake.read_cells_calls, before_reads);
        expect_int("tty no cursor poll", fake.get_cursor_calls, before_cursor);
        expect_int("tty invalidates shadow", (unsigned long)session.cells_valid,
                   0U);
    }
    rc = os2_vio_get_cur_pos(&session, &row, &column, 0U);
    expect_int("tty lazy cursor rc", rc, 0U);
    expect_int("tty cursor row", row, 1U);
    expect_int("tty cursor col", column, 1U);

    {
        unsigned long before_queries;
        before_queries = fake.query_size_calls;
        rc = os2_vio_write_char_str(&session, "Z", 1U, 0U, 0U, 0U);
        expect_int("direct hot write rc", rc, 0U);
        expect_int("direct hot write no size poll", fake.query_size_calls,
                   before_queries);
    }

    rc = os2_vio_get_cur_pos(&session, &row, &column, 1U);
    expect_int("invalid HVIO", rc, OS2_VIO_ERROR_INVALID_VIO_HANDLE);

    rc = os2_vio_set_cur_pos(&session, TEST_ROWS, 0U, 0U);
    expect_int("invalid row", rc, OS2_VIO_ERROR_INVALID_PARAMETER);

    {
        struct FakeVio cold_fake;
        struct Os2VioSession cold_session;
        init_fake(&cold_fake);
        os2_vio_session_init(&cold_session, &cold_fake, &fake_ops);
        rc = os2_vio_write_tty(&cold_session, "x", 1U, 0U);
        expect_int("cold tty rc", rc, 0U);
        expect_int("cold tty size polls", cold_fake.query_size_calls, 0U);
        expect_int("cold tty snapshots", cold_fake.read_cells_calls, 0U);
        expect_int("cold tty cursor polls", cold_fake.get_cursor_calls, 0U);
        expect_int("cold tty remains uninitialized",
                   (unsigned long)cold_session.initialized, 0U);
        os2_vio_session_destroy(&cold_session);
    }

    os2_vio_session_destroy(&session);
    expect_int("destroy cell count", session.cell_count, 0UL);
    expect_int("destroy cells valid", (unsigned long)session.cells_valid, 0U);

    if (failures != 0) {
        fprintf(stderr, "vio-core-check: %d failure(s)\n", failures);
        return 1;
    }
    puts("vio-core-check: PASS");
    return 0;
}
