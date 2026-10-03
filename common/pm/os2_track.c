#include "os2_track.h"
static int64_t clip(int64_t n,int64_t lo,int64_t hi)
{ return n<lo?lo:(n>hi?hi:n); }
int os2_track_valid(const struct O2TrackInfo *t)
{
    uint32_t f;if(!t) return 0;f=t->flags;
    if((f&~0x3ffU) || (f&0x100U) || !(f&15U)) return 0;
    if(t->border_x<0 || t->border_y<0 || t->min_x<0 || t->min_y<0 ||
       t->max_x<t->min_x || t->max_y<t->min_y) return 0;
    if(t->rect.right<t->rect.left || t->rect.top<t->rect.bottom) return 0;
    if((int64_t)t->rect.right-t->rect.left>INT32_MAX || (int64_t)t->rect.top-t->rect.bottom>INT32_MAX) return 0;
    if((f&0x200U) && (f&15U)!=15U) return 0;
    if((f&0x20U) && (t->grid_x<=0 || t->grid_y<=0)) return 0;
    if((f&0x280U) && (t->boundary.right<t->boundary.left || t->boundary.top<t->boundary.bottom)) return 0;
    if((f&0x80U) && (f&15U)==15U &&
       ((int64_t)t->rect.right-t->rect.left>(int64_t)t->boundary.right-t->boundary.left ||
        (int64_t)t->rect.top-t->rect.bottom>(int64_t)t->boundary.top-t->boundary.bottom)) return 0;
    if((f&0x80U) && (f&15U)!=15U) {
        if(t->rect.left<t->boundary.left || t->rect.right>t->boundary.right ||
           t->rect.bottom<t->boundary.bottom || t->rect.top>t->boundary.top) return 0;
        if((f&1U) && !(f&4U) && (int64_t)t->rect.right-t->boundary.left<t->min_x) return 0;
        if((f&4U) && !(f&1U) && (int64_t)t->boundary.right-t->rect.left<t->min_x) return 0;
        if((f&8U) && !(f&2U) && (int64_t)t->rect.top-t->boundary.bottom<t->min_y) return 0;
        if((f&2U) && !(f&8U) && (int64_t)t->boundary.top-t->rect.bottom<t->min_y) return 0;
    }
    return 1;
}
static void axis(int32_t lo,int32_t hi,int32_t b0,int32_t b1,int32_t min,int32_t max,
                 int64_t d,int first,int last,uint32_t flags,int32_t *a,int32_t *b)
{
    int64_t l=lo,r=hi,low,high;
    if(first && last) {
        if(flags&0x80U) d=clip(d,(int64_t)b0-l,(int64_t)b1-r);
        else if(flags&0x200U) d=clip(d,(int64_t)b0-r,(int64_t)b1-l);
        d=clip(d,(int64_t)INT32_MIN-l,(int64_t)INT32_MAX-r);l+=d;r+=d;
    } else if(first) {
        low=r-max;high=r-min;
        if((flags&0x80U) && low<b0) low=b0;
        l=clip(l+d,low,high);
    } else if(last) {
        low=l+min;high=l+max;
        if((flags&0x80U) && high>b1) high=b1;
        r=clip(r+d,low,high);
    }
    *a=(int32_t)clip(l,INT32_MIN,INT32_MAX);*b=(int32_t)clip(r,INT32_MIN,INT32_MAX);
}
void os2_track_step(const struct O2TrackInfo *t,int32_t dx,int32_t dy,struct O2TrackRect *r)
{
    int64_t x=dx,y=dy;uint32_t f=t->flags;
    if(f&0x20U) { x=x/t->grid_x*t->grid_x;y=y/t->grid_y*t->grid_y; }
    axis(t->rect.left,t->rect.right,t->boundary.left,t->boundary.right,t->min_x,t->max_x,x,
         (f&1)!=0,(f&4)!=0,f,&r->left,&r->right);
    axis(t->rect.bottom,t->rect.top,t->boundary.bottom,t->boundary.top,t->min_y,t->max_y,y,
         (f&8)!=0,(f&2)!=0,f,&r->bottom,&r->top);
}
