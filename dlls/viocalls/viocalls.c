/*
 * VIOCALLS.dll - public OS/2 VIO facade.
 *
 * VIO R2 keeps the historical exported signatures/ordinals here while OS/2
 * text-mode semantics and state live in common/vio.  Win32 Console is the
 * first and only backend in this milestone.
 */

#include "os2_vio.h"
#include "os2_vio_win32.h"

#ifndef __cdecl
#define __cdecl
#endif

static struct Os2VioSession vio_session;
static int vio_session_ready;

static struct Os2VioSession *session(void)
{
    if (!vio_session_ready) {
        os2_vio_win32_session_init(&vio_session);
        vio_session_ready = 1;
    }
    return &vio_session;
}

unsigned short __cdecl VioWrtTTY(const char *text, unsigned short count,
                                  unsigned short hvio)
{
    return os2_vio_write_tty(session(), text, count, hvio);
}

unsigned short __cdecl VioGetCurPos(unsigned short *row, unsigned short *col,
                                     unsigned short hvio)
{
    return os2_vio_get_cur_pos(session(), row, col, hvio);
}

unsigned short __cdecl VioSetCurPos(unsigned short row, unsigned short col,
                                     unsigned short hvio)
{
    return os2_vio_set_cur_pos(session(), row, col, hvio);
}

unsigned short __cdecl VioGetCurType(struct Os2VioCursorInfo *cursor,
                                      unsigned short hvio)
{
    return os2_vio_get_cur_type(session(), cursor, hvio);
}

unsigned short __cdecl VioSetCurType(const struct Os2VioCursorInfo *cursor,
                                      unsigned short hvio)
{
    return os2_vio_set_cur_type(session(), cursor, hvio);
}

unsigned short __cdecl VioGetMode(struct Os2VioModeInfo *mode,
                                   unsigned short hvio)
{
    return os2_vio_get_mode(session(), mode, hvio);
}

unsigned short __cdecl VioWrtCharStr(const char *text, unsigned short count,
                                      unsigned short row, unsigned short col,
                                      unsigned short hvio)
{
    return os2_vio_write_char_str(session(), text, count, row, col, hvio);
}

unsigned short __cdecl VioWrtCharStrAtt(const char *text,
                                         unsigned short count,
                                         unsigned short row,
                                         unsigned short col,
                                         const unsigned char *attribute,
                                         unsigned short hvio)
{
    return os2_vio_write_char_str_att(session(), text, count, row, col,
                                      attribute, hvio);
}

unsigned short __cdecl VioReadCellStr(unsigned char *cells,
                                       unsigned short *byte_count,
                                       unsigned short row,
                                       unsigned short col,
                                       unsigned short hvio)
{
    return os2_vio_read_cell_str(session(), cells, byte_count, row, col, hvio);
}

unsigned short __cdecl VioWrtNAttr(const unsigned char *attribute,
                                    unsigned short count,
                                    unsigned short row,
                                    unsigned short col,
                                    unsigned short hvio)
{
    return os2_vio_write_n_attr(session(), attribute, count, row, col, hvio);
}

unsigned short __cdecl VioWrtNCell(const unsigned char *cell,
                                    unsigned short count,
                                    unsigned short row,
                                    unsigned short col,
                                    unsigned short hvio)
{
    return os2_vio_write_n_cell(session(), cell, count, row, col, hvio);
}

unsigned short __cdecl VioScrollUp(unsigned short top, unsigned short left,
                                    unsigned short bottom,
                                    unsigned short right,
                                    unsigned short lines,
                                    const unsigned char *cell,
                                    unsigned short hvio)
{
    return os2_vio_scroll_up(session(), top, left, bottom, right, lines,
                             cell, hvio);
}
