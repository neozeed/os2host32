#include "blood_ui.h"
#include "blood_upcom.h"
#include <string.h>
static unsigned be(const uint8_t *p) { return (unsigned)p[0]*256u+p[1]; }
static unsigned le(const uint8_t *p) { return p[0]+(unsigned)p[1]*256u; }
static void setbe(uint8_t *p,unsigned v) { p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v; }
static void setle(uint8_t *p,unsigned v) { p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8); }
int blood_ui_sprite(uint8_t *p,const uint8_t *s,size_t size,unsigned row,
                    unsigned height,int x,int y)
{
    unsigned yy,xx,off,bit,c,plane;
    int dx,dy;
    if(!p||!s||size<BU_SHEET||row>=200||height>200-row)return -1;
    for(yy=0;yy<height;yy++)for(xx=0;xx<32;xx++) {
        dx=x+(int)xx;dy=y+(int)yy; off=(row+yy)*4+xx/8;
        bit=128u>>(xx&7u);c=0;
        if((unsigned)dx>=320u||(unsigned)dy>=200u)continue;
        for(plane=0;plane<4;plane++)if(s[800*(plane+1)+off]&bit)c|=1u<<plane;
        p[dy*320+dx]=(uint8_t)((p[dy*320+dx]&(s[off]&bit?15u:0u))|c);
    }
    return 0;
}
void blood_eyes_request(BloodEyes *e,uint16_t mask)
{
    int i;
    unsigned on;
    e->busy=0;
    for(i=9;i>=0;i--,mask>>=1) {
        on=mask&1u;
        if(e->state[i]!=on) {
            e->state[i]=(uint8_t)(0x80|on);e->delay[i]=1;
            e->countdown[i]=(uint8_t)(on?6:5);e->busy=1;
        }
    }
    e->quota=1;e->cadence=3;
}
void blood_eye_step(BloodEyes *e,unsigned i)
{
    if(i>=10||--e->delay[i])return;
    e->delay[i]=1;
    if(--e->countdown[i])e->picture[i]=e->countdown[i]&1u;
    else { e->state[i]&=1u;if(!e->state[i])e->picture[i]=0; }
}
void blood_eyes_tick(BloodEyes *e)
{
    unsigned done=0,remaining=0,i;
    if(!e->busy)return;
    if(!--e->cadence){e->cadence=3;if(e->quota<4)e->quota++;}
    for(i=0;i<10;i++)if(e->state[i]&0x80u){
        if(done<e->quota){blood_eye_step(e,i);done++;}
        if(e->state[i]&0x80u)remaining++;
    }
    e->busy=remaining!=0;
}
void blood_eyes_draw(uint8_t *p,const BloodEyes *e,const uint8_t s[5][BU_SHEET])
{ blood_eyes_draw_mask(p,e,s,0x3ff); }
void blood_eyes_draw_mask(uint8_t *p,const BloodEyes *e,const uint8_t s[5][BU_SHEET],uint16_t visible)
{
    static const uint8_t x[10]={0,48,48,80,112,144,176,208,240,240};
    static const uint8_t y[10]={152,120,152,168,176,152,176,168,152,120};
    unsigned i,bank;
    for(i=0;i<10;i++) {
        if(!(visible&(0x200u>>i)))continue;
        bank=(i<8?1u:2u)+(e->picture[i]?0u:2u);
        blood_ui_sprite(p,s[bank],BU_SHEET,(i<8?i:i-8)*24,24,x[i],y[i]);
    }
}
void blood_hand_tick(BloodHand *h,uint8_t left)
{
    h->pulse=0;
    if(!h->state) { if(!left)return;h->delay=1;h->frame=1;h->state=2; }
    for(;;) {
        if(--h->delay)return;
        h->delay=1;if(--h->frame)return;
        h->frame=4;if(--h->state)return;
        h->pulse++;
        if(!left)return;
        h->delay=1;h->frame=1;h->state=2;
    }
}
void blood_hand_draw(uint8_t *p,const uint8_t *s,const BloodHand *h,int x,int y,int arm)
{
    unsigned row=h->state?((unsigned)h->frame-1u)*16u:64u;
    if(h->state)y-=3;
    blood_ui_sprite(p,s,BU_SHEET,row,16,x,y);
    blood_ui_sprite(p,s,BU_SHEET,80,arm?100:32,x,y+16);
}
void blood_ui_icon(uint8_t *p,const uint8_t *s,unsigned id,uint8_t c,unsigned x,unsigned y)
{
    unsigned off,yy,xx;
    if(id>=128||x>304||y>181)return;
    off=(id>>5)*1520u+(id&1u?760u:0u)+((id&31u)>>1)*2;
    for(yy=0;yy<19;yy++)for(xx=0;xx<16;xx++)
        p[(y+yy)*320+x+xx]=(s[off+yy*40+xx/8]&(128u>>(xx&7u)))?c:0;
}
void blood_ui_text(uint8_t *p,const uint8_t font[472],const char *s,unsigned x,unsigned y)
{
    unsigned ch,yy,xx;
    x&=~7u;
    for(;*s&&x<=312;s++,x+=8) {
        ch=(unsigned char)*s;
        if(ch<32||ch>90)continue;
        for(yy=0;yy<7&&y+yy<200;yy++)for(xx=0;xx<8;xx++)
            p[(y+yy)*320+x+xx]=(font[(ch-32)*8+yy]&(128u>>xx))?7:0;
    }
}
int blood_ui_available(uint8_t map[128],const uint8_t inv[256],const uint8_t *b,size_t n)
{
    unsigned i,token,step;
    size_t p,end;
    if(n<4)return -1;
    for(i=2;i<=108;i++)map[i]|=128;
    p=le(b);
    while(p<n&&b[p]!=255) {
        if(p+2>n||p+2+(b[p]&31u)>n)return -1;
        end=p+2+(b[p]&31u);
        if(!(b[p]&224u))for(i=(unsigned)(p+2);i<end;) {
            token=b[i];step=token<=3?3:1;
            if(inv[token]>=128||i+step>end)return -1;
            map[inv[token]]&=127;i+=step;
        }
        p=end;
    }
    return p<n?0:-1;
}
static int room(BloodAlien *a,size_t p,size_t n)
{ if(p>a->size||n>a->size-p){a->error=-1;return 0;}return 1; }
int blood_alien_attach(BloodAlien *a,uint8_t *b,size_t n)
{
    unsigned d;
    memset(a,0,sizeof(*a));a->bank=b;a->size=n;
    if(!b||!room(a,0,4))return -1;
    d=le(b+2)+16;if(!room(a,d,32))return -1;
    a->descriptor=(uint16_t)d;setle(b+d,(uint16_t)(d+be(b+d+10)));
    return 0;
}
void blood_alien_speech(BloodAlien *a,int talking)
{
    unsigned d=a->descriptor;
    if(!a->error&&be(a->bank+d+6)!=be(a->bank+d+22))
        setle(a->bank+d,(uint16_t)(d+be(a->bank+d+(talking?22:6))));
}
int blood_alien_passenger_pose(BloodAlien *a,uint8_t p[BU_PIXELS])
{
    unsigned d=a->descriptor,g;
    int rc;
    if(a->error||!room(a,d,16))return -1;
    setle(a->bank+d,(uint16_t)(d+be(a->bank+d+14)));
    rc=blood_alien_step(a,p);
    /* PE's pose lacks a terminator before its FFFF sprite-sheet preamble.
     * DOS dispatches slot 15 into the middle of an instruction at 23C8,
     * corrupting graphics bytes. Retain the completed, verified pose image
     * and stop at that data boundary instead of executing artwork as code. */
    g=d+be(a->bank+d+18);
    if(rc==-1&&le(a->bank+d)==g&&room(a,g,10)&&
       be(a->bank+g)==65535u&&be(a->bank+g+2)==0x8007u){
        a->error=0;return 0;
    }
    return rc;
}
void blood_passenger_capture(uint8_t image[4096],const uint8_t p[BU_PIXELS])
{
    unsigned plane,y,x;
    memset(image,0,2800);
    for(plane=0;plane<4;plane++)for(y=0;y<70;y++)for(x=0;x<80;x++)
        if(p[(y+40)*320+x+40]&(1u<<plane))
            image[plane*700+y*10+x/8]|=(uint8_t)(128u>>(x&7u));
}
void blood_passenger_draw(uint8_t p[BU_PIXELS],const uint8_t image[4096])
{
    unsigned y,x,c,off,bit,plane;
    for(y=0;y<70;y++)for(x=0;x<80;x++){
        c=0;off=y*10+x/8;bit=128u>>(x&7u);
        for(plane=0;plane<4;plane++)if(image[plane*700+off]&bit)c|=1u<<plane;
        p[(y+40)*320+x+40]=(uint8_t)c;
    }
}
static void position(unsigned pos,unsigned delta,unsigned *x,unsigned *y)
{ *y=pos/160;*x=((pos%160)&0xfff8u)*2+delta; }
static int shape(BloodAlien *a,uint8_t *p,unsigned id,int overridden,unsigned pos,unsigned delta)
{
    unsigned d=a->descriptor,s=d+32+id,g=d+be(a->bank+d+18),x,y;
    unsigned row,h;
    if(!room(a,s,8)||!room(a,g,BU_SHEET))return -1;
    row=(be(a->bank+s)>>4)&255u;h=(uint8_t)(a->bank[s+3]+1u);
    position(overridden?pos:be(a->bank+s+4),overridden?delta:be(a->bank+s+6),&x,&y);
    return blood_ui_sprite(p,a->bank+g,BU_SHEET,row,h,(int)x,(int)y);
}
static int cache(BloodAlien *a,uint8_t *p,unsigned off)
{
    unsigned key=be(a->bank+off+2),i,x,y,w=(uint8_t)((a->bank[off+7]+1)*2),h=(uint8_t)(a->bank[off+5]+1);
    BloodAlienCache *c;
    unsigned yy;
    for(i=0;i<4;i++)if(a->cache[i].key==key||!a->cache[i].key)break;
    if(i==4)return 0;
    position(be(a->bank+off),0,&x,&y);w=((w+1u)&254u)*8;
    if(x+w>320||y+h>200||w*h>sizeof(a->cache[i].pixels))return -1;
    c=&a->cache[i];c->key=(uint16_t)key;c->x=(uint16_t)x;c->y=(uint16_t)y;
    c->width=(uint16_t)w;c->height=(uint16_t)h;
    for(yy=0;yy<h;yy++)memcpy(c->pixels+yy*w,p+(y+yy)*320+x,w);
    return 0;
}
static void restore(BloodAlien *a,uint8_t *p,unsigned key)
{
    unsigned i,y;
    BloodAlienCache *c;
    for(i=0;i<4;i++)if(a->cache[i].key==key) {
        c=&a->cache[i];
        for(y=0;y<c->height;y++)memcpy(p+(c->y+y)*320+c->x,c->pixels+y*c->width,c->width);
        return;
    }
}
static uint8_t reverse(uint8_t v)
{ v=(uint8_t)((v>>4)|(v<<4));v=(uint8_t)(((v&204u)>>2)|((v&51u)<<2));return (uint8_t)(((v&170u)>>1)|((v&85u)<<1)); }
int blood_alien_step(BloodAlien *a,uint8_t pixels[BU_PIXELS])
{
    unsigned d=a->descriptor;uint8_t *b=a->bank;
    unsigned pc,fuel,op,p,need,row,h,g,plane,y,id,seq,cursor,q;
    int rc;
    uint16_t count;
    static const uint8_t lengths[10]={0,2,2,8,4,4,2,6,0,0};
    if(a->error)return a->error;
    pc=le(b+d);
    for(fuel=0;fuel<4096;fuel++) {
        if(!room(a,pc,2))return -1;
        op=be(b+pc)&15u;p=pc+2;need=0;rc=0;
        if(op>=10)return a->error=-1;
        need=lengths[op];if(!room(a,p,need))return -1;
        switch(op) {
        case 0:p=d+be(b+d+6);break;
        case 1:rc=shape(a,pixels,be(b+p),0,0,0);p+=2;break;
        case 2:restore(a,pixels,be(b+p));p+=2;break;
        case 3:rc=cache(a,pixels,p);p+=8;break;
        case 4: {
            row=(be(b+p)>>4)&255u;h=(uint8_t)(b[p+3]+1);g=d+be(b+d+18);
            if(row+h>200||!room(a,g,BU_SHEET))return a->error=-1;
            for(plane=0;plane<5;plane++)for(y=row;y<row+h;y++) {
                uint8_t *q=b+g+plane*800+y*4;
                uint8_t v0=q[0],v1=q[1];q[0]=reverse(q[3]);q[1]=reverse(q[2]);q[2]=reverse(v1);q[3]=reverse(v0);
            }
            p+=4;break;
        }
        case 5: {
            BloodUpcom rng;memset(&rng,0,sizeof(rng));rng.rng_low=a->rng_low;rng.rng_high=a->rng_high;
            id=(blood_upcom_random(&rng)&be(b+p))+be(b+p+2);
            a->rng_low=rng.rng_low;a->rng_high=rng.rng_high;
            rc=shape(a,pixels,id,0,0,0);p+=4;break;
        }
        case 6: {
            seq=d+be(b+p);if(!room(a,seq,2))return -1;
            cursor=be(b+seq);q=seq+cursor+2;if(!room(a,q,2))return -1;
            if(be(b+q)==65535){setbe(b+seq,0);p+=2;}
            else {if(!room(a,q,6))return -1;setbe(b+seq,(uint16_t)(cursor+6));
                rc=shape(a,pixels,be(b+q),1,be(b+q+2),be(b+q+4));p=pc;}
            break;
        }
        case 7: {
            count=(uint16_t)(be(b+p+2)-1);
            if(count&32768u){setbe(b+p+2,be(b+p+4));p+=6;}
            else {setbe(b+p+2,count);p=(uint16_t)(pc+be(b+p));}break;
        }
        case 8:break;
        case 9:p=d+be(b+d+22);break;
        }
        if(rc<0)return a->error=-1;
        if(!room(a,p,2))return -1;
        setle(b+d,(uint16_t)p);pc=p;
        if(!(b[p]&128u))return 0;
    }
    return a->error=-2;
}

/* 4DF8..4E9B: degradation of the console hand after the grace threshold. */
void blood_hand_tremor(BloodTremor *t,uint16_t minutes,uint16_t grace,uint8_t seconds,
                      int disabled,int *x,int *y,int low_x,int high_x,int low_y,int high_y)
{
    unsigned elapsed,dx,dy,nx,ny;
    if(disabled||minutes<grace)return;
    elapsed=minutes-grace;
    if(elapsed<20u&&((seconds>>3)&1u))return;
    dx=elapsed>=25u?16:8;dy=elapsed>=25u?8:4;
    if(!--t->delay_x){t->delay_x=4;t->delta_x=(uint16_t)(t->delta_x&32768u?dx:0u-dx);}
    if(!--t->delay_y){t->delay_y=5;t->delta_y=(uint16_t)(t->delta_y&32768u?dy:0u-dy);}
    nx=(uint16_t)((unsigned)*x+t->delta_x);ny=(uint16_t)((unsigned)*y+t->delta_y);
    if(nx>(unsigned)low_x&&nx<(unsigned)high_x)*x=(int)nx;
    if(ny>(unsigned)low_y&&ny<(unsigned)high_y)*y=(int)ny;
}
