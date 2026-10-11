/* C89 viewport arithmetic shared by the PM controls and hosted checks. */
#ifndef PM_SCROLL_H
#define PM_SCROLL_H

#define MP_STATUS_HEIGHT 22

static int pm_map_height(int clientHeight)
{
    return clientHeight > MP_STATUS_HEIGHT ? clientHeight - MP_STATUS_HEIGHT : 1;
}

#define MP_SCROLL_BACK 1
#define MP_SCROLL_FORWARD 2
#define MP_SCROLL_PAGE_BACK 3
#define MP_SCROLL_PAGE_FORWARD 4
#define MP_SCROLL_THUMB 5

static int pm_scroll_limit(int world, int visible)
{
    if (visible < 1) visible = 1;
    return world > visible ? world - visible : 0;
}

static int pm_scroll_clamp(int position, int limit)
{
    if (position < 0) return 0;
    return position > limit ? limit : position;
}

/* Keep the world point at the viewport center while changing tile size.
 * Clamp only after rescaling, including an oversized/minimized viewport. */
static int pm_zoom_offset(int offset, int visible, int oldTile,
                          int newTile, int worldTiles)
{
    long position;
    if (visible < 1) visible = 1;
    position = ((long)offset + visible / 2) * newTile / oldTile;
    position -= visible / 2;
    return pm_scroll_clamp((int)position,
                          pm_scroll_limit(worldTiles * newTile, visible));
}

static int pm_scroll_position(int position, int visible, int limit,
                              int action, int thumb, int tile)
{
    int page;
    page = visible - tile; /* Keep one tile of overlap between pages. */
    if (page < tile) page = tile;
    switch (action) {
    case MP_SCROLL_BACK: position -= tile; break;
    case MP_SCROLL_FORWARD: position += tile; break;
    case MP_SCROLL_PAGE_BACK: position -= page; break;
    case MP_SCROLL_PAGE_FORWARD: position += page; break;
    case MP_SCROLL_THUMB: position = thumb; break;
    }
    return pm_scroll_clamp(position, limit);
}

#endif

