#ifndef WIN32_VIO_STUB_H
#define WIN32_VIO_STUB_H

void win32_vio_stub_reset(void);
void win32_vio_stub_set_window(unsigned short left, unsigned short top);
unsigned char win32_vio_stub_buffer_char(unsigned short row, unsigned short col);
unsigned short win32_vio_stub_buffer_attr(unsigned short row, unsigned short col);
unsigned char win32_vio_stub_char(unsigned short row, unsigned short column);
unsigned short win32_vio_stub_attr(unsigned short row, unsigned short column);
unsigned short win32_vio_stub_cursor_row(void);
unsigned short win32_vio_stub_cursor_column(void);
unsigned long win32_vio_stub_cursor_size(void);
int win32_vio_stub_cursor_visible(void);

#endif
