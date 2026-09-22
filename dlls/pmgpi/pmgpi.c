/*
 * pmgpi.c - tiny PMGPI personality for the Sarien C/386 PM backend.
 *
 * HPS and HBITMAP are private 32-bit pointers to small compatibility objects.
 * Bitmaps are backed by native DIB sections.  GPI keeps OS/2 coordinates
 * bottom-left based and converts them to the native HDC's top-left space at
 * the final GDI operation.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "pmcompat.h"

#ifndef __cdecl
#define __cdecl
#endif

typedef DWORD O2ULONG;
typedef LONG O2LONG;
typedef WORD O2USHORT;
typedef BYTE O2BYTE;
typedef DWORD O2HAB;
typedef DWORD O2HDC;
typedef DWORD O2HPS;
typedef DWORD O2HBITMAP;

typedef struct O2POINTL {
    O2LONG x;
    O2LONG y;
} O2POINTL;

typedef struct O2SIZEL {
    O2LONG cx;
    O2LONG cy;
} O2SIZEL;

typedef struct O2RECTL {
    O2LONG xLeft;
    O2LONG yBottom;
    O2LONG xRight;
    O2LONG yTop;
} O2RECTL;

typedef struct O2BITMAPINFOHEADER2 {
    O2ULONG cbFix;
    O2ULONG cx;
    O2ULONG cy;
    O2USHORT cPlanes;
    O2USHORT cBitCount;
    O2ULONG ulCompression;
    O2ULONG cbImage;
    O2ULONG cxResolution;
    O2ULONG cyResolution;
    O2ULONG cclrUsed;
    O2ULONG cclrImportant;
    O2USHORT usUnits;
    O2USHORT usReserved;
    O2USHORT usRecording;
    O2USHORT usRendering;
    O2ULONG cSize1;
    O2ULONG cSize2;
    O2ULONG ulColorEncoding;
    O2ULONG ulIdentifier;
} O2BITMAPINFOHEADER2;


#define BM_MAGIC 0x4d42324fUL /* O2BM */

#define O2_RT_BITMAP 2U
#define O2_BFT_BITMAP      0x4d42U
#define O2_BFT_BITMAPARRAY 0x4142U

typedef const void *(__cdecl *O2QUERYRESOURCEFN)(O2ULONG module,
                                                  O2USHORT type,
                                                  O2USHORT id,
                                                  O2ULONG *sizeOut);

static O2USHORT gpi_rd16(const O2BYTE *p)
{
    return (O2USHORT)((O2USHORT)p[0] | ((O2USHORT)p[1] << 8));
}

static O2ULONG gpi_rd32(const O2BYTE *p)
{
    return (O2ULONG)p[0] | ((O2ULONG)p[1] << 8) |
           ((O2ULONG)p[2] << 16) | ((O2ULONG)p[3] << 24);
}

static int gpi_trace_enabled(void)
{
    char value[8];
    DWORD n;
    n = GetEnvironmentVariableA("OS2_PM_TRACE", value, sizeof(value));
    return n != 0 && n < sizeof(value) && value[0] != '0';
}

typedef struct O2FONTMETRICS {
    char szFamilyname[32];
    char szFacename[32];
    O2USHORT idRegistry;
    O2USHORT usCodePage;
    O2LONG lEmHeight;
    O2LONG lXHeight;
    O2LONG lMaxAscender;
    O2LONG lMaxDescender;
    O2LONG lLowerCaseAscent;
    O2LONG lLowerCaseDescent;
    O2LONG lInternalLeading;
    O2LONG lExternalLeading;
    O2LONG lAveCharWidth;
    O2LONG lMaxCharInc;
    O2LONG lEmInc;
    O2LONG lMaxBaselineExt;
    short sCharSlope;
    short sInlineDir;
    short sCharRot;
    O2USHORT usWeightClass;
    O2USHORT usWidthClass;
    short sXDeviceRes;
    short sYDeviceRes;
    short sFirstChar;
    short sLastChar;
    short sDefaultChar;
    short sBreakChar;
    short sNominalPointSize;
    short sMinimumPointSize;
    short sMaximumPointSize;
    O2USHORT fsType;
    O2USHORT fsDefn;
    O2USHORT fsSelection;
    O2USHORT fsCapabilities;
    O2LONG lSubscriptXSize;
    O2LONG lSubscriptYSize;
    O2LONG lSubscriptXOffset;
    O2LONG lSubscriptYOffset;
    O2LONG lSuperscriptXSize;
    O2LONG lSuperscriptYSize;
    O2LONG lSuperscriptXOffset;
    O2LONG lSuperscriptYOffset;
    O2LONG lUnderscoreSize;
    O2LONG lUnderscorePosition;
    O2LONG lStrikeoutSize;
    O2LONG lStrikeoutPosition;
    short sKerningPairs;
    short sFamilyClass;
    O2LONG lMatch;
} O2FONTMETRICS;

static struct CompatPS *ps_from(O2HPS hps)
{
    struct CompatPS *p;
    p = (struct CompatPS *)(DWORD)hps;
    if (!p || p->magic != PMCOMPAT_PS_MAGIC)
        return NULL;
    return p;
}


static COLORREF gpi_native_color(O2LONG color)
{
    switch (color) {
    case -2L: return RGB(255,255,255); /* CLR_WHITE */
    case -1L: return RGB(0,0,0);       /* CLR_BLACK */
    case 0L:  return RGB(192,192,192); /* CLR_BACKGROUND */
    case 1L:  return RGB(0,0,255);
    case 2L:  return RGB(255,0,0);
    case 3L:  return RGB(255,0,255);
    case 4L:  return RGB(0,255,0);
    case 5L:  return RGB(0,255,255);
    case 6L:  return RGB(255,255,0);
    case 7L:  return RGB(192,192,192);
    case 8L:  return RGB(128,128,128);
    case 9L:  return RGB(0,0,128);
    case 10L: return RGB(128,0,0);
    case 11L: return RGB(128,0,128);
    case 12L: return RGB(0,128,0);
    case 13L: return RGB(0,128,128);
    case 14L: return RGB(128,128,0);
    case 15L: return RGB(224,224,224);
    default:  return (COLORREF)(DWORD)color;
    }
}
O2LONG __cdecl GpiDeleteBitmap(O2HBITMAP hbm);

static struct CompatBitmap *bm_from(O2HBITMAP hbm)
{
    struct CompatBitmap *b;
    b = (struct CompatBitmap *)(DWORD)hbm;
    if (!b || b->magic != BM_MAGIC)
        return NULL;
    return b;
}

static void make_bmi(struct CompatBitmap *b, BITMAPINFOHEADER *h,
                     RGBQUAD *colors)
{
    int i;
    memset(h, 0, sizeof(*h));
    h->biSize = sizeof(*h);
    h->biWidth = b->width;
    h->biHeight = b->height;
    h->biPlanes = 1;
    h->biBitCount = b->bitcount;
    h->biCompression = BI_RGB;
    h->biSizeImage = (DWORD)(b->stride * b->height);
    if (b->bitcount <= 8) {
        DWORD ncolors = 1UL << b->bitcount;
        h->biClrUsed = ncolors;
        h->biClrImportant = ncolors;
        for (i = 0; i < (int)ncolors; ++i)
            colors[i] = b->palette[i];
    }
}

static void gpi_store_bitmap_info(struct CompatBitmap *b, void *info)
{
    O2BYTE *raw;
    O2ULONG cbfix;
    DWORD ncolors;
    DWORD i;
    O2BYTE *c;
    O2BITMAPINFOHEADER2 *h2;
    if (!b || !info)
        return;
    raw = (O2BYTE *)info;
    cbfix = *(O2ULONG *)raw;
    ncolors = b->bitcount <= 8 ? (1UL << b->bitcount) : 0UL;
    if (ncolors > 256UL) ncolors = 256UL;

    /* GpiQueryBitmapBits returns an information table as well as scan data.
     * This matters for the common Query -> modify -> Set round trip: callers
     * are allowed to reuse the returned BITMAPINFO/BITMAPINFO2, including its
     * RGB table, on a later GpiSetBitmapBits call. */
    if (cbfix == 12UL) {
        *(O2ULONG *)(raw + 0) = 12UL;
        *(O2USHORT *)(raw + 4) = (O2USHORT)b->width;
        *(O2USHORT *)(raw + 6) = (O2USHORT)b->height;
        *(O2USHORT *)(raw + 8) = 1;
        *(O2USHORT *)(raw + 10) = b->bitcount;
        c = raw + 12;
        for (i = 0; i < ncolors; ++i) {
            c[i * 3UL + 0UL] = b->palette[i].rgbBlue;
            c[i * 3UL + 1UL] = b->palette[i].rgbGreen;
            c[i * 3UL + 2UL] = b->palette[i].rgbRed;
        }
    } else if (cbfix >= (O2ULONG)sizeof(O2BITMAPINFOHEADER2)) {
        h2 = (O2BITMAPINFOHEADER2 *)raw;
        memset(h2, 0, sizeof(*h2));
        h2->cbFix = cbfix;
        h2->cx = (O2ULONG)b->width;
        h2->cy = (O2ULONG)b->height;
        h2->cPlanes = 1;
        h2->cBitCount = b->bitcount;
        h2->ulCompression = 0;
        h2->cbImage = (O2ULONG)(b->stride * b->height);
        h2->cclrUsed = ncolors;
        h2->cclrImportant = ncolors;
        c = raw + cbfix;
        for (i = 0; i < ncolors; ++i) {
            c[i * 4UL + 0UL] = b->palette[i].rgbBlue;
            c[i * 4UL + 1UL] = b->palette[i].rgbGreen;
            c[i * 4UL + 2UL] = b->palette[i].rgbRed;
            c[i * 4UL + 3UL] = 0;
        }
    }
}

