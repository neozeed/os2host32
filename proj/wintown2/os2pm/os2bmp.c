/* Manual Windows 3.x bitmap reader for the Micropolis tilesets.
 * The original files are 8-bit BI_RLE8 BMPs, not PM resources. */
#include "os2bmp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct _MP_RGB {
    unsigned char blue, green, red, zero;
} MP_RGB;

struct _MPBITMAP {
    int width;
    int height;
    int bpp;
    int stride;
    MP_RGB palette[256];
    unsigned char *bits;                 /* scan 0 is the bottom scan */
};

static unsigned short rd16(FILE *fp)
{
    unsigned char b[2];
    if (fread(b, 1, 2, fp) != 2) return 0;
    return (unsigned short)(b[0] | ((unsigned short)b[1] << 8));
}

static unsigned long rd32(FILE *fp)
{
    unsigned char b[4];
    if (fread(b, 1, 4, fp) != 4) return 0;
    return (unsigned long)b[0] | ((unsigned long)b[1] << 8) |
           ((unsigned long)b[2] << 16) | ((unsigned long)b[3] << 24);
}

static int decode_rle8(FILE *fp, MPBITMAP *bmp)
{
    int x, y, n, value, i;
    x = 0; y = 0;
    while (y < bmp->height) {
        n = fgetc(fp); value = fgetc(fp);
        if (n == EOF || value == EOF) return 0;
        if (n) {
            while (n-- && x < bmp->width)
                bmp->bits[y * bmp->stride + x++] = (unsigned char)value;
        } else if (value == 0) {
            x = 0; ++y;
        } else if (value == 1) {
            return 1;
        } else if (value == 2) {
            x += fgetc(fp); y += fgetc(fp);
        } else {
            for (i = 0; i < value; ++i) {
                n = fgetc(fp);
                if (n == EOF) return 0;
                if (x < bmp->width) bmp->bits[y * bmp->stride + x] = (unsigned char)n;
                ++x;
            }
            if (value & 1) (void)fgetc(fp);
        }
    }
    return 1;
}

MPBITMAP *mp_bmp_load(const char *path)
{
    FILE *fp;
    MPBITMAP *bmp;
    unsigned long off, cb, width, height, compression, colors;
    unsigned short type, planes, bpp;
    int i, row, bytes;
    fp = fopen(path, "rb");
    if (!fp) return NULL;
    type = rd16(fp); (void)rd32(fp); (void)rd16(fp); (void)rd16(fp); off = rd32(fp);
    cb = rd32(fp); width = rd32(fp); height = rd32(fp); planes = rd16(fp); bpp = rd16(fp);
    compression = rd32(fp); (void)rd32(fp); (void)rd32(fp); (void)rd32(fp);
    colors = rd32(fp); (void)rd32(fp);
    if (type != 0x4d42 || cb != 40 || planes != 1 || width == 0 || height == 0 ||
        width > 4096UL || height > 4096UL ||
        (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 24) ||
        (compression != 0 && !(compression == 1 && bpp == 8))) {
        fclose(fp); return NULL;
    }
    if (bpp == 24) colors = 0;
    else if (colors == 0) colors = 1UL << bpp;
    if (colors > 256) { fclose(fp); return NULL; }
    bmp = (MPBITMAP *)calloc(1, sizeof(*bmp));
    if (!bmp) { fclose(fp); return NULL; }
    bmp->width = (int)width; bmp->height = (int)height; bmp->bpp = (int)bpp;
    bmp->stride = (int)(((width * bpp + 31) / 32) * 4);
    bmp->bits = (unsigned char *)calloc((unsigned)bmp->stride, (unsigned)bmp->height);
    if (!bmp->bits) { free(bmp); fclose(fp); return NULL; }
    for (i = 0; i < (int)colors; ++i) {
        bmp->palette[i].blue = (unsigned char)fgetc(fp);
        bmp->palette[i].green = (unsigned char)fgetc(fp);
        bmp->palette[i].red = (unsigned char)fgetc(fp);
        bmp->palette[i].zero = (unsigned char)fgetc(fp);
    }
    if (fseek(fp, (long)off, SEEK_SET) != 0) { mp_bmp_free(bmp); fclose(fp); return NULL; }
    if (compression == 1 && bpp == 8) {
        if (!decode_rle8(fp, bmp)) { mp_bmp_free(bmp); fclose(fp); return NULL; }
    } else {
        bytes = bmp->stride;
        for (row = 0; row < bmp->height; ++row)
            if ((int)fread(bmp->bits + row * bytes, 1, (unsigned)bytes, fp) != bytes) {
                mp_bmp_free(bmp); fclose(fp); return NULL;
            }
    }
    fclose(fp);
    return bmp;
}

