/* C89 four-connected stroke traversal; caller supplies tool application.
 * Each tile is visited at most once per press, including loops/backtracking. */
#ifndef PM_STROKE_H
#define PM_STROKE_H
#include <string.h>
#define PM_STROKE_W 120
#define PM_STROKE_HGT 100
typedef struct PMStroke {
    int active, x, y;
    unsigned char seen[PM_STROKE_W][PM_STROKE_HGT];
} PMStroke;
static void pm_stroke_begin(PMStroke *s, int x, int y)
{
    memset(s, 0, sizeof(*s)); s->active = 1; s->x = x; s->y = y;
}
static int pm_stroke_move(PMStroke *s, int x, int y,
                          int (*visit)(int, int))
{
    int dx, dy, sx, sy, ix, iy, result;
    if (!s->active) return 0;
    if (x < 0 || x >= PM_STROKE_W || y < 0 || y >= PM_STROKE_HGT ||
        s->x < 0 || s->x >= PM_STROKE_W || s->y < 0 || s->y >= PM_STROKE_HGT) {
        s->active = 0; return 0;
    }
    dx = x-s->x; dy = y-s->y;
    sx = dx < 0 ? -1 : 1; sy = dy < 0 ? -1 : 1;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    ix = iy = 0;
    for (;;) {
        if (!s->seen[s->x][s->y]) {
            s->seen[s->x][s->y] = 1;
            result = visit(s->x, s->y);
            if (result) { s->active = 0; return result; }
        }
        if (ix == dx && iy == dy) break;
        /* Resolve corner ties consistently in both travel directions. */
        if (ix < dx && (iy == dy || (2*ix+1)*dy < (2*iy+1)*dx ||
            ((2*ix+1)*dy == (2*iy+1)*dx && sx > 0))) {
            s->x += sx; ++ix;
        } else { s->y += sy; ++iy; }
    }
    return 0;
}
#endif

