#ifndef OS2BMP_H
#define OS2BMP_H

#include "mp_ui.h"

typedef struct _MPBITMAP MPBITMAP;

/* Windows 3.x BMP reader: BI_RGB and BI_RLE8, 1/4/8 bpp indexed and 24 bpp RGB images. */
MPBITMAP *mp_bmp_load(const char *path);
void mp_bmp_free(MPBITMAP *bmp);
int mp_bmp_width(MPBITMAP *bmp);
int mp_bmp_height(MPBITMAP *bmp);
#ifndef MP_PORT_TEST
HBITMAP mp_bmp_create_gpi(MPBITMAP *bmp, HPS hps);
HBITMAP mp_bmp_create_sprite(MPBITMAP *bmp, HPS hps, int mask);
#endif

#endif