static void gpi_apply_bitmap_palette(struct CompatPS *p, struct CompatBitmap *b,
                                     const void *info)
{
    const O2BYTE *raw;
    O2ULONG cbfix;
    DWORD ncolors;
    DWORD i;
    if (!b || !info || b->bitcount > 8)
        return;
    raw = (const O2BYTE *)info;
    cbfix = *(const O2ULONG *)raw;
    ncolors = 1UL << b->bitcount;
    if (ncolors > 256UL) ncolors = 256UL;
    if (cbfix == 12UL) {
        const O2BYTE *c = raw + 12;
        for (i = 0; i < ncolors; ++i) {
            b->palette[i].rgbBlue = c[i * 3UL + 0UL];
            b->palette[i].rgbGreen = c[i * 3UL + 1UL];
            b->palette[i].rgbRed = c[i * 3UL + 2UL];
            b->palette[i].rgbReserved = 0;
        }
    } else if (cbfix >= 64UL) {
        const O2BYTE *c = raw + cbfix;
        for (i = 0; i < ncolors; ++i) {
            b->palette[i].rgbBlue = c[i * 4UL + 0UL];
            b->palette[i].rgbGreen = c[i * 4UL + 1UL];
            b->palette[i].rgbRed = c[i * 4UL + 2UL];
            b->palette[i].rgbReserved = 0;
        }
    } else {
        return;
    }
    if (p && p->dc && p->bitmap == b)
        SetDIBColorTable(p->dc, 0, (UINT)ncolors, b->palette);
}

/* 355 */
static int gpi_surface_height(struct CompatPS *p)
{
    RECT r;
    if (!p) return 0;
    if (p->bitmap) return (int)p->bitmap->height;
    memset(&r, 0, sizeof(r));
    if (p->hwnd && GetClientRect(p->hwnd, &r))
        return r.bottom - r.top;
    if (p->dc && GetClipBox(p->dc, &r) != ERROR)
        return r.bottom - r.top;
    return 0;
}

static DWORD gpi_rop(O2LONG rop)
{
    switch ((DWORD)rop & 0xffUL) {
    case 0xCC: return SRCCOPY;
    case 0xEE: return SRCPAINT;
    case 0x88: return SRCAND;
    case 0x66: return SRCINVERT;
    case 0x44: return SRCERASE;
    case 0x33: return NOTSRCCOPY;
    case 0x11: return NOTSRCERASE;
    case 0xC0: return MERGECOPY;
    case 0xBB: return MERGEPAINT;
    case 0xF0: return PATCOPY;
    case 0xFB: return PATPAINT;
    case 0x5A: return PATINVERT;
    case 0x55: return DSTINVERT;
    case 0x00: return BLACKNESS;
    case 0xFF: return WHITENESS;
    default:   return SRCCOPY;
    }
}

O2LONG __cdecl GpiBitBlt(O2HPS target, O2HPS source, O2LONG count,
                         O2POINTL *points, O2LONG rop, O2ULONG bbo)
{
    struct CompatPS *dst;
    struct CompatPS *src;
    int dhgt, shgt;
    int dx, dy, dw, dh;
    int sx, sy, sw, sh;
    int x0, x1, y0, y1;
    DWORD nativeRop;
    BOOL ok;
    (void)bbo;
    dst = ps_from(target);
    src = ps_from(source);
    if (gpi_trace_enabled()) {
        fprintf(stderr, "PMGPI: GpiBitBlt ENTER tid=%lu dst=%08lX[%ldx%ldx%u] src=%08lX[%ldx%ldx%u] count=%ld rop=%08lX p0=(%ld,%ld) p1=(%ld,%ld)\n",
                (unsigned long)GetCurrentThreadId(),
                (unsigned long)target,
                dst && dst->bitmap ? (long)dst->bitmap->width : -1L,
                dst && dst->bitmap ? (long)dst->bitmap->height : -1L,
                dst && dst->bitmap ? (unsigned)dst->bitmap->bitcount : 0U,
                (unsigned long)source,
                src && src->bitmap ? (long)src->bitmap->width : -1L,
                src && src->bitmap ? (long)src->bitmap->height : -1L,
                src && src->bitmap ? (unsigned)src->bitmap->bitcount : 0U,
                (long)count, (unsigned long)(DWORD)rop,
                points ? (long)points[0].x : -9999L, points ? (long)points[0].y : -9999L,
                points && count > 1 ? (long)points[1].x : -9999L,
                points && count > 1 ? (long)points[1].y : -9999L);
        fflush(stderr);
    }
    if (!dst || !dst->dc || !points || count < 2)
        return 0;
    dhgt = gpi_surface_height(dst);
    x0 = (int)points[0].x; y0 = (int)points[0].y;
    x1 = (int)points[1].x; y1 = (int)points[1].y;
    dx = x0 < x1 ? x0 : x1;
    dw = x0 < x1 ? x1 - x0 : x0 - x1;
    dh = y0 < y1 ? y1 - y0 : y0 - y1;
    dy = dhgt - (y0 > y1 ? y0 : y1);
    if (dw <= 0 || dh <= 0)
        return 0;
    nativeRop = gpi_rop(rop);

    /* ROP_ZERO / ROP_ONE are commonly used with a NULL source to build
     * monochrome masks.  PatBlt gives the same all-zero/all-one semantics. */
    if (!src || !src->dc || count < 3) {
        if (((DWORD)rop & 0xffUL) != 0x00UL &&
            ((DWORD)rop & 0xffUL) != 0xffUL)
            return 0;
        SetLastError(0);
        ok = PatBlt(dst->dc, dx, dy, dw, dh, nativeRop);
        if (gpi_trace_enabled()) {
            fprintf(stderr,
                    "PMGPI: GpiBitBlt PAT %s tid=%lu dst=(%d,%d %dx%d) rop=%08lX err=%lu\n",
                    ok ? "OK" : "FAIL", (unsigned long)GetCurrentThreadId(),
                    dx, dy, dw, dh, (unsigned long)nativeRop,
                    (unsigned long)GetLastError());
            fflush(stderr);
        }
        return ok ? 1 : 0;
    }

    sx = (int)points[2].x;
    sy = (int)points[2].y;

    /* Use one native-HDC path for bitmap-to-bitmap and bitmap-to-device
     * blits.  Both source and destination coordinates are OS/2 bottom-left
     * coordinates and are converted using their actual surface heights.
     * R4's bitmap-info/palette round trip means Sarien no longer needs a
     * separate buffer-present path. */
    shgt = gpi_surface_height(src);
    sw = dw;
    sh = dh;
    if (count >= 4) {
        sw = (int)(points[3].x - points[2].x);
        sh = (int)(points[3].y - points[2].y);
        if (sw < 0) { sx += sw; sw = -sw; }
        if (sh < 0) { sy += sh; sh = -sh; }
    }
    sy = shgt - sy - sh;
    SetStretchBltMode(dst->dc, COLORONCOLOR);
    SetLastError(0);
    ok = StretchBlt(dst->dc, dx, dy, dw, dh,
                    src->dc, sx, sy, sw, sh, nativeRop);
    if (gpi_trace_enabled()) {
        fprintf(stderr,
                "PMGPI: GpiBitBlt %s tid=%lu dst=(%d,%d %dx%d) src=(%d,%d %dx%d) rop=%08lX err=%lu\n",
                ok ? "OK" : "FAIL", (unsigned long)GetCurrentThreadId(),
                dx, dy, dw, dh, sx, sy, sw, sh,
                (unsigned long)nativeRop, (unsigned long)GetLastError());
        fflush(stderr);
    }
    return ok ? 1 : 0;
}

/* 356 */
O2LONG __cdecl GpiBox(O2HPS hps, O2LONG control, O2POINTL *corner,
                      O2LONG hround, O2LONG vround)
{
    struct CompatPS *p;
    O2LONG x1, x2, y1, y2;
    (void)hround; (void)vround;
    p = ps_from(hps);
    if (!p || !corner)
        return 0;
    x1 = p->current_x;
    y1 = p->current_y;
    x2 = corner->x;
    y2 = corner->y;

    if (p->bitmap) {
        struct CompatBitmap *b = p->bitmap;
        O2LONG left = x1 < x2 ? x1 : x2;
        O2LONG right = x1 > x2 ? x1 : x2;
        O2LONG bottom = y1 < y2 ? y1 : y2;
        O2LONG top = y1 > y2 ? y1 : y2;
        O2LONG x, y;
        if (left < 0) left = 0;
        if (bottom < 0) bottom = 0;
        if (right >= b->width) right = b->width - 1;
        if (top >= b->height) top = b->height - 1;
        if (control == 1L || control == 3L) {
            for (y = bottom; y <= top; ++y)
                for (x = left; x <= right; ++x)
                    b->bits[y * b->stride + x] = (O2BYTE)(p->color & 0xff);
        }
        p->current_x = corner->x;
        p->current_y = corner->y;
        return 1;
    }

    if (p->dc) {
        RECT client;
        RECT r;
        LONG h;
        HBRUSH br;
        HPEN pen;
        HGDIOBJ oldPen;
        HGDIOBJ oldBrush;
        memset(&client, 0, sizeof(client));
        if (p->hwnd)
            GetClientRect(p->hwnd, &client);
        else
            GetClipBox(p->dc, &client);
        h = client.bottom - client.top;
        r.left = (x1 < x2 ? x1 : x2);
        r.right = (x1 > x2 ? x1 : x2) + 1;
        r.top = h - (y1 > y2 ? y1 : y2) - 1;
        r.bottom = h - (y1 < y2 ? y1 : y2);
        br = CreateSolidBrush(gpi_native_color(p->color));
        if (!br)
            return 0;
        if (control == 1L) {
            FillRect(p->dc, &r, br);
        } else {
            pen = CreatePen(PS_SOLID, 1, gpi_native_color(p->color));
            if (!pen) { DeleteObject(br); return 0; }
            oldPen = SelectObject(p->dc, pen);
            oldBrush = SelectObject(p->dc, control == 3L ? br : GetStockObject(NULL_BRUSH));
            Rectangle(p->dc, r.left, r.top, r.right, r.bottom);
            SelectObject(p->dc, oldBrush);
            SelectObject(p->dc, oldPen);
            DeleteObject(pen);
        }
        DeleteObject(br);
        p->current_x = corner->x;
        p->current_y = corner->y;
        return 1;
    }
    return 0;
}

