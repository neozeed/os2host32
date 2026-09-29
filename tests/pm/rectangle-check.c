#include <assert.h>
#include <stdio.h>
#include <string.h>
#define __cdecl
typedef long LONG;
typedef long O2LONG;
typedef unsigned long DWORD;
typedef unsigned long O2ULONG;
typedef unsigned long COLORREF;
typedef COLORREF HBRUSH;
typedef COLORREF HPEN;
typedef COLORREF HGDIOBJ;
typedef struct { LONG left, top, right, bottom; } RECT;
typedef struct { O2LONG x, y; } O2POINTL;
typedef struct { O2LONG xLeft, yBottom, xRight, yTop; } O2RECTL;
struct FakeDC { RECT last; COLORREF brush; int fills, boxes; };
typedef struct FakeDC *HDC;
typedef struct FakeDC *HWND;
struct CompatBitmap { LONG width, height; int bitcount, stride; unsigned char *bits; };
struct CompatPS {
    HDC dc;
    HWND hwnd;
    struct CompatBitmap *bitmap;
    O2LONG current_x, current_y, color;
};
typedef struct CompatPS *O2HPS;
#define O2_SYSCLR_WINDOW -20L
#define COLOR_WINDOW 5
#define NULL_BRUSH 0
#define PS_SOLID 0
#define VERTRES 10
#define ERROR 0
static struct CompatPS *ps_from(O2HPS h) { return h; }
static void pm_trace(const char *s, unsigned long a, unsigned long b, unsigned long c)
{ (void)s; (void)a; (void)b; (void)c; }
static COLORREF os2_color(O2LONG c) { return c == -2 ? 0xffffffUL : (COLORREF)c; }
static COLORREF gpi_native_color(O2LONG c) { return os2_color(c); }
static int GetClientRect(HWND h, RECT *r)
{ (void)h; r->left = r->top = 0; r->right = 640; r->bottom = 480; return 1; }
static int GetClipBox(HDC h, RECT *r) { return GetClientRect(h, r); }
static int GetDeviceCaps(HDC h, int n) { (void)h; (void)n; return 480; }
static void client_rect_os2(HWND h, O2RECTL *r)
{ (void)h; r->xLeft = r->yBottom = 0; r->xRight = 640; r->yTop = 480; }
static HBRUSH CreateSolidBrush(COLORREF c) { return c; }
static HBRUSH GetSysColorBrush(int c) { (void)c; return 0xffffffUL; }
static int DeleteObject(HGDIOBJ o) { (void)o; return 1; }
static int FillRect(HDC dc, const RECT *r, HBRUSH brush)
{ dc->last = *r; dc->brush = brush; ++dc->fills; return 1; }
static HPEN CreatePen(int s, int w, COLORREF c) { (void)s; (void)w; return c; }
static HGDIOBJ SelectObject(HDC dc, HGDIOBJ o) { (void)dc; return o; }
static HGDIOBJ GetStockObject(int n) { return (HGDIOBJ)n; }
static int Rectangle(HDC dc, LONG l, LONG t, LONG r, LONG b)
{ dc->last.left = l; dc->last.top = t; dc->last.right = r; dc->last.bottom = b;
  ++dc->boxes; return 1; }

#include "rectangle-functions.inc"

int main(void)
{
    struct FakeDC dc;
    struct CompatPS ps;
    struct CompatBitmap bm;
    O2RECTL r;
    O2POINTL p;
    int bpp;
    memset(&dc, 0, sizeof(dc)); memset(&ps, 0, sizeof(ps));
    memset(&bm, 0, sizeof(bm));
    ps.dc = &dc; ps.bitmap = &bm; ps.color = -2;
    bm.width = 108; bm.height = 240;
    r.xLeft = 2; r.xRight = 34; r.yBottom = 210; r.yTop = 236;
    assert(WinFillRect(&ps, &r, -2));
    assert(dc.last.top == 4 && dc.last.bottom == 30);
    assert(dc.last.left == 2 && dc.last.right == 34 && dc.brush == 0xffffffUL);
    assert(WinFillRect(&ps, NULL, -2));
    assert(dc.last.top == 0 && dc.last.bottom == 240 && dc.last.right == 108);
    for (bpp = 4; bpp <= 32; bpp *= 2) {
        bm.bitcount = bpp;
        ps.current_x = 2; ps.current_y = 210; p.x = 33; p.y = 235;
        dc.boxes = 0;
        assert(GpiBox(&ps, 2, &p, 0, 0));
        assert(dc.boxes == 1 && dc.last.top == 4 && dc.last.bottom == 30);
        assert(dc.last.left == 2 && dc.last.right == 34);
        assert(ps.current_x == 33 && ps.current_y == 235);
        ps.current_x = 2; ps.current_y = 210; dc.fills = 0;
        assert(GpiBox(&ps, 1, &p, 0, 0));
        assert(dc.fills == 1 && dc.brush == 0xffffffUL);
    }
    ps.bitmap = NULL; ps.hwnd = &dc;
    assert(WinFillRect(&ps, &r, -2));
    assert(dc.last.top == 244 && dc.last.bottom == 270);
    puts("PASS: memory/window rectangle coordinates, 4/8/16/32-bit DC routing, outlines and fills");
    return 0;
}
