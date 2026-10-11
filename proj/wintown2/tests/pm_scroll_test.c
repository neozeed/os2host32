#include "../os2pm/pm_scroll.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    int visibleX, visibleY, limitX, limitY, x, y, i;
    /* A 640x300 client, with the fixed 108-pixel toolbar. The PM frame
     * gives us the actual client size after reserving its scroll controls. */
    visibleX = 640 - 108; visibleY = pm_map_height(300);
    limitX = pm_scroll_limit(120 * 16, visibleX);
    limitY = pm_scroll_limit(100 * 16, visibleY);
    assert(limitX == 1388 && limitY == 1322);

    x = 0; y = 0;
    for (i = 0; i < 200; ++i) {
        x = pm_scroll_position(x, visibleX, limitX, MP_SCROLL_FORWARD, 0, 16);
        y = pm_scroll_position(y, visibleY, limitY, MP_SCROLL_FORWARD, 0, 16);
    }
    /* Includes the partial last tile, rather than rounding its end away. */
    assert(x + visibleX == 1920 && y + visibleY == 1600);
    assert((x + visibleX - 1) / 16 == 119);
    assert((y + visibleY - 1) / 16 == 99);
    for (i = 0; i < 200; ++i) {
        x = pm_scroll_position(x, visibleX, limitX, MP_SCROLL_BACK, 0, 16);
        y = pm_scroll_position(y, visibleY, limitY, MP_SCROLL_BACK, 0, 16);
    }
    assert(x == 0 && y == 0);

    assert(pm_scroll_position(0, visibleX, limitX, MP_SCROLL_PAGE_FORWARD, 0, 16) == 516);
    assert(pm_scroll_position(516, visibleX, limitX, MP_SCROLL_PAGE_BACK, 0, 16) == 0);
    assert(pm_scroll_position(0, visibleX, limitX, MP_SCROLL_THUMB, 517, 16) == 517);
    assert(pm_scroll_position(0, visibleX, limitX, MP_SCROLL_THUMB, 32000, 16) == limitX);
    assert(pm_scroll_position(500, visibleX, limitX, MP_SCROLL_THUMB, -1, 16) == 0);

    /* Maximizing / resizing must clamp old offsets to the new viewport. */
    assert(pm_scroll_clamp(limitX, pm_scroll_limit(1920, 1800)) == 120);
    assert(pm_scroll_clamp(limitY, pm_scroll_limit(1600, 1600)) == 0);
    assert(pm_scroll_limit(1920, 4000) == 0);
    assert(pm_scroll_position(0, 4000, 0, MP_SCROLL_FORWARD, 0, 16) == 0);
    assert(pm_map_height(22) == 1 && pm_map_height(0) == 1);
    /* Minimized and toolbar-only clients must not create negative ranges. */
    assert(pm_scroll_limit(1920, -108) == 1919);
    assert(pm_scroll_limit(1600, 0) == 1599);
    assert(pm_scroll_position(0, 0, 1599, MP_SCROLL_PAGE_FORWARD, 0, 16) == 16);
    puts("PASS scrolling: line/page/thumb, all map edges, resize, oversized and minimized views");
    return 0;
}