/* 369 */
O2HPS __cdecl GpiCreatePS(O2HAB hab, O2HDC hdc, O2SIZEL *size, O2ULONG options)
{
    struct CompatPS *p;
    (void)hab;
    p = (struct CompatPS *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*p));
    if (!p)
        return 0;
    p->magic = PMCOMPAT_PS_MAGIC;
    p->dc = (HDC)(DWORD)hdc;
    p->hwnd = p->dc ? WindowFromDC(p->dc) : NULL;
    p->color = -1; /* OS/2 GPI default foreground is CLR_BLACK */
    p->default_view[0] = 65536L;
    p->default_view[4] = 65536L;
    p->default_view[8] = 1L;
    p->line_type = 0;
    p->saved_dc = p->dc ? SaveDC(p->dc) : 0;
    if (gpi_trace_enabled()) {
        fprintf(stderr,
                "PMGPI: GpiCreatePS tid=%lu hps=%08lX hdc=%08lX hwnd=%08lX size=(%ld,%ld) opt=%08lX saved=%d\n",
                (unsigned long)GetCurrentThreadId(),
                (unsigned long)(DWORD)p, (unsigned long)hdc,
                (unsigned long)(DWORD)p->hwnd,
                size ? (long)size->cx : -1L, size ? (long)size->cy : -1L,
                (unsigned long)options, p->saved_dc);
        fflush(stderr);
    }
    return (O2HPS)(DWORD)p;
}

/* 453 */
O2LONG __cdecl GpiQueryFontMetrics(O2HPS hps, O2LONG metricsLength,
                                   O2FONTMETRICS *metrics)
{
    struct CompatPS *p;
    TEXTMETRICA tm;
    O2FONTMETRICS fm;
    O2LONG copy;
    p = ps_from(hps);
    if (!p || !p->dc || !metrics || metricsLength <= 0)
        return 0;
    memset(&tm, 0, sizeof(tm));
    if (!GetTextMetricsA(p->dc, &tm))
        return 0;
    if (gpi_trace_enabled()) {
        fprintf(stderr, "PMGPI: GpiQueryFontMetrics hps=%08lX len=%ld height=%ld ave=%ld max=%ld extlead=%ld\n",
                (unsigned long)hps, (long)metricsLength,
                (long)tm.tmHeight, (long)tm.tmAveCharWidth,
                (long)tm.tmMaxCharWidth, (long)tm.tmExternalLeading);
        fflush(stderr);
    }
    memset(&fm, 0, sizeof(fm));
    fm.lEmHeight = (O2LONG)tm.tmHeight;
    fm.lXHeight = (O2LONG)(tm.tmAscent / 2);
    fm.lMaxAscender = (O2LONG)tm.tmAscent;
    fm.lMaxDescender = (O2LONG)tm.tmDescent;
    fm.lLowerCaseAscent = (O2LONG)tm.tmAscent;
    fm.lLowerCaseDescent = (O2LONG)tm.tmDescent;
    fm.lInternalLeading = (O2LONG)tm.tmInternalLeading;
    fm.lExternalLeading = (O2LONG)tm.tmExternalLeading;
    fm.lAveCharWidth = (O2LONG)tm.tmAveCharWidth;
    fm.lMaxCharInc = (O2LONG)tm.tmMaxCharWidth;
    fm.lEmInc = (O2LONG)tm.tmAveCharWidth;
    fm.lMaxBaselineExt = (O2LONG)tm.tmHeight;
    fm.sFirstChar = (short)(unsigned char)tm.tmFirstChar;
    fm.sLastChar = (short)(unsigned char)tm.tmLastChar;
    fm.sDefaultChar = (short)(unsigned char)tm.tmDefaultChar;
    fm.sBreakChar = (short)(unsigned char)tm.tmBreakChar;
    fm.usWeightClass = (O2USHORT)tm.tmWeight;
    fm.fsSelection = (O2USHORT)((tm.tmItalic ? 0x0001U : 0U) |
                                (tm.tmUnderlined ? 0x0002U : 0U) |
                                (tm.tmStruckOut ? 0x0010U : 0U) |
                                (tm.tmWeight >= FW_BOLD ? 0x0020U : 0U));
    if (gpi_trace_enabled()) {
        fprintf(stderr,
                "PMGPI: FONTMETRICS off96(ext)=%ld off100(ave)=%ld "
                "off104(max)=%ld off112(base)=%ld size=%lu\n",
                (long)fm.lExternalLeading, (long)fm.lAveCharWidth,
                (long)fm.lMaxCharInc, (long)fm.lMaxBaselineExt,
                (unsigned long)sizeof(fm));
        fflush(stderr);
    }
    copy = metricsLength;
    if (copy > (O2LONG)sizeof(fm))
        copy = (O2LONG)sizeof(fm);
    memcpy(metrics, &fm, (size_t)copy);
    return 1;
}

/* 505 */
O2LONG __cdecl GpiSetBackMix(O2HPS hps, O2LONG mix)
{
    (void)hps; (void)mix;
    return 1;
}

/* 506 */
O2HBITMAP __cdecl GpiSetBitmap(O2HPS hps, O2HBITMAP hbm)
{
    struct CompatPS *p;
    struct CompatBitmap *old;
    struct CompatBitmap *next;
    p = ps_from(hps);
    if (!p)
        return 0;
    old = p->bitmap;
    next = bm_from(hbm);
    if (gpi_trace_enabled()) {
        fprintf(stderr,
                "PMGPI: GpiSetBitmap ENTER tid=%lu hps=%08lX old=%08lX new=%08lX newgeom=%ldx%ldx%u\n",
                (unsigned long)GetCurrentThreadId(), (unsigned long)hps,
                (unsigned long)(DWORD)old, (unsigned long)hbm,
                next ? (long)next->width : -1L,
                next ? (long)next->height : -1L,
                next ? (unsigned)next->bitcount : 0U);
        fflush(stderr);
    }
    if (hbm != 0 && !next)
        return 0;
    if (p->dc) {
        if (next && next->native_bitmap) {
            HGDIOBJ selected;
            SetLastError(0);
            selected = SelectObject(p->dc, next->native_bitmap);
            if (!selected) {
                if (gpi_trace_enabled()) {
                    fprintf(stderr,
                            "PMGPI: GpiSetBitmap FAIL tid=%lu SelectObject err=%lu\n",
                            (unsigned long)GetCurrentThreadId(),
                            (unsigned long)GetLastError());
                    fflush(stderr);
                }
                return 0;
            }
        } else if (p->saved_dc) {
            /* Restoring and immediately re-saving the PS state is the safest
             * way to detach a selected DIB section from a memory DC. */
            RestoreDC(p->dc, p->saved_dc);
            p->saved_dc = SaveDC(p->dc);
        }
    }
    p->bitmap = next;
    if (gpi_trace_enabled()) {
        fprintf(stderr,
                "PMGPI: GpiSetBitmap OK tid=%lu hps=%08lX selected=%08lX\n",
                (unsigned long)GetCurrentThreadId(), (unsigned long)hps,
                (unsigned long)hbm);
        fflush(stderr);
    }
    return (O2HBITMAP)(DWORD)old;
}

/* 517 */
O2LONG __cdecl GpiSetColor(O2HPS hps, O2LONG color)
{
    struct CompatPS *p;
    O2LONG old;
    p = ps_from(hps);
    if (!p)
        return -1;
    old = p->color;
    p->color = color;
    return old;
}


/* 519 */
O2LONG __cdecl GpiSetCurrentPosition(O2HPS hps, O2POINTL *pt)
{
    struct CompatPS *p;
    p = ps_from(hps);
    if (!p || !pt)
        return 0;
    p->current_x = pt->x;
    p->current_y = pt->y;
    return 1;
}

/* 544 */
O2LONG __cdecl GpiSetPel(O2HPS hps, O2POINTL *pt)
{
    struct CompatPS *p;
    struct CompatBitmap *b;
    O2LONG x;
    O2LONG y;
    if (!pt)
        return 0;
    p = ps_from(hps);
    if (!p || !p->bitmap)
        return 0;
    b = p->bitmap;
    x = pt->x;
    y = pt->y;
    if (x < 0 || y < 0 || x >= b->width || y >= b->height)
        return 0;
    b->bits[y * b->stride + x] = (O2BYTE)(p->color & 0xff);
    return 1;
}

