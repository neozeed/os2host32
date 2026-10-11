/* Recovered DOS UI caller: 30DB/33D6 clear, 4080/40A0 dispatch,
 * 4453 input conversion, 4498/451E reply conversion, 2EF0/2F23 hover.
 * The bytecode interpreter stays unchanged in src/blood_upcom.c. */
#include "blood_conversation.h"
#include <string.h>
void blood_conversation_seed(uint16_t minutes,uint16_t seconds,
                             uint16_t service_clock,uint16_t *low,uint16_t *high)
{
    *low=*high=(uint16_t)(minutes+seconds+service_clock);
}
static int append(uint8_t *out, unsigned *n, unsigned value)
{
    if (*n >= BLOOD_REPLY_ICONS) return -1;
    out[(*n)++] = (uint8_t)value;
    return 0;
}
static int digits(uint8_t *out, unsigned *n, unsigned value)
{
    unsigned divisor=100, started=0, i, digit;
    /* 72D8 formats exactly three positions, stripping leading spaces at
     * 452F. Do not silently apply modulo 1000 to a time argument. */
    for (i=0;i<3;i++,divisor/=10) {
        digit=value/divisor; value%=divisor;
        if (digit || started || i==2) {
            started=1;
            if (append(out,n,digit+0x70u)) return -1;
        }
    }
    return 0;
}
int blood_conversation_expand(const BloodUpcom *v,const uint8_t d[2560],
                              const BloodGame *g,uint8_t out[BLOOD_REPLY_ICONS])
{
    unsigned n=0, i, t, hi, lo, id;
    const BloodCharacter *c;
    for(i=0;i<32 && v->reply[i]!=255;i++) {
        t=v->reply[i];
        if(append(out,&n,d[384+t])) return -1;
        if(t>3) continue;
        if(i+2>=32) return -1;
        hi=v->reply[++i];lo=v->reply[++i];
        if(t==2) continue;
        if(t==1) {
            if(digits(out,&n,(hi||lo)?hi*256u+lo:v->minutes)) return -1;
        } else {
            id=(hi?lo*256u+hi:lo)-1u;
            if(id>=64) return -1;
            c=&g->character[id];
            if(digits(out,&n,hi?c->wanted_x:c->actual_x) ||
               append(out,&n,111) ||
               digits(out,&n,(hi?c->wanted_y:c->actual_y)&127u)) return -1;
        }
    }
    return 0;
}
int blood_conversation_begin(BloodUpcom *v,const uint8_t d[2560],
                             const BloodGame *g,uint8_t out[BLOOD_REPLY_ICONS])
{
    int rc;
    memset(out,0,BLOOD_REPLY_ICONS);
    rc=blood_upcom_begin(v);
    if(rc<0) return rc;
    return v->yielded?blood_conversation_expand(v,d,g,out):0;
}
int blood_conversation_click(BloodUpcom *v,uint8_t phrase[8],
                             const uint8_t d[2560],const BloodGame *g,
                             uint8_t out[BLOOD_REPLY_ICONS])
{
    int rc;
    /* 30DB clears visible reply even when 40A0 returns without a yield.
     * The original intentionally retains the VM reply bytes in that case. */
    memset(out,0,BLOOD_REPLY_ICONS);
    if(v->active) rc=blood_upcom_next(v);
    else {
        uint8_t tokens[24]; unsigned n=0, i, t;
        for(i=0;i<8;i++) if(phrase[i]) {
            t=d[256+phrase[i]]; tokens[n++]=(uint8_t)t;
            if(t==3) tokens[n-1]=0;
            else if(t<=2) {tokens[n++]=0;tokens[n++]=0;}
        }
        if(!n) {v->input[0]=255;v->active=0;return 0;}
        memset(phrase,0,8); /* 33BB: only a submitted phrase is consumed. */
        rc=blood_upcom_submit(v,tokens,n);
    }
    if(rc<0) return rc;
    return v->yielded?blood_conversation_expand(v,d,g,out):0;
}
unsigned blood_conversation_hover(const uint8_t out[BLOOD_REPLY_ICONS],
                                  unsigned visible,const uint8_t map[128],
                                  int x,int y)
{
    unsigned slot;
    if(x<16 || x>=144 || y<112 || y>=130) return 255;
    slot=(unsigned)(x-16)/16;
    if(slot>=visible || out[slot]>=128) return 255;
    return out[slot]==111?123:map[out[slot]]&127u;
}