void mp_bmp_free(MPBITMAP *bmp)
{
    if (bmp) { free(bmp->bits); free(bmp); }
}
int mp_bmp_width(MPBITMAP *bmp) { return bmp ? bmp->width : 0; }
int mp_bmp_height(MPBITMAP *bmp) { return bmp ? bmp->height : 0; }

#ifndef MP_PORT_TEST
HBITMAP mp_bmp_create_gpi(MPBITMAP *bmp, HPS hps)
{
    BITMAPINFOHEADER2 hdr;
    BITMAPINFO2 *info;
    RGB2 *colors;
    HBITMAP hbm;
    int n, i;
    if (!bmp) return NULLHANDLE;
    n = bmp->bpp <= 8 ? 1 << bmp->bpp : 0;
    /* The beta BITMAPINFO2 has no embedded argbColor[1] member.  The RGB2
       table is variable storage immediately following its fixed header. */
    info = (BITMAPINFO2 *)calloc(1, sizeof(BITMAPINFO2) + n * sizeof(RGB2));
    if (!info) return NULLHANDLE;
    memset(&hdr, 0, sizeof(hdr));
    hdr.cbFix = sizeof(hdr); hdr.cx = (ULONG)bmp->width; hdr.cy = (ULONG)bmp->height;
    hdr.cPlanes = 1; hdr.cBitCount = (USHORT)bmp->bpp; hdr.ulCompression = BCA_UNCOMP;
    hdr.cbImage = (ULONG)(bmp->stride * bmp->height);
    hdr.cclrUsed = (ULONG)n; hdr.cclrImportant = (ULONG)n;
    memcpy(info, &hdr, sizeof(hdr));
    colors = (RGB2 *)((char *)info + sizeof(BITMAPINFO2));
    for (i = 0; i < n; ++i) {
        colors[i].bBlue = bmp->palette[i].blue;
        colors[i].bGreen = bmp->palette[i].green;
        colors[i].bRed = bmp->palette[i].red;
        colors[i].fcOptions = 0;
    }
    hbm = GpiCreateBitmap(hps, &hdr, CBM_INIT, bmp->bits, info);
    free(info);
    return hbm;
}

/* Magenta-key sprites use an AND mask followed by an OR colour bitmap.
 * Both are true-colour DIBs; no palette-index assumptions cross the PSs. */
HBITMAP mp_bmp_create_sprite(MPBITMAP *bmp, HPS ps, int mask)
{
    BITMAPINFOHEADER2 hdr;
    BITMAPINFO2 info;
    unsigned char *bits, *p, *src;
    MP_RGB color;
    HBITMAP result;
    int x, y, index, transparent;
    if (!bmp) return NULLHANDLE;
    bits = (unsigned char *)malloc((unsigned long)bmp->width * bmp->height * 4UL);
    if (!bits) return NULLHANDLE;
    for (y = 0; y < bmp->height; ++y) {
        for (x = 0; x < bmp->width; ++x) {
            src = bmp->bits + y * bmp->stride;
            if (bmp->bpp == 24) {
                color.blue = src[x * 3]; color.green = src[x * 3 + 1];
                color.red = src[x * 3 + 2];
            } else {
                if (bmp->bpp == 8) index = src[x];
                else if (bmp->bpp == 4) index = (src[x / 2] >> ((x & 1) ? 0 : 4)) & 15;
                else index = (src[x / 8] >> (7 - (x & 7))) & 1;
                color = bmp->palette[index];
            }
            transparent = color.red == 255 && color.green == 0 && color.blue == 255;
            p = bits + ((long)y * bmp->width + x) * 4;
            if (mask) p[0] = p[1] = p[2] = (unsigned char)(transparent ? 255 : 0);
            else {
                p[0] = transparent ? 0 : color.blue;
                p[1] = transparent ? 0 : color.green;
                p[2] = transparent ? 0 : color.red;
            }
            p[3] = 0;
        }
    }
    memset(&hdr, 0, sizeof(hdr));
    hdr.cbFix = sizeof(hdr); hdr.cx = bmp->width; hdr.cy = bmp->height;
    hdr.cPlanes = 1; hdr.cBitCount = 32;
    hdr.cbImage = (ULONG)bmp->width * bmp->height * 4UL;
    memcpy(&info, &hdr, sizeof(info));
    result = GpiCreateBitmap(ps, &hdr, CBM_INIT, bits, &info);
    free(bits);
    return result;
}
#endif