/* 592 */
O2LONG __cdecl GpiCreateLogColorTable(O2HPS hps, O2ULONG options,
                                      O2LONG format, O2LONG start,
                                      O2LONG count, O2LONG *colors)
{
    struct CompatPS *p;
    struct CompatBitmap *b;
    O2LONG i;
    DWORD v;
    int idx;
    (void)options; (void)format;
    p = ps_from(hps);
    if (!p || !p->bitmap || !colors)
        return 0;
    b = p->bitmap;
    for (i = 0; i < count; ++i) {
        idx = (int)(start + i);
        if (idx < 0 || idx >= 256)
            continue;
        v = (DWORD)colors[i];
        b->palette[idx].rgbRed = (BYTE)((v >> 16) & 0xff);
        b->palette[idx].rgbGreen = (BYTE)((v >> 8) & 0xff);
        b->palette[idx].rgbBlue = (BYTE)(v & 0xff);
        b->palette[idx].rgbReserved = 0;
    }
    if (p->dc && b->bitcount <= 8 && count > 0)
        SetDIBColorTable(p->dc, (UINT)(start < 0 ? 0 : start),
                         (UINT)count, b->palette + (start < 0 ? 0 : start));
    return 1;
}

/* 598 */
O2HBITMAP __cdecl GpiCreateBitmap(O2HPS hps, O2BITMAPINFOHEADER2 *hdr,
                                  O2ULONG options, O2BYTE *initialBits,
                                  void *info)
{
    struct CompatBitmap *b;
    const O2BYTE *raw;
    O2LONG width, height, stride, bytes;
    O2USHORT planes, bitcount;
    O2ULONG cbfix;
    DWORD colors, cbinfo;
    BITMAPINFO *bi;
    void *dibBits;
    int i;
    (void)hps; (void)options; (void)info;
    if (!hdr)
        return 0;
    raw = (const O2BYTE *)hdr;
    cbfix = *(const O2ULONG *)raw;
    if (cbfix == 12UL) {
        width = (O2LONG)*(const O2USHORT *)(raw + 4);
        height = (O2LONG)*(const O2USHORT *)(raw + 6);
        planes = *(const O2USHORT *)(raw + 8);
        bitcount = *(const O2USHORT *)(raw + 10);
    } else {
        width = (O2LONG)hdr->cx;
        height = (O2LONG)hdr->cy;
        planes = hdr->cPlanes;
        bitcount = hdr->cBitCount;
    }
    if (gpi_trace_enabled()) {
        fprintf(stderr, "PMGPI: GpiCreateBitmap hps=%08lX cbFix=%lu size=%ldx%ld planes=%u bpp=%u initial=%08lX info=%08lX\n",
                (unsigned long)hps, (unsigned long)cbfix, (long)width, (long)height,
                (unsigned)planes, (unsigned)bitcount,
                (unsigned long)(DWORD)initialBits, (unsigned long)(DWORD)info);
        fflush(stderr);
    }
    if (width <= 0 || height <= 0 || planes != 1 ||
        (bitcount != 1 && bitcount != 4 && bitcount != 8 &&
         bitcount != 24 && bitcount != 32)) {
        if (gpi_trace_enabled()) {
            fprintf(stderr, "PMGPI: GpiCreateBitmap REJECT header\n");
            fflush(stderr);
        }
        return 0;
    }

    b = (struct CompatBitmap *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                         sizeof(*b));
    if (!b)
        return 0;
    b->magic = BM_MAGIC;
    b->width = width;
    b->height = height;
    b->bitcount = bitcount;
    stride = (O2LONG)((((O2ULONG)width * bitcount + 31UL) / 32UL) * 4UL);
    b->stride = stride;
    bytes = stride * height;

    colors = bitcount <= 8 ? (1UL << bitcount) : 0UL;
    cbinfo = sizeof(BITMAPINFOHEADER) + colors * sizeof(RGBQUAD);
    bi = (BITMAPINFO *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cbinfo);
    if (!bi) {
        HeapFree(GetProcessHeap(), 0, b);
        return 0;
    }
    bi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi->bmiHeader.biWidth = width;
    bi->bmiHeader.biHeight = height; /* OS/2 and Win32 DIBs are bottom-up here */
    bi->bmiHeader.biPlanes = 1;
    bi->bmiHeader.biBitCount = bitcount;
    bi->bmiHeader.biCompression = BI_RGB;
    bi->bmiHeader.biSizeImage = (DWORD)bytes;
    bi->bmiHeader.biClrUsed = colors;
    bi->bmiHeader.biClrImportant = colors;
    for (i = 0; i < (int)colors; ++i) {
        BYTE v;
        if (colors > 1)
            v = (BYTE)((255UL * (DWORD)i) / (colors - 1UL));
        else
            v = 0;
        bi->bmiColors[i].rgbRed = v;
        bi->bmiColors[i].rgbGreen = v;
        bi->bmiColors[i].rgbBlue = v;
        b->palette[i] = bi->bmiColors[i];
    }
    for (; i < 256; ++i) {
        b->palette[i].rgbRed = (BYTE)i;
        b->palette[i].rgbGreen = (BYTE)i;
        b->palette[i].rgbBlue = (BYTE)i;
    }

    dibBits = NULL;
    b->native_bitmap = CreateDIBSection(NULL, bi, DIB_RGB_COLORS,
                                        &dibBits, NULL, 0);
    HeapFree(GetProcessHeap(), 0, bi);
    if (!b->native_bitmap || !dibBits) {
        if (b->native_bitmap) DeleteObject(b->native_bitmap);
        HeapFree(GetProcessHeap(), 0, b);
        return 0;
    }
    b->bits = (O2BYTE *)dibBits;
    if (initialBits)
        memcpy(b->bits, initialBits, (size_t)bytes);
    if (gpi_trace_enabled()) {
        fprintf(stderr, "PMGPI: GpiCreateBitmap OK hbm=%08lX stride=%ld bytes=%ld native=%p\n",
                (unsigned long)(DWORD)b, (long)stride, (long)bytes, (void *)b->native_bitmap);
        fflush(stderr);
    }
    return (O2HBITMAP)(DWORD)b;
}

static void gpi_apply_resource_palette(struct CompatBitmap *b,
                                       const O2BYTE *palette,
                                       O2ULONG colors, int rgb2)
{
    O2ULONG i;
    HDC dc;
    HGDIOBJ old;
    if (!b || !palette || colors == 0 || b->bitcount > 8)
        return;
    if (colors > 256UL)
        colors = 256UL;
    for (i = 0; i < colors; ++i) {
        O2ULONG step;
        step = rgb2 ? 4UL : 3UL;
        b->palette[i].rgbBlue  = palette[i * step + 0UL];
        b->palette[i].rgbGreen = palette[i * step + 1UL];
        b->palette[i].rgbRed   = palette[i * step + 2UL];
        b->palette[i].rgbReserved = 0;
    }
    dc = CreateCompatibleDC(NULL);
    if (!dc)
        return;
    old = SelectObject(dc, b->native_bitmap);
    if (old && old != HGDI_ERROR)
        SetDIBColorTable(dc, 0, (UINT)colors, b->palette);
    if (old && old != HGDI_ERROR)
        SelectObject(dc, old);
    DeleteDC(dc);
}

static O2HBITMAP gpi_scale_loaded_bitmap(O2HPS hps, O2HBITMAP source,
                                         O2LONG width, O2LONG height)
{
    struct CompatBitmap *src;
    struct CompatBitmap *dst;
    O2BITMAPINFOHEADER2 hdr;
    O2HBITMAP result;
    HDC sdc, ddc;
    HGDIOBJ sold, dold;
    BOOL ok;
    O2ULONG colors;
    src = bm_from(source);
    if (!src)
        return 0;
    if (width <= 0) width = src->width;
    if (height <= 0) height = src->height;
    if (width == src->width && height == src->height)
        return source;
    memset(&hdr, 0, sizeof(hdr));
    hdr.cbFix = sizeof(hdr);
    hdr.cx = (O2ULONG)width;
    hdr.cy = (O2ULONG)height;
    hdr.cPlanes = 1;
    hdr.cBitCount = src->bitcount;
    result = GpiCreateBitmap(hps, &hdr, 0, NULL, NULL);
    dst = bm_from(result);
    if (!dst)
        return 0;
    colors = src->bitcount <= 8 ? (1UL << src->bitcount) : 0UL;
    if (colors) {
        memcpy(dst->palette, src->palette,
               (size_t)colors * sizeof(src->palette[0]));
        gpi_apply_resource_palette(dst, (const O2BYTE *)src->palette,
                                   colors, 1);
    }
    sdc = CreateCompatibleDC(NULL);
    ddc = CreateCompatibleDC(NULL);
    if (!sdc || !ddc) {
        if (sdc) DeleteDC(sdc);
        if (ddc) DeleteDC(ddc);
        GpiDeleteBitmap(result);
        return 0;
    }
    sold = SelectObject(sdc, src->native_bitmap);
    dold = SelectObject(ddc, dst->native_bitmap);
    SetStretchBltMode(ddc, COLORONCOLOR);
    ok = StretchBlt(ddc, 0, 0, width, height,
                    sdc, 0, 0, src->width, src->height, SRCCOPY);
    if (sold && sold != HGDI_ERROR) SelectObject(sdc, sold);
    if (dold && dold != HGDI_ERROR) SelectObject(ddc, dold);
    DeleteDC(sdc);
    DeleteDC(ddc);
    if (!ok) {
        GpiDeleteBitmap(result);
        return 0;
    }
    GpiDeleteBitmap(source);
    return result;
}

/* 399 - HBITMAP GpiLoadBitmap(HPS, HMODULE, ULONG id, LONG cx, LONG cy)
 *
 * OS/2 RT_BITMAP resources use the native OS/2 bitmap file structures rather
 * than Win32 resources.  Query the guest resource registry exported by PMWIN,
 * decode either a 1.x BITMAPINFOHEADER or 2.x BITMAPINFOHEADER2, create a
 * compatibility DIB section, and optionally scale to the caller's requested
 * dimensions.
 */
