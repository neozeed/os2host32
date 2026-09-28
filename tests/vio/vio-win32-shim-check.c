#include <stdio.h>
#include <string.h>

#include "os2_vio.h"
#include "win32_vio_stub.h"

unsigned short VioWrtTTY(const char *text, unsigned short count,
                         unsigned short hvio);
unsigned short VioGetCurPos(unsigned short *row, unsigned short *column,
                            unsigned short hvio);
unsigned short VioSetCurPos(unsigned short row, unsigned short column,
                            unsigned short hvio);
unsigned short VioGetCurType(struct Os2VioCursorInfo *cursor,
                             unsigned short hvio);
unsigned short VioSetCurType(const struct Os2VioCursorInfo *cursor,
                             unsigned short hvio);
unsigned short VioGetMode(struct Os2VioModeInfo *mode, unsigned short hvio);
unsigned short VioWrtCharStr(const char *text, unsigned short count,
                             unsigned short row, unsigned short column,
                             unsigned short hvio);
unsigned short VioWrtCharStrAtt(const char *text, unsigned short count,
                                unsigned short row, unsigned short column,
                                const unsigned char *attribute,
                                unsigned short hvio);
unsigned short VioReadCellStr(unsigned char *cells, unsigned short *byte_count,
                              unsigned short row, unsigned short column,
                              unsigned short hvio);
unsigned short VioWrtNAttr(const unsigned char *attribute, unsigned short count,
                           unsigned short row, unsigned short column,
                           unsigned short hvio);
unsigned short VioWrtNCell(const unsigned char *cell, unsigned short count,
                           unsigned short row, unsigned short column,
                           unsigned short hvio);
unsigned short VioScrollUp(unsigned short top, unsigned short left,
                           unsigned short bottom, unsigned short right,
                           unsigned short lines, const unsigned char *cell,
                           unsigned short hvio);

static int failures;

static void expect(const char *name, unsigned long got, unsigned long want)
{
    if (got != want) {
        fprintf(stderr, "FAIL %s: got %lu want %lu\n", name, got, want);
        ++failures;
    }
}

int main(void)
{
    struct Os2VioModeInfo mode;
    struct Os2VioCursorInfo saved;
    struct Os2VioCursorInfo hidden;
    unsigned short row;
    unsigned short column;
    unsigned short rc;
    unsigned char attr;
    unsigned char cell[2];
    unsigned char read_cells[8];
    unsigned short read_bytes;
    char raw[3];
    unsigned short r;
    unsigned short c;

    win32_vio_stub_reset();
    memset(&mode, 0, sizeof(mode));
    mode.cb = (unsigned short)sizeof(mode);
    rc = VioGetMode(&mode, 0U);
    expect("get mode rc", rc, 0U);
    expect("mode rows", mode.rows, 5U);
    expect("mode columns", mode.columns, 10U);

    rc = VioSetCurPos(2U, 3U, 0U);
    expect("set cur pos", rc, 0U);
    rc = VioGetCurPos(&row, &column, 0U);
    expect("get cur pos", rc, 0U);
    expect("row", row, 2U);
    expect("column", column, 3U);

    rc = VioGetCurType(&saved, 0U);
    expect("get cursor type", rc, 0U);
    hidden = saved;
    hidden.attr = 0xffffU;
    rc = VioSetCurType(&hidden, 0U);
    expect("hide cursor", rc, 0U);
    expect("cursor visible false", (unsigned long)win32_vio_stub_cursor_visible(),
           0U);
    rc = VioSetCurType(&saved, 0U);
    expect("restore cursor", rc, 0U);
    expect("cursor visible true", (unsigned long)win32_vio_stub_cursor_visible(),
           1U);

    raw[0] = 1;
    raw[1] = 2;
    raw[2] = 'X';
    rc = VioWrtCharStr(raw, 3U, 1U, 8U, 0U);
    expect("wrt char str", rc, 0U);
    expect("raw 1", win32_vio_stub_char(1U, 8U), 1U);
    expect("raw 2", win32_vio_stub_char(1U, 9U), 2U);
    expect("raw wrap", win32_vio_stub_char(2U, 0U), (unsigned char)'X');
    expect("direct cursor row unchanged", win32_vio_stub_cursor_row(), 2U);
    expect("direct cursor col unchanged", win32_vio_stub_cursor_column(), 3U);

    attr = 0x9eU;
    rc = VioWrtCharStrAtt("A", 1U, 3U, 4U, &attr, 0U);
    expect("wrt char str att", rc, 0U);
    expect("att char", win32_vio_stub_char(3U, 4U), (unsigned char)'A');
    expect("att map", win32_vio_stub_attr(3U, 4U), 0x1eU);

    attr = 0x2aU;
    rc = VioWrtNAttr(&attr, 3U, 0U, 8U, 0U);
    expect("wrt n attr", rc, 0U);
    expect("wrt n attr first", win32_vio_stub_attr(0U, 8U), 0x2aU);
    expect("wrt n attr wrap", win32_vio_stub_attr(1U, 0U), 0x2aU);

    cell[0] = (unsigned char)'Z';
    cell[1] = 0x1bU;
    rc = VioWrtNCell(cell, 3U, 2U, 8U, 0U);
    expect("wrt n cell", rc, 0U);
    expect("wrt n cell char", win32_vio_stub_char(2U, 8U),
           (unsigned char)'Z');
    expect("wrt n cell attr", win32_vio_stub_attr(2U, 8U), 0x1bU);
    expect("wrt n cell wrap", win32_vio_stub_char(3U, 0U),
           (unsigned char)'Z');

    read_bytes = (unsigned short)sizeof(read_cells);
    memset(read_cells, 0, sizeof(read_cells));
    rc = VioReadCellStr(read_cells, &read_bytes, 2U, 8U, 0U);
    expect("read cell str", rc, 0U);
    expect("read cell bytes", read_bytes, 8U);
    expect("read cell char", read_cells[0], (unsigned char)'Z');
    expect("read cell attr", read_cells[1], 0x1bU);
    expect("read cell wrapped", read_cells[4], (unsigned char)'Z');

    cell[0] = (unsigned char)' ';
    cell[1] = 0x07U;
    rc = VioScrollUp(0U, 0U, 0xffffU, 0xffffU, 0xffffU, cell, 0U);
    expect("clear", rc, 0U);
    for (r = 0U; r < 5U; ++r) {
        for (c = 0U; c < 10U; ++c) {
            if (win32_vio_stub_char(r, c) != (unsigned char)' ' ||
                win32_vio_stub_attr(r, c) != 0x07U) {
                fprintf(stderr, "FAIL clear cell %u,%u\n", r, c);
                ++failures;
                r = 5U;
                break;
            }
        }
    }

    rc = VioSetCurPos(0U, 0U, 0U);
    expect("tty set home", rc, 0U);
    rc = VioWrtTTY("A\r\nB", 4U, 0U);
    expect("tty", rc, 0U);
    expect("tty A", win32_vio_stub_char(0U, 0U), (unsigned char)'A');
    expect("tty B", win32_vio_stub_char(1U, 0U), (unsigned char)'B');
    expect("tty cursor row", win32_vio_stub_cursor_row(), 1U);
    expect("tty cursor column", win32_vio_stub_cursor_column(), 1U);

    rc = VioGetCurPos(&row, &column, 7U);
    expect("invalid hvio", rc, OS2_VIO_ERROR_INVALID_VIO_HANDLE);

    if (failures != 0) {
        fprintf(stderr, "vio-win32-shim-check: %d failure(s)\n", failures);
        return 1;
    }
    puts("vio-win32-shim-check: PASS");
    return 0;
}
