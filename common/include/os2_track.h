#ifndef OS2_TRACK_H
#define OS2_TRACK_H
#include <stdint.h>
struct O2TrackRect { int32_t left,bottom,right,top; };
struct O2TrackInfo {
    int32_t border_x,border_y,grid_x,grid_y,key_x,key_y;
    struct O2TrackRect rect,boundary;
    int32_t min_x,min_y,max_x,max_y;
    uint32_t flags;
};
int os2_track_valid(const struct O2TrackInfo *);
void os2_track_step(const struct O2TrackInfo *,int32_t,int32_t,struct O2TrackRect *);
#endif