O2HBITMAP __cdecl GpiLoadBitmap(O2HPS hps, O2ULONG module, O2ULONG id,
                                O2LONG width, O2LONG height)
{
    HMODULE pmwin;
    O2QUERYRESOURCEFN query;
    const O2BYTE *data;
    const O2BYTE *hdrp;
    const O2BYTE *palette;
    O2ULONG size, base, hdr_off, cbfix, colors, palette_bytes;
    O2ULONG bits_off, stride, bytes, compression;
    O2LONG src_width, src_height;
    O2USHORT planes, bitcount, type;
    O2BITMAPINFOHEADER2 hdr2;
    O2HBITMAP result;
    struct CompatBitmap *b;
    int rgb2;

    pmwin = GetModuleHandleA("PMWIN.dll");
    if (!pmwin)
        pmwin = GetModuleHandleA("pmwin.dll");
    if (!pmwin)
        return 0;
    query = (O2QUERYRESOURCEFN)GetProcAddress(pmwin, "OS2PM_QueryResource");
    if (!query)
        return 0;
    size = 0;
    data = (const O2BYTE *)query(module, O2_RT_BITMAP, (O2USHORT)id, &size);
    if (!data || size < 12UL)
        return 0;

    base = 0;
    type = gpi_rd16(data);
    if (type == O2_BFT_BITMAPARRAY) {
        if (size < 28UL)
            return 0;
        base = 14UL;
        type = gpi_rd16(data + base);
    }

    if (type == O2_BFT_BITMAP) {
        if (base + 14UL > size)
            return 0;
        bits_off = gpi_rd32(data + base + 10UL);
        hdr_off = base + 14UL;
    } else {
        /* Some resource compilers store the bitmap-info table directly. */
        bits_off = 0;
        hdr_off = base;
    }
    if (hdr_off + 12UL > size)
        return 0;
    hdrp = data + hdr_off;
    cbfix = gpi_rd32(hdrp);
    compression = 0;
    if (cbfix == 12UL) {
        src_width = (O2LONG)gpi_rd16(hdrp + 4UL);
        src_height = (O2LONG)gpi_rd16(hdrp + 6UL);
        planes = gpi_rd16(hdrp + 8UL);
        bitcount = gpi_rd16(hdrp + 10UL);
        colors = bitcount <= 8 ? (1UL << bitcount) : 0UL;
        palette_bytes = colors * 3UL;
        rgb2 = 0;
    } else if (cbfix >= 64UL && hdr_off + cbfix <= size) {
        src_width = (O2LONG)gpi_rd32(hdrp + 4UL);
        src_height = (O2LONG)gpi_rd32(hdrp + 8UL);
        planes = gpi_rd16(hdrp + 12UL);
        bitcount = gpi_rd16(hdrp + 14UL);
        compression = gpi_rd32(hdrp + 16UL);
        colors = gpi_rd32(hdrp + 32UL);
        if (colors == 0 && bitcount <= 8)
            colors = 1UL << bitcount;
        palette_bytes = colors * 4UL;
        rgb2 = 1;
    } else {
        return 0;
    }
    if (src_width <= 0 || src_height <= 0 || planes != 1 || compression != 0 ||
        (bitcount != 1 && bitcount != 4 && bitcount != 8 &&
         bitcount != 24 && bitcount != 32) || colors > 256UL)
        return 0;
    if (hdr_off + cbfix + palette_bytes > size)
        return 0;
    palette = hdrp + cbfix;
    if (bits_off == 0)
        bits_off = hdr_off + cbfix + palette_bytes;
    stride = (((O2ULONG)src_width * bitcount + 31UL) / 32UL) * 4UL;
    bytes = stride * (O2ULONG)src_height;
    if (bits_off > size || bytes > size - bits_off)
        return 0;

    memset(&hdr2, 0, sizeof(hdr2));
    hdr2.cbFix = sizeof(hdr2);
    hdr2.cx = (O2ULONG)src_width;
    hdr2.cy = (O2ULONG)src_height;
    hdr2.cPlanes = 1;
    hdr2.cBitCount = bitcount;
    result = GpiCreateBitmap(hps, &hdr2, 0, (O2BYTE *)(data + bits_off), NULL);
    b = bm_from(result);
    if (!b)
        return 0;
    if (colors)
        gpi_apply_resource_palette(b, palette, colors, rgb2);
    if (width > 0 || height > 0) {
        O2HBITMAP scaled;
        scaled = gpi_scale_loaded_bitmap(hps, result, width, height);
        if (!scaled) {
            GpiDeleteBitmap(result);
            return 0;
        }
        result = scaled;
    }
    if (gpi_trace_enabled()) {
        fprintf(stderr,
                "PMGPI: GpiLoadBitmap hps=%08lX mod=%08lX id=%lu -> %08lX (%ldx%ld)\n",
                (unsigned long)hps, (unsigned long)module, (unsigned long)id,
                (unsigned long)result, (long)src_width, (long)src_height);
        fflush(stderr);
    }
    return result;
}

/* 599 */
O2LONG __cdecl GpiQueryBitmapBits(O2HPS hps, O2LONG start,
                                  O2LONG count, O2BYTE *buffer, void *info)
{
    struct CompatPS *p;
    struct CompatBitmap *b;
    O2LONG n;
    p = ps_from(hps);
    if (!p || !p->bitmap || !buffer)
        return 0;
    b = p->bitmap;
    if (start < 0 || start >= b->height)
        return 0;
    n = count;
    if (n > b->height - start)
        n = b->height - start;
    if (n < 0)
        return 0;
    gpi_store_bitmap_info(b, info);
    memcpy(buffer, b->bits + start * b->stride, (size_t)(n * b->stride));
    if (gpi_trace_enabled()) {
        fprintf(stderr,
                "PMGPI: GpiQueryBitmapBits OK scans=%ld stride=%ld bytes=%ld info=%08lX\n",
                (long)n, (long)b->stride, (long)(n * b->stride),
                (unsigned long)(DWORD)info);
        fflush(stderr);
    }
    return n;
}

/* 601 */
O2LONG __cdecl GpiQueryBitmapInfoHeader(O2HBITMAP hbm, O2BITMAPINFOHEADER2 *hdr)
{
    struct CompatBitmap *b;
    b = bm_from(hbm);
    if (!b || !hdr)
        return 0;
    memset(hdr, 0, sizeof(*hdr));
    hdr->cbFix = sizeof(*hdr);
    hdr->cx = (O2ULONG)b->width;
    hdr->cy = (O2ULONG)b->height;
    hdr->cPlanes = 1;
    hdr->cBitCount = b->bitcount;
    hdr->ulCompression = 0;
    hdr->cbImage = (O2ULONG)(b->stride * b->height);
    hdr->cclrUsed = b->bitcount <= 8 ? (1UL << b->bitcount) : 0UL;
    hdr->cclrImportant = hdr->cclrUsed;
    return 1;
}

/* 602 */
O2LONG __cdecl GpiSetBitmapBits(O2HPS hps, O2LONG start,
                                O2LONG count, O2BYTE *buffer, void *info)
{
    struct CompatPS *p;
    struct CompatBitmap *b;
    O2LONG n;
    p = ps_from(hps);
    if (gpi_trace_enabled()) {
        O2ULONG icb = info ? *(const O2ULONG *)info : 0UL;
        fprintf(stderr, "PMGPI: GpiSetBitmapBits hps=%08lX start=%ld count=%ld buffer=%08lX info=%08lX cbFix=%lu\n",
                (unsigned long)hps, (long)start, (long)count,
                (unsigned long)(DWORD)buffer, (unsigned long)(DWORD)info,
                (unsigned long)icb);
        fflush(stderr);
    }
    if (!p || !p->bitmap || !buffer)
        return 0;
    b = p->bitmap;
    gpi_apply_bitmap_palette(p, b, info);
    if (gpi_trace_enabled() && b->bitcount <= 8) {
        DWORD last;
        last = (1UL << b->bitcount) - 1UL;
        fprintf(stderr,
                "PMGPI: GpiSetBitmapBits palette tid=%lu bpp=%u p0=%02X%02X%02X p1=%02X%02X%02X plast=%02X%02X%02X\n",
                (unsigned long)GetCurrentThreadId(), (unsigned)b->bitcount,
                (unsigned)b->palette[0].rgbRed, (unsigned)b->palette[0].rgbGreen, (unsigned)b->palette[0].rgbBlue,
                (unsigned)b->palette[1].rgbRed, (unsigned)b->palette[1].rgbGreen, (unsigned)b->palette[1].rgbBlue,
                (unsigned)b->palette[last].rgbRed, (unsigned)b->palette[last].rgbGreen, (unsigned)b->palette[last].rgbBlue);
        fflush(stderr);
    }
    if (start < 0 || start >= b->height)
        return 0;
    n = count;
    if (n > b->height - start)
        n = b->height - start;
    if (n < 0)
        return 0;
    memcpy(b->bits + start * b->stride, buffer, (size_t)(n * b->stride));
    if (gpi_trace_enabled()) {
        fprintf(stderr, "PMGPI: GpiSetBitmapBits OK scans=%ld stride=%ld bytes=%ld\n",
                (long)n, (long)b->stride, (long)(n * b->stride));
        fflush(stderr);
    }
    return n;
}

/* 588 */
O2ULONG __cdecl GpiSetAttrs(O2HPS hps, O2LONG primType,
                            O2ULONG attrMask, O2ULONG defMask,
                            const void *attrs)
{
    struct CompatPS *p;
    const O2LONG *color;
    (void)defMask;
    p=ps_from(hps);
    if (!p || !attrs) return 0;
    /* OPENDLG/HELLO uses PRIM_CHAR + CBB_COLOR.  CHARBUNDLE begins with
       lColor, so implement that historical contract without depending on a
       host copy of the full 1990 structure layout. */
    if (primType == 2L && (attrMask & 0x0001UL) != 0UL) {
        color=(const O2LONG *)attrs;
        p->color=*color;
    }
    return 1;
}

