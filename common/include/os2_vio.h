#ifndef OS2_VIO_H
#define OS2_VIO_H

/*
 * Backend-neutral OS/2 base VIO state used by the native Win32 host.
 *
 * R2 makes the text-cell image an OS/2-side concept rather than a Win32
 * implementation detail.  Win32 remains only the renderer/backend here.
 * This is not an AVIO, PM, ANSI, or VioRegister implementation.
 */

#define OS2_VIO_NO_ERROR                 0U
#define OS2_VIO_ERROR_INVALID_FUNCTION   1U
#define OS2_VIO_ERROR_INVALID_HANDLE     6U
#define OS2_VIO_ERROR_NOT_ENOUGH_MEMORY  8U
#define OS2_VIO_ERROR_INVALID_PARAMETER 87U
#define OS2_VIO_ERROR_INVALID_VIO_HANDLE 436U

struct Os2VioCursorInfo {
    unsigned short y_start;
    unsigned short c_end;
    unsigned short cx;
    unsigned short attr;
};

struct Os2VioModeInfo {
    unsigned short cb;
    unsigned char fb_type;
    unsigned char color;
    unsigned short columns;
    unsigned short rows;
    unsigned short hres;
    unsigned short vres;
    unsigned char fmt_id;
    unsigned char attrib;
    unsigned long buf_addr;
    unsigned long buf_length;
    unsigned long full_length;
    unsigned long partial_length;
    char *ext_data_addr;
};

struct Os2VioCell {
    unsigned char character;
    unsigned char attribute;
};

struct Os2VioBackendOps {
    unsigned short (*query_size)(void *opaque, unsigned short *rows,
                                 unsigned short *columns);
    unsigned short (*get_cursor_pos)(void *opaque, unsigned short *row,
                                     unsigned short *column);
    unsigned short (*set_cursor_pos)(void *opaque, unsigned short row,
                                     unsigned short column);
    unsigned short (*get_cursor_type)(void *opaque,
                                      struct Os2VioCursorInfo *cursor);
    unsigned short (*set_cursor_type)(void *opaque,
                                      const struct Os2VioCursorInfo *cursor);
    unsigned short (*read_cells)(void *opaque, struct Os2VioCell *cells,
                                 unsigned short rows,
                                 unsigned short columns);
    unsigned short (*write_chars_at)(void *opaque, const char *text,
                                     unsigned short count,
                                     unsigned short row,
                                     unsigned short column);
    unsigned short (*write_chars_attr_at)(void *opaque, const char *text,
                                          unsigned short count,
                                          unsigned short row,
                                          unsigned short column,
                                          unsigned char attribute);
    unsigned short (*write_attrs_at)(void *opaque, unsigned char attribute,
                                     unsigned short count,
                                     unsigned short row,
                                     unsigned short column);
    unsigned short (*write_cell_at)(void *opaque, unsigned char character,
                                    unsigned char attribute,
                                    unsigned short count,
                                    unsigned short row,
                                    unsigned short column);
    unsigned short (*scroll_up)(void *opaque, unsigned short top,
                                unsigned short left, unsigned short bottom,
                                unsigned short right, unsigned short lines,
                                const unsigned char cell[2]);
    unsigned short (*write_tty)(void *opaque, const char *text,
                                unsigned short count);
};

struct Os2VioSession {
    void *backend_opaque;
    const struct Os2VioBackendOps *backend;
    int initialized;
    unsigned short rows;
    unsigned short columns;
    unsigned short cursor_row;
    unsigned short cursor_column;
    struct Os2VioCursorInfo cursor_type;
    unsigned char default_attribute;
    struct Os2VioCell *cells;
    unsigned long cell_count;
    int cells_valid;
};

void os2_vio_session_init(struct Os2VioSession *session,
                          void *backend_opaque,
                          const struct Os2VioBackendOps *backend);
void os2_vio_session_destroy(struct Os2VioSession *session);

unsigned short os2_vio_get_cur_pos(struct Os2VioSession *session,
                                   unsigned short *row, unsigned short *column,
                                   unsigned short hvio);
unsigned short os2_vio_set_cur_pos(struct Os2VioSession *session,
                                   unsigned short row, unsigned short column,
                                   unsigned short hvio);
unsigned short os2_vio_get_cur_type(struct Os2VioSession *session,
                                    struct Os2VioCursorInfo *cursor,
                                    unsigned short hvio);
unsigned short os2_vio_set_cur_type(struct Os2VioSession *session,
                                    const struct Os2VioCursorInfo *cursor,
                                    unsigned short hvio);
unsigned short os2_vio_get_mode(struct Os2VioSession *session,
                                struct Os2VioModeInfo *mode,
                                unsigned short hvio);
unsigned short os2_vio_write_char_str(struct Os2VioSession *session,
                                      const char *text, unsigned short count,
                                      unsigned short row, unsigned short column,
                                      unsigned short hvio);
unsigned short os2_vio_write_char_str_att(struct Os2VioSession *session,
                                          const char *text,
                                          unsigned short count,
                                          unsigned short row,
                                          unsigned short column,
                                          const unsigned char *attribute,
                                          unsigned short hvio);
unsigned short os2_vio_read_cell_str(struct Os2VioSession *session,
                                     unsigned char *cells,
                                     unsigned short *byte_count,
                                     unsigned short row,
                                     unsigned short column,
                                     unsigned short hvio);
unsigned short os2_vio_write_n_attr(struct Os2VioSession *session,
                                    const unsigned char *attribute,
                                    unsigned short count,
                                    unsigned short row,
                                    unsigned short column,
                                    unsigned short hvio);
unsigned short os2_vio_write_n_cell(struct Os2VioSession *session,
                                    const unsigned char *cell,
                                    unsigned short count,
                                    unsigned short row,
                                    unsigned short column,
                                    unsigned short hvio);
unsigned short os2_vio_scroll_up(struct Os2VioSession *session,
                                 unsigned short top, unsigned short left,
                                 unsigned short bottom, unsigned short right,
                                 unsigned short lines,
                                 const unsigned char *cell,
                                 unsigned short hvio);
unsigned short os2_vio_write_tty(struct Os2VioSession *session,
                                 const char *text, unsigned short count,
                                 unsigned short hvio);

#endif
