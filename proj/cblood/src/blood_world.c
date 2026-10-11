#include "blood_world.h"
#include "blood_upcom.h"
#include <string.h>
int blood_generate_galaxy(BloodGame *s, uint16_t *lo, uint16_t *hi, uint16_t *cached)
{
    BloodUpcom v;
    unsigned i,j,attempts=0,x,y,start;
    if (!s || !lo || !hi || !cached) return -1;
    memset(&v,0,sizeof(v));v.rng_low=*lo;v.rng_high=*hi;
    for (i=0;i<64;) {
        /* Fail closed for an impossible/corrupted non-progressing RNG. */
        if (++attempts>1000000u) return -1;
        x=((blood_upcom_random(&v)>>8)&0xfeu)+1u;
        y=(blood_upcom_random(&v)>>8)&0x7fu;
        for (j=0;j<i;++j)
            if (s->character[j].wanted_x==x && s->character[j].wanted_y==y) break;
        if (j<i) continue;
        s->character[i].wanted_x=s->character[i].actual_x=(uint8_t)x;
        s->character[i].wanted_y=s->character[i].actual_y=(uint8_t)y;
        ++i;
    }
    /* Four SHL AH,1 instructions select byte offsets 0,16,32,48.
     * With four-byte records these are RECORDS 0,4,8,12, not 0,1,2,3. */
    start=((blood_upcom_random(&v)>>8)&3u)*4u;
    s->planet_x=s->character[start].wanted_x;s->planet_y=s->character[start].wanted_y;
    for (i=40;i<=49;++i) blood_game_set_bit(s,i,1);
    blood_game_set_bit(s,999,0);
    s->destroyed_count=s->destroyed_write=s->visited_write=0;
    *cached=0xffffu;*lo=v.rng_low;*hi=v.rng_high;
    return 0;
}