/* 610 */
O2HDC __cdecl DevOpenDC(O2HAB hab, O2LONG type, const char *token,
                        O2LONG count, void *openData, O2HDC compatible)
{
    HDC dc;
    (void)hab; (void)type; (void)token; (void)count;
    (void)openData; (void)compatible;
    dc = CreateCompatibleDC(NULL);
    return (O2HDC)(DWORD)dc;
}


/* 359 */
O2LONG __cdecl GpiCharStringAt(O2HPS hps, O2POINTL *pt, O2LONG count,
                               const char *text)
{
    struct CompatPS *p;
    TEXTMETRICA tm;
    RECT cr;
    int y;
    p = ps_from(hps);
    if (gpi_trace_enabled()) {
        fprintf(stderr, "PMGPI: GpiCharStringAt hps=%08lX pt=(%ld,%ld) count=%ld text=%08lX\n",
                (unsigned long)hps,
                pt ? (long)pt->x : -9999L, pt ? (long)pt->y : -9999L,
                (long)count, (unsigned long)(DWORD)text);
        fflush(stderr);
    }
    if (!p || !p->dc || !pt || !text || count < 0) {
        if (gpi_trace_enabled()) {
            fprintf(stderr, "PMGPI: GpiCharStringAt rejected ps=%p dc=%p\n",
                    (void *)p, p ? (void *)p->dc : NULL);
            fflush(stderr);
        }
        return 0;
    }
    memset(&tm, 0, sizeof(tm));
    GetTextMetricsA(p->dc, &tm);
    memset(&cr, 0, sizeof(cr));
    if (p->hwnd) GetClientRect(p->hwnd, &cr);
    else cr.bottom = p->bitmap ? p->bitmap->height : 0;
    y = cr.bottom - (int)pt->y - (int)tm.tmHeight;
    SetTextColor(p->dc, gpi_native_color(p->color));
    SetBkMode(p->dc, TRANSPARENT);
    if (!TextOutA(p->dc, (int)pt->x, y, text, (int)count)) {
        if (gpi_trace_enabled()) {
            fprintf(stderr, "PMGPI: GpiCharStringAt TextOutA FAIL err=%lu native=(%ld,%d) color=%ld\n",
                    (unsigned long)GetLastError(), (long)pt->x, y, (long)p->color);
            fflush(stderr);
        }
        return 0;
    }
    if (gpi_trace_enabled()) {
        fprintf(stderr, "PMGPI: GpiCharStringAt OK native=(%ld,%d) color=%ld\n",
                (long)pt->x, y, (long)p->color);
        fflush(stderr);
    }
    p->current_x = pt->x;
    p->current_y = pt->y;
    return count;
}

/* 370 */
O2ULONG __cdecl GpiCreateRegion(O2HPS hps, O2LONG count, O2RECTL *rects)
{
    struct CompatPS *p;
    HRGN out, one;
    O2LONG i;
    RECT r;
    LONG h;
    p = ps_from(hps);
    if (!p || count < 0 || (count > 0 && !rects))
        return 0;
    h = gpi_surface_height(p);
    out = CreateRectRgn(0,0,0,0);
    if (!out) return 0;
    for (i=0; i<count; ++i) {
        r.left=rects[i].xLeft; r.right=rects[i].xRight;
        r.top=h-rects[i].yTop; r.bottom=h-rects[i].yBottom;
        one=CreateRectRgn(r.left,r.top,r.right,r.bottom);
        if (one) { CombineRgn(out,out,one,RGN_OR); DeleteObject(one); }
    }
    return (O2ULONG)(DWORD)out;
}

/* 379 */
O2LONG __cdecl GpiDestroyPS(O2HPS hps)
{
    struct CompatPS *p;
    p = ps_from(hps);
    if (!p) return 0;
    if (p->dc && p->saved_dc)
        RestoreDC(p->dc, p->saved_dc);
    p->magic = 0;
    HeapFree(GetProcessHeap(),0,p);
    return 1;
}

static int gpi_pen_style(O2LONG lt)
{
    switch (lt) {
    case 1: return PS_DOT;
    case 2: return PS_DASH;
    case 3: return PS_DASHDOT;
    case 4: return PS_DASHDOTDOT;
    case 5: return PS_DASH;
    case 6: return PS_DASHDOTDOT;
    case 8: return PS_NULL;
    default: return PS_SOLID;
    }
}

/* 398 */
O2LONG __cdecl GpiLine(O2HPS hps, O2POINTL *end)
{
    struct CompatPS *p;
    RECT cr;
    int h;
    HPEN pen;
    HGDIOBJ old;
    p=ps_from(hps);
    if (!p || !p->dc || !end) return 0;
    memset(&cr,0,sizeof(cr));
    if (p->hwnd) GetClientRect(p->hwnd,&cr);
    h=cr.bottom-cr.top;
    pen=CreatePen(gpi_pen_style(p->line_type),1,gpi_native_color(p->color));
    if (!pen) return 0;
    old=SelectObject(p->dc,pen);
    MoveToEx(p->dc,(int)p->current_x,h-(int)p->current_y,NULL);
    LineTo(p->dc,(int)end->x,h-(int)end->y);
    SelectObject(p->dc,old); DeleteObject(pen);
    p->current_x=end->x; p->current_y=end->y;
    return 1;
}

/* 404 */
O2LONG __cdecl GpiMove(O2HPS hps, O2POINTL *pt)
{
    return GpiSetCurrentPosition(hps, pt);
}

/* 489 */
O2LONG __cdecl GpiQueryTextBox(O2HPS hps, O2LONG count, const char *text,
                               O2LONG boxCount, O2POINTL *pts)
{
    struct CompatPS *p;
    SIZE z;
    TEXTMETRICA tm;
    if (!text || !pts || boxCount < 5 || count < 0) return 0;
    p=ps_from(hps); if (!p || !p->dc) return 0;
    memset(&z,0,sizeof(z)); memset(&tm,0,sizeof(tm));
    if (!GetTextExtentPoint32A(p->dc,text,(int)count,&z)) return 0;
    GetTextMetricsA(p->dc,&tm);
    pts[0].x=0; pts[0].y=tm.tmHeight;
    pts[1].x=0; pts[1].y=0;
    pts[2].x=z.cx; pts[2].y=tm.tmHeight;
    pts[3].x=z.cx; pts[3].y=0;
    pts[4].x=z.cx; pts[4].y=0;
    return 1;
}

/* 492 */
O2LONG __cdecl GpiQueryWidthTable(O2HPS hps, O2LONG first, O2LONG count,
                                  O2LONG *widths)
{
    struct CompatPS *p;
    int i;
    int w;
    if (!widths || count < 0) return 0;
    p=ps_from(hps); if (!p || !p->dc) return 0;
    for (i=0; i<(int)count; ++i) {
        w=0;
        if (!GetCharWidth32A(p->dc,(UINT)(first+i),(UINT)(first+i),&w)) {
            SIZE z; char ch=(char)(first+i); memset(&z,0,sizeof(z));
            GetTextExtentPoint32A(p->dc,&ch,1,&z); w=z.cx;
        }
        widths[i]=(O2LONG)w;
    }
    return 1;
}

/* 516 */
O2LONG __cdecl GpiSetClipRegion(O2HPS hps, O2ULONG hrgn, O2ULONG *oldrgn)
{
    struct CompatPS *p;
    int rc;
    p=ps_from(hps); if (!p || !p->dc) return 0;
    if (oldrgn) *oldrgn=0;
    rc=SelectClipRgn(p->dc,(HRGN)(DWORD)hrgn);
    return rc==ERROR ? 0 : (O2LONG)rc;
}

/* 530 */
O2LONG __cdecl GpiSetLineType(O2HPS hps, O2LONG type)
{
    struct CompatPS *p;
    O2LONG old;
    p=ps_from(hps); if (!p) return -1;
    old=p->line_type; p->line_type=type; return old;
}


/* M31F JIGSAW: transforms, paths, regions, and bitmap lifetime. */
static double gpi_fixed_to_double(LONG v)
{
    return ((double)v) / 65536.0;
}

static LONG gpi_double_to_fixed(double v)
{
    if (v >= 0.0)
        return (LONG)(v * 65536.0 + 0.5);
    return (LONG)(v * 65536.0 - 0.5);
}

static void gpi_matrix_to_double(const LONG *m, double *d)
{
    d[0] = gpi_fixed_to_double(m[0]);
    d[1] = gpi_fixed_to_double(m[1]);
    d[2] = (double)m[2];
    d[3] = gpi_fixed_to_double(m[3]);
    d[4] = gpi_fixed_to_double(m[4]);
    d[5] = (double)m[5];
    d[6] = (double)m[6];
    d[7] = (double)m[7];
    d[8] = (double)m[8];
}

static void gpi_matrix_from_double(LONG *m, const double *d)
{
    m[0] = gpi_double_to_fixed(d[0]);
    m[1] = gpi_double_to_fixed(d[1]);
    m[2] = (LONG)d[2];
    m[3] = gpi_double_to_fixed(d[3]);
    m[4] = gpi_double_to_fixed(d[4]);
    m[5] = (LONG)d[5];
    m[6] = (LONG)(d[6] >= 0.0 ? d[6] + 0.5 : d[6] - 0.5);
    m[7] = (LONG)(d[7] >= 0.0 ? d[7] + 0.5 : d[7] - 0.5);
    m[8] = (LONG)d[8];
}

