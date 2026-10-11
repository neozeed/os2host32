/* Pure C89 minimap coordinates. All input coordinates use a top origin. */
#ifndef PM_MINIMAP_H
#define PM_MINIMAP_H
static int pm_mini_position(int point, int origin, int extent,
                            int world, int visible)
{
    long center;
    int limit;
    if (extent < 1) return 0;
    point -= origin;
    if (point < 0) point = 0;
    if (point > extent) point = extent;
    center = (long)point * world / extent - visible / 2;
    limit = world > visible ? world - visible : 0;
    if (center < 0) center = 0;
    if (center > limit) center = limit;
    return (int)center;
}
#endif