static void gpi_matrix_multiply(LONG *out, const LONG *a, const LONG *b)
{
    double A[9], B[9], C[9];
    gpi_matrix_to_double(a, A);
    gpi_matrix_to_double(b, B);
    memset(C, 0, sizeof(C));
    C[0] = A[0]*B[0] + A[1]*B[3];
    C[1] = A[0]*B[1] + A[1]*B[4];
    C[3] = A[3]*B[0] + A[4]*B[3];
    C[4] = A[3]*B[1] + A[4]*B[4];
    C[6] = A[6]*B[0] + A[7]*B[3] + B[6];
    C[7] = A[6]*B[1] + A[7]*B[4] + B[7];
    C[8] = 1.0;
    gpi_matrix_from_double(out, C);
}

static void gpi_model_to_device(struct CompatPS *p, O2LONG x, O2LONG y,
                                O2LONG *ox, O2LONG *oy)
{
    double m11, m12, m21, m22, tx, ty;
    double dx, dy;
    m11 = gpi_fixed_to_double(p->default_view[0]);
    m12 = gpi_fixed_to_double(p->default_view[1]);
    m21 = gpi_fixed_to_double(p->default_view[3]);
    m22 = gpi_fixed_to_double(p->default_view[4]);
    tx = (double)p->default_view[6];
    ty = (double)p->default_view[7];
    dx = (double)x * m11 + (double)y * m21 + tx;
    dy = (double)x * m12 + (double)y * m22 + ty;
    *ox = (O2LONG)(dx >= 0.0 ? dx + 0.5 : dx - 0.5);
    *oy = (O2LONG)(dy >= 0.0 ? dy + 0.5 : dy - 0.5);
}

static int gpi_device_to_model(struct CompatPS *p, O2LONG x, O2LONG y,
                               O2LONG *ox, O2LONG *oy)
{
    double a, b, c, d, tx, ty, det, px, py, dx, dy;
    a = gpi_fixed_to_double(p->default_view[0]);
    b = gpi_fixed_to_double(p->default_view[1]);
    c = gpi_fixed_to_double(p->default_view[3]);
    d = gpi_fixed_to_double(p->default_view[4]);
    tx = (double)p->default_view[6];
    ty = (double)p->default_view[7];
    det = a*d - b*c;
    if (det > -0.0000001 && det < 0.0000001)
        return 0;
    px = (double)x - tx;
    py = (double)y - ty;
    dx = (px*d - py*c) / det;
    dy = (-px*b + py*a) / det;
    *ox = (O2LONG)(dx >= 0.0 ? dx + 0.5 : dx - 0.5);
    *oy = (O2LONG)(dy >= 0.0 ? dy + 0.5 : dy - 0.5);
    return 1;
}

static void gpi_logical_to_native(struct CompatPS *p, O2LONG x, O2LONG y,
                                  POINT *pt)
{
    O2LONG dx, dy;
    int h;
    gpi_model_to_device(p, x, y, &dx, &dy);
    h = gpi_surface_height(p);
    pt->x = (LONG)dx;
    pt->y = (LONG)(h - dy);
}

/* 351 */
O2LONG __cdecl GpiAssociate(O2HPS hps, O2HDC hdc)
{
    struct CompatPS *p;
    p = ps_from(hps);
    if (!p) return 0;
    if (p->dc && p->saved_dc)
        RestoreDC(p->dc, p->saved_dc);
    p->dc = (HDC)(DWORD)hdc;
    p->hwnd = p->dc ? WindowFromDC(p->dc) : NULL;
    p->saved_dc = p->dc ? SaveDC(p->dc) : 0;
    if (p->dc && p->bitmap && p->bitmap->native_bitmap)
        SelectObject(p->dc, p->bitmap->native_bitmap);
    return 1;
}

/* 354 */
O2LONG __cdecl GpiBeginPath(O2HPS hps, O2LONG path)
{
    struct CompatPS *p;
    p = ps_from(hps);
    if (gpi_trace_enabled()) { fprintf(stderr, "PMGPI: GpiBeginPath ENTER tid=%lu hps=%08lX path=%ld\n", (unsigned long)GetCurrentThreadId(), (unsigned long)hps, (long)path); fflush(stderr); }
    if (!p || !p->dc || path <= 0) return 0;
    SetLastError(0);
    if (!BeginPath(p->dc)) {
        if (gpi_trace_enabled()) { fprintf(stderr, "PMGPI: GpiBeginPath FAIL tid=%lu err=%lu\n", (unsigned long)GetCurrentThreadId(), (unsigned long)GetLastError()); fflush(stderr); }
        return 0;
    }
    p->active_path = path;
    if (gpi_trace_enabled()) { fprintf(stderr, "PMGPI: GpiBeginPath OK tid=%lu hps=%08lX path=%ld\n", (unsigned long)GetCurrentThreadId(), (unsigned long)hps, (long)path); fflush(stderr); }
    return 1;
}

/* 362 */
O2LONG __cdecl GpiCombineRegion(O2HPS hps, O2ULONG dest, O2ULONG src1,
                                O2ULONG src2, O2LONG mode)
{
    int nativeMode;
    int rc;
    (void)hps;
    switch (mode) {
    case 1: nativeMode = RGN_OR; break;
    case 2: nativeMode = RGN_COPY; break;
    case 4: nativeMode = RGN_XOR; break;
    case 6: nativeMode = RGN_AND; break;
    case 7: nativeMode = RGN_DIFF; break;
    default: return 0;
    }
    rc = CombineRgn((HRGN)(DWORD)dest, (HRGN)(DWORD)src1,
                    (HRGN)(DWORD)src2, nativeMode);
    return rc == ERROR ? 0 : (O2LONG)rc;
}

/* 364 */
O2LONG __cdecl GpiConvert(O2HPS hps, O2LONG src, O2LONG targ,
                          O2LONG count, O2POINTL *pts)
{
    struct CompatPS *p;
    O2LONG i, x, y;
    p = ps_from(hps);
    if (gpi_trace_enabled()) {
        fprintf(stderr, "PMGPI: GpiConvert hps=%08lX src=%ld targ=%ld count=%ld first=(%ld,%ld)\n",
                (unsigned long)hps, (long)src, (long)targ, (long)count,
                pts && count > 0 ? (long)pts[0].x : -9999L,
                pts && count > 0 ? (long)pts[0].y : -9999L);
        fflush(stderr);
    }
    if (!p || !pts || count < 0) return 0;
    if (src == targ) return 1;
    for (i = 0; i < count; ++i) {
        x = pts[i].x; y = pts[i].y;
        if (targ == 5L && src != 5L) {
            gpi_model_to_device(p, x, y, &pts[i].x, &pts[i].y);
        } else if (src == 5L && targ != 5L) {
            if (!gpi_device_to_model(p, x, y, &pts[i].x, &pts[i].y))
                return 0;
        }
        /* JIGSAW treats WORLD/MODEL/DEFAULTPAGE/PAGE identically aside
         * from the default-view transform, which is sufficient here. */
    }
    if (gpi_trace_enabled()) {
        fprintf(stderr,
                "PMGPI: GpiConvert OK tid=%lu first=(%ld,%ld) last=(%ld,%ld)\n",
                (unsigned long)GetCurrentThreadId(),
                count > 0 ? (long)pts[0].x : -9999L,
                count > 0 ? (long)pts[0].y : -9999L,
                count > 0 ? (long)pts[count - 1].x : -9999L,
                count > 0 ? (long)pts[count - 1].y : -9999L);
        fflush(stderr);
    }
    return 1;
}

/* 371 */
O2LONG __cdecl GpiDeleteBitmap(O2HBITMAP hbm)
{
    struct CompatBitmap *b;
    b = bm_from(hbm);
    if (!b) return 0;
    if (b->native_bitmap && !DeleteObject(b->native_bitmap))
        return 0;
    b->magic = 0;
    b->native_bitmap = NULL;
    b->bits = NULL;
    HeapFree(GetProcessHeap(), 0, b);
    return 1;
}

/* 387 */
O2LONG __cdecl GpiEndPath(O2HPS hps)
{
    struct CompatPS *p;
    p = ps_from(hps);
    if (gpi_trace_enabled()) { fprintf(stderr, "PMGPI: GpiEndPath ENTER tid=%lu hps=%08lX active=%ld\n", (unsigned long)GetCurrentThreadId(), (unsigned long)hps, p ? (long)p->active_path : -1L); fflush(stderr); }
    if (!p || !p->dc || !p->active_path) return 0;
    SetLastError(0);
    if (!EndPath(p->dc)) {
        if (gpi_trace_enabled()) { fprintf(stderr, "PMGPI: GpiEndPath FAIL tid=%lu err=%lu\n", (unsigned long)GetCurrentThreadId(), (unsigned long)GetLastError()); fflush(stderr); }
        return 0;
    }
    p->active_path = 0;
    if (gpi_trace_enabled()) { fprintf(stderr, "PMGPI: GpiEndPath OK tid=%lu hps=%08lX\n", (unsigned long)GetCurrentThreadId(), (unsigned long)hps); fflush(stderr); }
    return 1;
}

/* 409 */
O2LONG __cdecl GpiPaintRegion(O2HPS hps, O2ULONG hrgn)
{
    struct CompatPS *p;
    HBRUSH br;
    int rc;
    p = ps_from(hps);
    if (!p || !p->dc || !hrgn) return 0;
    br = CreateSolidBrush(gpi_native_color(p->color));
    if (!br) return 0;
    rc = FillRgn(p->dc, (HRGN)(DWORD)hrgn, br);
    DeleteObject(br);
    return rc ? 1 : 0;
}

/* 417 */
O2LONG __cdecl GpiPolySpline(O2HPS hps, O2LONG count, O2POINTL *pts)
{
    struct CompatPS *p;
    POINT *nativePts;
    POINT start;
    HPEN pen;
    HGDIOBJ oldPen;
    O2LONG i;
    BOOL rc;
    if (!pts || count <= 0) return 0;
    p = ps_from(hps);
    if (gpi_trace_enabled()) { fprintf(stderr, "PMGPI: GpiPolySpline ENTER tid=%lu hps=%08lX count=%ld current=(%ld,%ld)\n", (unsigned long)GetCurrentThreadId(), (unsigned long)hps, (long)count, p ? (long)p->current_x : -9999L, p ? (long)p->current_y : -9999L); fflush(stderr); }
    if (!p || !p->dc) return 0;
    nativePts = (POINT *)HeapAlloc(GetProcessHeap(), 0,
                                   sizeof(POINT) * (SIZE_T)count);
    if (!nativePts) return 0;
    for (i = 0; i < count; ++i)
        gpi_logical_to_native(p, pts[i].x, pts[i].y, &nativePts[i]);
    gpi_logical_to_native(p, p->current_x, p->current_y, &start);
    MoveToEx(p->dc, start.x, start.y, NULL);
    pen = CreatePen(gpi_pen_style(p->line_type), 1,
                    gpi_native_color(p->color));
    oldPen = pen ? SelectObject(p->dc, pen) : NULL;
    SetLastError(0);
    if ((count % 3L) == 0L)
        rc = PolyBezierTo(p->dc, nativePts, (DWORD)count);
    else
        rc = PolylineTo(p->dc, nativePts, (DWORD)count);
    if (gpi_trace_enabled()) {
        fprintf(stderr,
                "PMGPI: GpiPolySpline %s tid=%lu first=(%ld,%ld) last=(%ld,%ld) native-first=(%ld,%ld) native-last=(%ld,%ld) err=%lu\n",
                rc ? "OK" : "FAIL", (unsigned long)GetCurrentThreadId(),
                (long)pts[0].x, (long)pts[0].y,
                (long)pts[count - 1].x, (long)pts[count - 1].y,
                (long)nativePts[0].x, (long)nativePts[0].y,
                (long)nativePts[count - 1].x, (long)nativePts[count - 1].y,
                (unsigned long)GetLastError());
        fflush(stderr);
    }
    if (oldPen) SelectObject(p->dc, oldPen);
    if (pen) DeleteObject(pen);
    p->current_x = pts[count - 1].x;
    p->current_y = pts[count - 1].y;
    HeapFree(GetProcessHeap(), 0, nativePts);
    return rc ? count : 0;
}

/* 443 */
O2LONG __cdecl GpiQueryDefaultViewMatrix(O2HPS hps, O2LONG count, LONG *m)
{
    struct CompatPS *p;
    O2LONG n;
    p = ps_from(hps);
    if (!p || !m || count <= 0) return 0;
    n = count < 9 ? count : 9;
    memcpy(m, p->default_view, (size_t)n * sizeof(LONG));
    return 1;
}

/* 476 */
O2LONG __cdecl GpiQueryPel(O2HPS hps, O2POINTL *pt)
{
    struct CompatPS *p;
    O2LONG dx, dy;
    int h;
    COLORREF c;
    p = ps_from(hps);
    if (!p || !p->dc || !pt) return -1;
    gpi_model_to_device(p, pt->x, pt->y, &dx, &dy);
    h = gpi_surface_height(p);
    c = GetPixel(p->dc, (int)dx, h - (int)dy - 1);
    return c == CLR_INVALID ? -1 : (O2LONG)c;
}

/* 481 */
O2LONG __cdecl GpiQueryRegionBox(O2HPS hps, O2ULONG hrgn, O2RECTL *out)
{
    struct CompatPS *p;
    RECT r;
    int rc, h;
    p = ps_from(hps);
    if (!p || !hrgn || !out) return 0;
    rc = GetRgnBox((HRGN)(DWORD)hrgn, &r);
    if (rc == ERROR) return 0;
    h = gpi_surface_height(p);
    out->xLeft = r.left;
    out->xRight = r.right;
    out->yBottom = h - r.bottom;
    out->yTop = h - r.top;
    return (O2LONG)rc;
}

/* 503 */
O2LONG __cdecl GpiSetAttrMode(O2HPS hps, O2LONG mode)
{
    struct CompatPS *p;
    O2LONG old;
    p = ps_from(hps);
    if (!p) return -1;
    old = p->attr_mode;
    p->attr_mode = mode;
    return old;
}

/* 515 */
O2LONG __cdecl GpiSetClipPath(O2HPS hps, O2LONG path, O2LONG options)
{
    struct CompatPS *p;
    int rc;
    p = ps_from(hps);
    if (gpi_trace_enabled()) { fprintf(stderr, "PMGPI: GpiSetClipPath ENTER tid=%lu hps=%08lX path=%ld options=%ld\n", (unsigned long)GetCurrentThreadId(), (unsigned long)hps, (long)path, (long)options); fflush(stderr); }
    if (!p || !p->dc) return 0;
    SetLastError(0);
    if (path == 0L && options == 0L) {
        rc = SelectClipRgn(p->dc, NULL);
        if (gpi_trace_enabled()) { fprintf(stderr, "PMGPI: GpiSetClipPath RESET %s tid=%lu rc=%d err=%lu\n", rc == ERROR ? "FAIL" : "OK", (unsigned long)GetCurrentThreadId(), rc, (unsigned long)GetLastError()); fflush(stderr); }
        return rc == ERROR ? 0 : 1;
    }
    if (path <= 0L) return 0;
    SetPolyFillMode(p->dc, (options & 2L) ? WINDING : ALTERNATE);
    rc = SelectClipPath(p->dc, (options & 4L) ? RGN_AND : RGN_COPY);
    if (gpi_trace_enabled()) { fprintf(stderr, "PMGPI: GpiSetClipPath %s tid=%lu rc=%d mode=%s err=%lu\n", rc ? "OK" : "FAIL", (unsigned long)GetCurrentThreadId(), rc, (options & 4L) ? "AND" : "COPY", (unsigned long)GetLastError()); fflush(stderr); }
    return rc ? 1 : 0;
}

/* 520 */
O2LONG __cdecl GpiSetDefaultViewMatrix(O2HPS hps, O2LONG count,
                                       LONG *m, O2LONG options)
{
    struct CompatPS *p;
    LONG tmp[9];
    O2LONG n;
    p = ps_from(hps);
    if (gpi_trace_enabled()) {
        fprintf(stderr, "PMGPI: GpiSetDefaultViewMatrix hps=%08lX count=%ld opt=%ld m=[%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld]\n",
                (unsigned long)hps, (long)count, (long)options,
                m ? (long)m[0] : 0L, m ? (long)m[1] : 0L, m ? (long)m[2] : 0L,
                m ? (long)m[3] : 0L, m ? (long)m[4] : 0L, m ? (long)m[5] : 0L,
                m ? (long)m[6] : 0L, m ? (long)m[7] : 0L, m ? (long)m[8] : 0L);
        fflush(stderr);
    }
    if (!p || !m || count <= 0) return 0;
    if (count < 9) return 0;
    if (options == 0L) {
        memcpy(p->default_view, m, 9 * sizeof(LONG));
    } else if (options == 1L) {
        gpi_matrix_multiply(tmp, p->default_view, m);
        memcpy(p->default_view, tmp, sizeof(tmp));
    } else if (options == 2L) {
        gpi_matrix_multiply(tmp, m, p->default_view);
        memcpy(p->default_view, tmp, sizeof(tmp));
    } else {
        return 0;
    }
    n = count;
    (void)n;
    if (gpi_trace_enabled()) {
        fprintf(stderr,
                "PMGPI: GpiSetDefaultViewMatrix OK tid=%lu result=[%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld]\n",
                (unsigned long)GetCurrentThreadId(),
                (long)p->default_view[0], (long)p->default_view[1],
                (long)p->default_view[2], (long)p->default_view[3],
                (long)p->default_view[4], (long)p->default_view[5],
                (long)p->default_view[6], (long)p->default_view[7],
                (long)p->default_view[8]);
        fflush(stderr);
    }
    return 1;
}

/* 546 */
O2LONG __cdecl GpiSetRegion(O2HPS hps, O2ULONG hrgn, O2LONG count,
                            O2RECTL *rects)
{
    struct CompatPS *p;
    HRGN dest, one;
    RECT r;
    O2LONG i;
    int h;
    p = ps_from(hps);
    if (!p || !hrgn || count < 0) return 0;
    dest = (HRGN)(DWORD)hrgn;
    SetRectRgn(dest, 0, 0, 0, 0);
    if (count == 0) return 1;
    if (!rects) return 0;
    h = gpi_surface_height(p);
    for (i = 0; i < count; ++i) {
        r.left = rects[i].xLeft;
        r.right = rects[i].xRight;
        r.top = h - rects[i].yTop;
        r.bottom = h - rects[i].yBottom;
        one = CreateRectRgn(r.left, r.top, r.right, r.bottom);
        if (!one) return 0;
        CombineRgn(dest, dest, one, RGN_OR);
        DeleteObject(one);
    }
    return 1;
}

/* 611 */
O2LONG __cdecl GpiDestroyRegion(O2HPS hps, O2ULONG hrgn)
{
    (void)hps;
    if (!hrgn) return 0;
    return DeleteObject((HRGN)(DWORD)hrgn) ? 1 : 0;
}

/* 604 */
O2LONG __cdecl DevCloseDC(O2HDC hdc)
{
    return DeleteDC((HDC)(DWORD)hdc) ? 1 : 0;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)instance; (void)reason; (void)reserved;
    return TRUE;
}
