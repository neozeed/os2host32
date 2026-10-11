#include "blood_terrain.h"
#include "blood_upcom.h"
#include "blood_flight.h"
#include <string.h>

static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] | ((unsigned)p[1]<<8)); }
static void set16(uint8_t *p, unsigned x) { p[0]=(uint8_t)x; p[1]=(uint8_t)(x>>8); }
uint16_t blood_terrain_word(const BloodTerrain *t, unsigned p)
{ return t && p<65535u ? get16(t->mem+p) : 0; }
void blood_terrain_put_word(BloodTerrain *t, unsigned p, uint16_t v)
{ if (t && p<65535u) set16(t->mem+p,v); }
#define W(p) get16(t->mem+(p))
#define P(p,v) set16(t->mem+(p),(unsigned)(v))
#define B(p) t->mem[(p)]
static uint16_t tab(const BloodTerrainTables *a, unsigned ds)
{ return get16(a->data+ds-0x149c); }
static int signed8(unsigned n) { return (n&128u) ? (int)(n&255u)-256 : (int)(n&255u); }
static int signed16(unsigned n) { return (n&32768u) ? (int)(n&65535u)-65536 : (int)(n&65535u); }
static int sar(int n, unsigned s)
{ return n>=0 ? n/(1<<s) : -((-n+(1<<s)-1)/(1<<s)); }
static uint8_t average(unsigned a, unsigned b) { return (uint8_t)((a+b)>>1); }
static uint8_t floor_byte(unsigned a, unsigned f) { return (uint8_t)(a>f?a:f); }
static uint16_t main_random(BloodTerrain *t)
{
    BloodUpcom v;
    uint16_t result;
    /* Reuse the already verified recurrence, rather than a host RNG. Only its
     * two documented input fields are read by blood_upcom_random(). */
    v.rng_low=t->rng_low; v.rng_high=t->rng_high;
    result=blood_upcom_random(&v);
    t->rng_low=v.rng_low; t->rng_high=v.rng_high;
    return result;
}
static uint16_t local_random(BloodTerrain *t)
{
    /* Original 7C83. The ADC includes carry from the preceding 7*s addition. */
    static const char operations[]="DSDSASDDSDDSDDSDSDDSDDS";
    uint16_t seed=W(BT_LOCAL_RNG), x=seed;
    unsigned i,carry=0;
    for(i=0;i<sizeof(operations)-1u;i++) {
        uint32_t sum=(uint32_t)x+(operations[i]=='S'?seed:x)
                     +(operations[i]=='A'?carry:0u);
        carry=sum>65535u; x=(uint16_t)sum;
    }
    x=(uint16_t)(x+1u); P(BT_LOCAL_RNG,x); return x;
}
static int valid(const BloodTerrain *t)
{ return t && t->initialized && (W(BT_SPAN)==1024u || W(BT_SPAN)==512u); }
static int modifier_index(unsigned ip)
{
    static const uint16_t list[10]={0x5981,0x5987,0x5990,0x5998,0x599b,
                                   0x599e,0x59ac,0x59ba,0x59c3,0x59c6};
    unsigned i; for(i=0;i<10;i++) if(ip==list[i]) return (int)i;
    return -1;
}
static uint8_t adjust_random(BloodTerrain *t, unsigned rule, unsigned shift, unsigned span)
{
    uint16_t r;
    switch(rule) {
    case 0: return (uint8_t)sar(signed8(local_random(t)),shift);
    case 1: r=(uint16_t)(local_random(t)|0xff80u); return (uint8_t)sar(signed16(r),shift);
    case 2: return (uint8_t)((local_random(t)&127u)>>shift);
    case 3: return 252;
    case 4: return 3;
    case 5: r=(uint16_t)(W(BT_LOCAL_RNG)+1u); P(BT_LOCAL_RNG,r); return (uint8_t)sar(signed8(r),shift);
    case 6: r=(uint16_t)(W(BT_LOCAL_RNG)-1u); P(BT_LOCAL_RNG,r); return (uint8_t)sar(signed8(r),shift);
    case 7: return (uint8_t)(local_random(t)&(span-1u));
    case 8: return 0;
    case 9: return (uint8_t)(local_random(t)&0x8fu);
    default: return 0; /* indexes are checked when reading the original tables */
    }
}
static uint8_t midpoint(BloodTerrain *t, unsigned a, unsigned b,
                         unsigned rule, unsigned shift, unsigned span, int ordinary)
{
    unsigned base=average(a,b), noise=adjust_random(t,rule,shift,span);
    uint8_t result=(uint8_t)(base+noise);
    /* Preserve byte sign/overflow tests, not an invented floating clamp. In
     * ordinary refinement positive 127 is rejected; canyon accepts 127. */
    if(noise&128u) { if(result&128u) result=0; }
    else if(result >= (ordinary?127u:128u)) result=(uint8_t)base;
    return result;
}

int blood_terrain_tables(BloodTerrainTables *out,const uint8_t *exe,size_t bytes)
{
    unsigned i;
    if(!out || !exe || bytes!=35585u || exe[0]!='M' || exe[1]!='Z') return -1;
    memcpy(out->data,exe+0x179c,BLOOD_TERRAIN_TABLE_BYTES);
    memcpy(out->pixel_masks,exe+0x490,8);
    out->drift_half=(uint8_t)(exe[0x2333]!=0);
    memcpy(out->glyphs,exe+0x1a00,38);
    for(i=0;i<10;i++) if(modifier_index(tab(out,0x16c0+2*i))!=(int)i) return -1;
    if(tab(out,0x169c)!=0 || tab(out,0x16ac)!=4096) return -1;
    for(i=0;i<8;i++) if(out->pixel_masks[i]!=(128u>>i)) return -1;
    return 0;
}
void blood_terrain_height(BloodTerrain *t,uint8_t height)
{
    if(!t) return;
    B(BT_CAMERA)=height; t->projection_bias=(uint8_t)((height>>1)+32u);
}
int blood_terrain_init(BloodTerrain *t,const BloodTerrainTables *a,uint8_t x,uint8_t y)
{
    unsigned i;
    if(!t || !a) return -1;
    memset(t,0,sizeof(*t));
    t->initialized=1;
    t->rng_low=t->rng_high=(uint16_t)(x|((unsigned)y<<8));
    P(BT_LOCAL_RNG,t->rng_low); P(BT_SECTION_SEED,t->rng_low);
    P(BT_SPAN,1024); P(BT_CENTER,512); P(BT_COURSE,512);
    memcpy(t->mem+BT_VSCALE,a->data,256);
    memcpy(t->mem+BT_HSCALE,a->data+256,256);
    memcpy(t->mem+0x2080,a->data+0x200,32);
    memcpy(t->mem+0x20c0,a->data+0x212,32);
    memcpy(t->mem+0x2078,a->pixel_masks,8);
    memset(t->mem,255,0x2000);
    P(BT_SCREEN_ROWS,0);
    for(i=0;i<126;i++) P(BT_SCREEN_ROWS+2+2*i,0x16acu-40u*i);
    P(BT_SCREEN_ROWS+254,0);
    P(BT_CAMERA,0x217e); B(BT_PREVIOUS_HEIGHT)=126;
    blood_terrain_height(t,126);
    B(BT_PHASE)=15; P(BT_DISTANCE,4095); P(BT_SPEED,12); P(BT_KEY_DIVIDER,1);
    P(BT_SPARE,0x2700); P(BT_SECTION,1); P(BT_RECIPE,0x4af0); P(BT_SECTION_LEFT,1);
    B(BT_ATTACK_DELAY)=48; B(BT_ATTACK_PHASE)=1; B(BT_STEER_INDICATOR)=31;
    B(BT_HEIGHT_INDICATOR)=62; B(BT_INDICATOR_DIV)=1; B(BT_INDICATOR_PHASE)=1;
    for(i=0;i<8;i++) { P(BT_STRIPS+2*i,0x2700+1024*i); P(BT_CENTERS+2*i,512); }
    return 0;
}

/* 7E82: piecewise-constant canyon steering. Note the original reads the half
 * flag through MAIN DS, not ES. The exact B2 constant is nonzero even in full
 * resolution. R19 retains that distinction via the supplied numeric table. */
static void canyon_drift(BloodTerrain *t,const BloodTerrainTables *a,
                         uint8_t *count,uint8_t *velocity)
{
    uint16_t r; unsigned b;
    *count=(uint8_t)(*count-1u);
    if(*count) return;
    r=(uint16_t)(main_random(t)>>1);
    *count=(uint8_t)(((r>>8)&7u)+1u);
    b=r&0x8fu; if(a->drift_half) b&=0xf7u;
    *velocity=(uint8_t)((b&128u)? -(int)((b*2u)&255u):(int)(b*2u));
}
static void canyon_recipe(BloodTerrain *t,const BloodTerrainTables *a)
{
    uint16_t grid,center=512;
    uint8_t distance=1, minimum, count=1,velocity=0,width;
    unsigned i;
    grid=(main_random(t)&0x400u)?16:32;
    if(B(BT_HALF)) grid>>=1;
    P(BT_CANYON_GRID,grid); P(BT_RECIPE,BT_RECIPES);
    minimum=(uint8_t)(((main_random(t)>>8)&3u)+1u);
    canyon_drift(t,a,&count,&velocity);
    if(B(BT_HALF)) center>>=1;
    width=(uint8_t)(((main_random(t)>>8)&7u)+8u);
    for(i=0;i<255;i++) {
        unsigned p=BT_RECIPES+4*i;
        int trial;
        /* 7F06 shifts DL BEFORE the repeated subtraction. The total offset
         * is width*grid/2, retaining its odd-width half-grid term. */
        P(p,(uint16_t)(center-(width/2u)*grid-(width&1u)*(grid/2u)));
        B(p+2)=width;
        distance=(uint8_t)(distance-1u);
        if(!distance && width>minimum) {
            width--; distance=(uint8_t)(((main_random(t)>>8)&7u)+1u);
        }
        canyon_drift(t,a,&count,&velocity);
        trial=(uint16_t)(center+signed8(velocity));
        if(trial<(B(BT_HALF)?128:256)) trial=B(BT_HALF)?128:256;
        if(trial>(B(BT_HALF)?384:768)) trial=B(BT_HALF)?384:768;
        center=(uint16_t)trial;
    }
    i=(main_random(t)>>8)&3u;
    t->canyon_modifier=(uint8_t)modifier_index(tab(a,0x16d4+2*i));
    i=(main_random(t)>>8)&3u;
    t->edge_modifier=(uint8_t)modifier_index(tab(a,0x16dc+2*i));
    i=((main_random(t)>>8)&15u)+3u;
    B(BT_CANYON_MASK)=7; B(BT_CANYON_BASE)=(uint8_t)(B(BT_FLOOR)>=i?B(BT_FLOOR)-i:0);
    P(BT_EDGE_GRID,B(BT_HALF)?32:64);
}
static int ordinary_strip(BloodTerrain *t)
{
    unsigned p,base,spacing,count,mask,bias,shift,n,i,half;
    int index;
    P(BT_SECTION_LEFT,(uint16_t)(W(BT_SECTION_LEFT)-1u));
    if(!W(BT_SECTION_LEFT)) {
        P(BT_SECTION,W(BT_SECTION)+1u);
        p=(uint16_t)(W(BT_RECIPE)+16u);
        if(p+8u>=65536u) return -1;
        P(BT_RECIPE,p); P(BT_SECTION_LEFT,B(p+8u));
    }
    P(BT_SECTION_SEED,W(BT_SECTION_SEED)+1u); P(BT_LOCAL_RNG,W(BT_SECTION_SEED));
    p=W(BT_RECIPE); if(p+8>=65536u) return -1;
    base=W(BT_SPARE); spacing=W(p); count=W(p+2); mask=B(p+4); bias=B(p+5);
    if(spacing<8 || spacing>256 || (spacing&(spacing-1)) || count*spacing!=W(BT_SPAN) || base+W(BT_SPAN)>65536u) return -1;
    index=modifier_index(W(p+6)); if(index<0) return -1;
    t->ordinary_modifier=(uint8_t)index;
    for(i=0;i<count;i++) B(base+i*spacing)=(uint8_t)((local_random(t)&mask)+bias);
    n=count; shift=2;
    do {
        half=spacing/2u;
        for(i=0;i<n;i++) {
            unsigned left=base+i*spacing, right=i+1u==n?base:left+spacing;
            B(left+half)=midpoint(t,B(left),B(right),(unsigned)index,shift,half,1);
        }
        spacing=half; n*=2; shift++;
    } while(spacing!=4);
    B(BT_INTERPOLATED)=0;
    return 0;
}
static int canyon_strip(BloodTerrain *t)
{
    unsigned base=W(BT_SPARE),p=W(BT_RECIPE),spacing=W(BT_CANYON_GRID),n,i,shift=3,start,end;
    if(base+W(BT_SPAN)>65536u || p+2>=65536u || spacing<8 || spacing>32 || (spacing&(spacing-1u)) ||
       t->canyon_modifier>=10 || t->edge_modifier>=10) return -1;
    memset(t->mem+base,127,W(BT_SPAN));
    P(BT_SECTION_SEED,W(BT_SECTION_SEED)+1u);P(BT_LOCAL_RNG,W(BT_SECTION_SEED));
    n=B(p+2); start=base+W(p); end=start+n*spacing;
    if(!n || start<W(BT_EDGE_GRID) || end+W(BT_EDGE_GRID)>=base+W(BT_SPAN)) return -1;
    for(i=0;i<=n;i++) B(start+i*spacing)=(uint8_t)(((local_random(t)>>8)&B(BT_CANYON_MASK))+B(BT_CANYON_BASE));
    do {
        unsigned half=spacing/2;
        for(i=0;i<n;i++) {
            unsigned left=start+i*spacing;
            B(left+half)=midpoint(t,B(left),B(left+spacing),t->canyon_modifier,shift,half,0);
        }
        spacing=half; n*=2; shift++;
    } while(spacing!=4);
    P(BT_EDGE_LEFT,start-W(BT_EDGE_GRID)); P(BT_EDGE_RIGHT,end);
    P(BT_PROGRESS,W(BT_PROGRESS)+1u); P(BT_SECTION,W(BT_SECTION)+1u);
    P(BT_RECIPE,W(BT_RECIPE)+4u); B(BT_INTERPOLATED)=0;
    return 0;
}
int blood_terrain_generate_strip(BloodTerrain *t,const BloodTerrainTables *a)
{
    if(!valid(t) || !a) return -1;
    t->operations++;
    if(W(BT_SECTION)<20) return ordinary_strip(t);
    if(W(BT_SECTION)==20) canyon_recipe(t,a);
    return canyon_strip(t);
}
static unsigned refine_edge(BloodTerrain *t,unsigned start)
{
    unsigned spacing=W(BT_EDGE_GRID),n=1,shift=2,i,end=start+spacing;
    do {
        unsigned half=spacing/2;
        for(i=0;i<n;i++) {
            unsigned p=start+i*spacing;
            B(p+half)=midpoint(t,B(p),B(p+spacing),t->edge_modifier,shift,half,0);
        }
        spacing=half;n*=2;shift++;
    } while(spacing!=4);
    return end;
}
int blood_terrain_interpolate_strip(BloodTerrain *t)
{
    unsigned start,end,floor,left,first,i;
    int circular;
    if(!valid(t)) return -1;
    floor=B(BT_FLOOR); circular=!W(BT_PROGRESS);
    if(circular) { start=W(BT_SPARE); end=start+W(BT_SPAN); }
    else {
        start=W(BT_EDGE_LEFT); end=W(BT_EDGE_RIGHT)+W(BT_EDGE_GRID);
        if(end>=65536u || start>=end || start+W(BT_EDGE_GRID)>=65536u ||
           W(BT_EDGE_GRID)<8 || W(BT_EDGE_GRID)>64 ||
           (W(BT_EDGE_GRID)&(W(BT_EDGE_GRID)-1u)) || t->edge_modifier>=10) return -1;
        (void)refine_edge(t,start); (void)refine_edge(t,W(BT_EDGE_RIGHT));
    }
    if(end>65536u || start>=end || ((end-start)&3u)) return -1;
    first=left=B(start); B(start)=floor_byte(left,floor);
    for(i=start;i<end;i+=4) {
        unsigned right,middle;
        if(circular && i+4==end) right=first;
        else { right=B(i+4);B(i+4)=floor_byte(right,floor); }
        middle=average(left,right);
        B(i+1)=floor_byte(average(left,middle),floor);
        B(i+2)=floor_byte(middle,floor);
        B(i+3)=floor_byte(average(middle,right),floor);
        left=right;
    }
    B(BT_INTERPOLATED)=1; t->operations++; return 0;
}
int blood_terrain_prepare(BloodTerrain *t,const BloodTerrainTables *a)
{
    unsigned i,p,k,f;
    if(!valid(t)||!a) return -1;
    for(i=0;i<96;i++) {
        p=BT_RECIPES+16*i;
        B(p+8)=(uint8_t)(((main_random(t)>>8)&15u)+1u);
        do { k=(main_random(t)>>8)&7u; } while(k>=4);
        P(p,tab(a,0x16e4+4*k)>>(B(BT_HALF)?1:0));
        P(p+2,tab(a,0x16e6+4*k)); B(p+4)=63;
        B(p+5)=(uint8_t)((main_random(t)>>8)&31u);
        do { k=main_random(t)&15u; } while(k>9);
        P(p+6,tab(a,0x16c0+2*k));
    }
    B(BT_FLOOR)=(uint8_t)(((main_random(t)>>8)&31u)+16u);
    f=B(BT_FLOOR)+1u;
    for(i=0;i<256;i++) B(BT_COLOURS+i)=(uint8_t)(i<=f?11:9);
    for(i=0;i<9;i++) {
        if(blood_terrain_generate_strip(t,a)||blood_terrain_interpolate_strip(t)) return -1;
        if(i<8) P(BT_SPARE,W(BT_SPARE)+1024u);
    }
    B(BT_INTERP_DELAY)=255;return 0;
}

int blood_terrain_control(BloodTerrain *t,const BloodFlightInput *input)
{
    int delta,trial;
    unsigned oldh,newh,i,center,event,speed;
    if(!valid(t)||!input) return -1;
    /* The historical cockpit's logical X displacement changes altitude; Y
     * changes the circular strip center. This API does not impose a new mapping. */
    delta=input->horizontal;
    if(input->alternate_axis) delta=(!delta || input->button_code==0x81u)?0:(delta>0?-4:4);
    else {
        delta=sar(delta,1);
        if(delta>4) delta=4;
        if(delta< -4) delta=-4;
        delta=-delta;
    }
    if(delta) {
        newh=(uint8_t)(B(BT_CAMERA)+delta);
        if(newh<B(BT_FLOOR)) newh=B(BT_FLOOR);
        if(newh>124) newh=124;
        if(newh!=B(BT_CAMERA)) {
            blood_terrain_height(t,(uint8_t)newh);
            oldh=B(BT_PREVIOUS_HEIGHT); B(BT_PREVIOUS_HEIGHT)=(uint8_t)newh;
            trial=(uint8_t)(B(BT_HEIGHT_INDICATOR)+((uint8_t)(newh-oldh)*4u));
            if(trial>=8 && trial<=120) B(BT_HEIGHT_INDICATOR)=(uint8_t)trial;
        }
    }
    delta=input->vertical;
    if(input->alternate_axis) delta=delta?(delta<0?-5:5):0;
    else {
        delta=sar(delta,2);
        if(delta>8) delta=8;
        if(delta< -8) delta=-8;
    }
    if(delta) {
        if(B(BT_HALF)) delta=sar(delta,1);
        center=(uint16_t)(W(BT_CENTER)+delta);
        if(center&0x8000u) center=(uint16_t)(center+W(BT_SPAN));
        else if(center>=W(BT_SPAN)) center-=W(BT_SPAN);
        P(BT_CENTER,center);
        for(i=0;i<8;i++) P(BT_CENTERS+2*i,center);
        trial=(uint8_t)(B(BT_STEER_INDICATOR)+delta);
        if((trial&128) || trial<4) trial=4;
        else if(trial>=60) trial=60;
        B(BT_STEER_INDICATOR)=(uint8_t)trial;
    }
    speed=W(BT_SPEED); event=input->speed_event;
    delta=0;
    if(input->mouse_mode) {
        if(event&128u) delta=-2;
        else if(event&64u) delta=2;
    } else {
        event&=0x83u;
        if(event==0x81u) delta=2;
        else if(event==0x82u) delta=-2;
    }
    if(!delta) {
        P(BT_KEY_DIVIDER,W(BT_KEY_DIVIDER)-1u);
        if(!W(BT_KEY_DIVIDER)) {
            P(BT_KEY_DIVIDER,2);
            if(input->bios_modifiers&8u) delta=-2;
            else if(input->bios_modifiers&4u) delta=2;
        }
    }
    if(delta<0 && speed!=0) P(BT_SPEED,speed-2u);
    if(delta>0 && speed!=16) P(BT_SPEED,speed+2u);
    return 0;
}
int blood_terrain_advance(BloodTerrain *t,const BloodTerrainTables *a)
{
    uint16_t distance,first;
    unsigned speed,i,p;
    if(!valid(t)||!a || W(BT_SPEED)>16 || (W(BT_SPEED)&1u)) return -1;
    t->sound_event=0;
    speed=W(BT_SPEED); distance=(uint16_t)(W(BT_DISTANCE)-W(0x2080+speed));
    if((distance>>8)<=16u) {
        B(BT_INTERP_DELAY)--;
        if(!B(BT_INTERP_DELAY) && !B(BT_INTERPOLATED))
            if(blood_terrain_interpolate_strip(t)) return -1;
    } else {
        if(!B(BT_INTERPOLATED) && blood_terrain_interpolate_strip(t)) return -1;
        B(BT_IMPACT)=0;
        p=W(BT_STRIPS)+W(BT_CENTERS);
        if(p>=65536u) return -1;
        if(!B(BT_NO_COLLISION) && B(BT_CAMERA)<B(p)) {
            P(BT_SPEED,0); B(BT_IMPACT)=1;
            memset(t->mem,255,0x2000);
            B(BT_SOUND_DIV)=2; t->sound_event=2; /* original play sound 4 */
            B(BT_IMPACTS)++;
            if(B(BT_IMPACTS)>=8) { t->failed=1; return 2; }
            return 1; /* distance/phase and old horizon were NOT updated */
        }
        first=W(BT_STRIPS);
        for(i=0;i<7;i++) {
            P(BT_STRIPS+2*i,W(BT_STRIPS+2*i+2));
            P(BT_CENTERS+2*i,W(BT_CENTERS+2*i+2));
        }
        P(BT_STRIPS+14,W(BT_SPARE)); P(BT_SPARE,first);
        if(blood_terrain_generate_strip(t,a)) return -1;
        B(BT_INTERP_DELAY)=(uint8_t)W(0x20c0+W(BT_SPEED));
        P(BT_CENTERS+14,W(BT_CENTER));
        distance=(uint16_t)(distance+0x1000u);
    }
    P(BT_DISTANCE,distance); B(BT_PHASE)=(uint8_t)(distance>>8);
    if(B(BT_SOUND_DIV)) {
        B(BT_SOUND_DIV)--;
        if(!B(BT_SOUND_DIV)) t->sound_event=1; /* stop sound */
    }
    return 0;
}

/* Build a table from unsigned height to projected signed-looking screen
 * height (bias 128). The assembly fills away from the camera in both
 * directions and saturates on 16-bit carry/borrow. Values128..255 are left
 * alone; original terrain samples are0..127. */
static void height_lookup(BloodTerrain *t,unsigned depth)
{
    unsigned h=B(BT_CAMERA),i,scale=W(BT_VSCALE+depth);
    for(i=h;i<128;i++) {
        uint32_t v=0x8000u+(i-h)*scale;
        B(BT_HEIGHT_LUT+i)=(uint8_t)(v>65535u?255u:v>>8);
    }
    for(i=0;i<=h && i<128;i++) {
        uint32_t v=(h-i)*scale;
        B(BT_HEIGHT_LUT+i)=(uint8_t)(v>0x8000u?0u:(0x8000u-v)>>8);
    }
}
static uint8_t projected_height(BloodTerrain *t,unsigned raw,unsigned bias)
{
    unsigned p=B(BT_HEIGHT_LUT+raw)+bias;
    return (uint8_t)(p>255u?255u:p);
}
static int append_point(BloodTerrain *t,unsigned x,unsigned y,unsigned colour)
{
    unsigned at=W(BT_COMMAND_END);
    if(at+4u>0x2000u) return -1;
    B(at)=(uint8_t)x; B(at+1)=(uint8_t)y; B(at+2)=(uint8_t)colour;
    /* byte3 is padding, retained exactly as the original does. */
    P(BT_COMMAND_END,at+4); return 0;
}
static unsigned ring_step(unsigned p,unsigned base,int delta)
{
    unsigned r=(uint16_t)(p+delta);
    if(delta>=0 && r>=base+1024u) r-=1024u;
    else if(delta<0 && r<base) r+=1024u;
    return r;
}
static int project_far(BloodTerrain *t,unsigned base,unsigned center,unsigned scale,
                        unsigned count,unsigned bias,int stationary)
{
    unsigned direction;
    scale&=4095u;
    for(direction=0;direction<2;direction++) {
        unsigned i,p=base+center,fraction=0,x=128;
        unsigned horizon=direction?(stationary?0x2480:0x247f):0x2480;
        unsigned n=count+(direction && stationary?1u:0u);
        for(i=0;i<n;i++) {
            unsigned raw=B(p),sum=B(BT_HEIGHT_LUT+raw)+bias,old=fraction>>8,step;
            if(sum>255u) B(horizon)=255;
            else if(sum>B(horizon)) {
                B(horizon)=(uint8_t)sum;
                if(append_point(t,x,sum&127u,B(BT_COLOURS+raw))) return -1;
            }
            fraction=(uint16_t)(fraction+scale);
            step=(uint8_t)((fraction>>8)-old);
            p=ring_step(p,base,direction?-(int)step:(int)step);
            horizon=(uint16_t)(horizon+(direction?-1:1));
            x=(uint8_t)(x+(direction?-1:1));
        }
    }
    return 0;
}
static int project_near_side(BloodTerrain *t,unsigned base,unsigned center,
                             unsigned scale,unsigned bias,int direction)
{
    unsigned p=base+center,horizon=0x2480,x=128,previous,steps=0;
    P(0x20fe,0);P(0x20fc,scale);P(0x20fa,direction<0?base:base+1023);
    previous=projected_height(t,B(p),bias);
    p=ring_step(p,base,direction);
    for(;;) {
        unsigned acc=W(0x20fe)+scale,run,next,raw,j,overflow;
        int slope;
        uint16_t vertical;
        if(++steps>2048u) return -1; /* malformed scale guard */
        run=(uint8_t)(acc>>8);P(0x20fe,acc&255u);
        raw=B(p);p=ring_step(p,base,direction);
        next=projected_height(t,raw,bias);
        overflow=B(BT_HEIGHT_LUT+raw)+bias>255u;
        if(!run) return -1; /* originals use DIV here: zero run is invalid */
        slope=((int)next-(int)previous)*256/(int)run;
        vertical=(uint16_t)(previous<<8);
        for(j=0;j<run;j++) {
            unsigned value=vertical>>8;
            if(previous==255u && overflow) B(horizon)=255;
            else if(value>B(horizon)) {
                B(horizon)=(uint8_t)value;
                if(append_point(t,x,value&127u,slope?9u:B(BT_COLOURS+raw))) return -1;
            }
            /* The original tests the wrapping byte X before stepping its
             * horizon pointer/fixed-point accumulator. Left includes x0. */
            x=(uint8_t)(x+direction);
            if((direction>0 && x==0) || (direction<0 && (x&128u))) return 0;
            horizon=(uint16_t)(horizon+direction);
            vertical=(uint16_t)(vertical+slope);
        }
        /* CH is the post-run accumulator, not the exact next sample. DIV
         * truncation leaves subpixel residue, which must survive this edge. */
        previous=vertical>>8;
    }
}
static int project_layer(BloodTerrain *t,unsigned base,unsigned center,
                         unsigned depth,unsigned bias,int stationary)
{
    unsigned scale;
    if(base+1024u>65536u || center>=1024u || depth>254u || (depth&1u)) return -1;
    height_lookup(t,depth); scale=W(BT_HSCALE+depth);
    if(depth>=128u) {
        unsigned count=stationary?128u:(((scale>>8)&0xf0u)>>1)+1u;
        return project_far(t,base,center,scale,count,bias,stationary);
    }
    if(project_near_side(t,base,center,scale,bias,1)) return -1;
    return project_near_side(t,base,center,scale,bias,-1);
}
int blood_terrain_project(BloodTerrain *t)
{
    unsigned layer,depth;
    if(!valid(t) || B(BT_CAMERA)>127 || B(BT_PHASE)>15) return -1;
    memset(t->mem+BT_HORIZON,127,256);P(BT_COMMAND_END,0);
    B(BT_LAYER)=0;depth=B(BT_PHASE)*2u;P(BT_DEPTH,depth);
    for(layer=0;layer<8;layer++) {
        if(project_layer(t,W(BT_STRIPS+layer*2),W(BT_CENTERS+layer*2),depth,t->projection_bias,0)) return -1;
        depth+=32;P(BT_DEPTH,depth);B(BT_LAYER)=(uint8_t)(layer*2u+2u);
    }
    P(W(BT_COMMAND_END),65535); return 0;
}
int blood_terrain_project_stationary(BloodTerrain *t)
{
    unsigned depth;
    if(!valid(t)||B(BT_CAMERA)>127||B(BT_PHASE)>15||B(BT_LAYER)>14) return -1;
    P(BT_COMMAND_END,0);depth=B(BT_PHASE)*2u+B(BT_LAYER)*16u;P(BT_DEPTH,depth);
    if(project_layer(t,BT_RECIPES,W(BT_CENTER),depth,t->stationary_bias,1)) return -1;
    P(W(BT_COMMAND_END),65535);return 0;
}
int blood_terrain_clear(uint8_t *page,size_t bytes)
{
    unsigned plane,y;
    if(!page||bytes<BLOOD_TERRAIN_EGA_BYTES) return -1;
    for(plane=0;plane<4;plane++) for(y=20;y<146;y++)
        memset(page+plane*8000+y*40+4,0,32);
    return 0;
}
int blood_terrain_draw(const BloodTerrain *t,uint8_t *page,size_t bytes)
{
    unsigned i,plane;
    if(!valid(t)||!page||bytes<BLOOD_TERRAIN_EGA_BYTES) return -1;
    for(i=0;i+3<0x2000;i+=4) {
        unsigned xy=W(i),x,y,row,mask,colour;
        if(xy&32768u) return 0;
        x=xy&255u;y=xy>>8;
        row=W(BT_SCREEN_ROWS+((y*2u)&255u));
        if(!row) continue;
        row+=x/8u;mask=B(0x2078+x%8u);colour=B(i+2);
        if(row>=8000u) return -1;
        if(B(BT_HALF)) mask|=mask>>1;
        for(plane=0;plane<4;plane++) {
            unsigned at=plane*8000u+row;
            page[at]=(uint8_t)((page[at]&~mask)|((colour&(1u<<plane))?mask:0u));
        }
    }
    return -1;
}
int blood_terrain_shortcut(BloodTerrain *t,const BloodTerrainTables *a,uint8_t x,uint8_t y)
{
    unsigned i;
    if(blood_terrain_init(t,a,x,y)||blood_terrain_prepare(t,a)) return -1;
    P(BT_SECTION,20);blood_terrain_height(t,(uint8_t)(B(BT_FLOOR)+10u));
    P(BT_CENTER,W(BT_SPAN)/2u);
    for(i=0;i<8;i++) P(BT_CENTERS+2*i,W(BT_CENTER));
    P(BT_SPARE,BT_FIRST_STRIP);
    for(i=0;i<9;i++) {
        if(blood_terrain_generate_strip(t,a)||blood_terrain_interpolate_strip(t)) return -1;
        if(i<8) P(BT_SPARE,W(BT_SPARE)+1024u);
    }
    B(BT_INTERP_DELAY)=255;return 0;
}
int blood_terrain_finish(BloodTerrain *t,const BloodTerrainTables *a,uint8_t *page,size_t bytes)
{
    unsigned i;
    if(!valid(t)||!a||!page||bytes<BLOOD_TERRAIN_EGA_BYTES) return -1;
    t->sound_event=1;
    if(B(BT_HALF)) {
        /* 74C1: backward expansion deliberately leaves bytes0 and1 alone. */
        for(i=0;i<8;i++) {
            unsigned base=W(BT_STRIPS+2*i),j=511,dst=1023,left;
            if(base+1024>65536u) return -1;
            left=B(base+j--);
            do {
                unsigned right;
                B(base+dst--)= (uint8_t)left;
                right=B(base+j--);B(base+dst--)=average(left,right);left=right;
            } while(dst>1u);
        }
        P(BT_CENTER,W(BT_CENTER)*2u);
        for(i=0;i<8;i++) P(BT_CENTERS+2*i,W(BT_CENTERS+2*i)*2u);
        B(BT_HALF)=0;P(BT_SPAN,W(BT_SPAN)*2u);
    }
    if(B(BT_CAMERA)<(uint8_t)(B(BT_FLOOR)+10u)) blood_terrain_height(t,(uint8_t)(B(BT_FLOOR)+10u));
    B(BT_PHASE)=15;P(BT_DISTANCE,4095);B(BT_SPEED)=0;
    if(blood_terrain_advance(t,a)!=0) return -1;
    if(blood_terrain_clear(page,bytes)||blood_terrain_project(t)||blood_terrain_draw(t,page,bytes)) return -1;
    return 0;
}
int blood_terrain_landscape(BloodTerrain *t,uint8_t *page,size_t bytes,
                             BloodLandscapeFrame frame,void *opaque)
{
    uint8_t slices[16384];
    unsigned layer,phase,j,start;
    if(!valid(t)||!page||bytes<BLOOD_TERRAIN_EGA_BYTES||B(BT_HALF)) return -1;
    memset(t->mem+BT_HORIZON,127,256);
    t->stationary_bias=(uint8_t)((B(BT_CAMERA)>>1)+32u);
    B(BT_LAYER)=0;
    for(layer=0;layer<8;layer++) {
        start=W(BT_STRIPS+2*layer); if(start+1024u>65536u) return -1;
        memcpy(slices+15360,t->mem+start,1024);
        /* Sixteen depth slices: each nearer synthesized strip is six lower,
         * but terrain at/below the floor is not raised by this procedure. */
        for(j=15360;j>0;j-=1024) {
            unsigned k;
            for(k=0;k<1024;k++) {
                unsigned h=slices[j+k],floor=B(BT_FLOOR);
                if(h>floor) { h=(uint8_t)(h-6u);if(h<floor)h=floor; }
                slices[j-1024+k]=(uint8_t)h;
            }
        }
        B(BT_PHASE)=0;
        for(phase=0;phase<16;phase++) {
            memcpy(t->mem+BT_RECIPES,slices+phase*1024,1024);
            if(layer<=2) {
                t->stationary_bias=(uint8_t)((B(BT_CAMERA)>>1)+31u);
                if(blood_terrain_project_stationary(t)) return -1;
                /* The source keeps the updated horizon, discards this point
                 * list and projects again one pixel higher before drawing. */
            }
            t->stationary_bias=(uint8_t)((B(BT_CAMERA)>>1)+32u);
            if(blood_terrain_project_stationary(t)||blood_terrain_draw(t,page,bytes)) return -1;
            if(frame) frame(opaque,page,layer,phase);
            B(BT_PHASE)++;
        }
        B(BT_LAYER)=(uint8_t)(layer*2u+2u);
    }
    return 0;
}

/* The one-bit HUD glyph path (79AD) sets red+intensity planes and clears the
 * green+blue planes ONLY at set glyph bits: EGA index12, retaining background.
 * Coordinates are the original local unsigned WORD operands, including the
 * deliberate wrap of small negative x positions. They are not PM coordinates. */
static int hud_glyph(const BloodTerrainTables *a,uint8_t *page,
                      unsigned vertical,unsigned horizontal,unsigned glyph,unsigned rows)
{
    unsigned y=(uint16_t)(146u-vertical),x=(uint16_t)(32u+horizontal),j,k;
    if(glyph+rows>38u || y>=200u || y+rows>200u || x+8u>320u) return -1;
    for(j=0;j<rows;j++) for(k=0;k<8;k++) if(a->glyphs[glyph+j]&(128u>>k)) {
        unsigned at=(y+j)*40u+(x+k)/8u,mask=128u>>((x+k)%8u),plane;
        for(plane=0;plane<4;plane++) {
            unsigned p=plane*8000u+at;
            page[p]=(uint8_t)((page[p]&~mask)|((12u&(1u<<plane))?mask:0u));
        }
    }
    return 0;
}
int blood_terrain_hud(BloodTerrain *t,const BloodTerrainTables *a,
                       uint8_t hostile,uint8_t *page,size_t bytes)
{
    unsigned h,cx,cy,i,where,low,high,delta;
    BloodAttack attack;BloodAttackSound sound;int result;
    if(!valid(t)||!a||!page||bytes<BLOOD_TERRAIN_EGA_BYTES) return -1;
    h=B(BT_HEIGHT_INDICATOR);
    if(h>63)h=(uint8_t)(h-2);else if(h<60)h=(uint8_t)(h+2);
    B(BT_HEIGHT_INDICATOR)=(uint8_t)h;
    cx=B(BT_STEER_INDICATOR);
    if(cx>31)cx--;else if(cx<31)cx++;
    B(BT_STEER_INDICATOR)=(uint8_t)cx;
    cx=(uint8_t)(cx*4u);cy=(uint8_t)(h+3u);
    if(hud_glyph(a,page,cy,cx,16,8))return -1;
    B(BT_INDICATOR_DIV)--;
    if(!B(BT_INDICATOR_DIV)){B(BT_INDICATOR_DIV)=2;B(BT_INDICATOR_PHASE)^=1;}
    if(B(BT_INDICATOR_PHASE)&1u) {
        int glyph=-1;
        if(W(BT_PROGRESS)>8)glyph=24;
        else {
            /* Course prompts consume the LOCAL terrain RNG. Even omitted
             * cosmetic drawing must not omit these state changes. */
            if((uint16_t)(20-W(BT_SECTION))<=4u)P(BT_COURSE,512);
            else {
                if(W(BT_SPEED)<=2)goto course_done;
                if((local_random(t)&31u)>=29u) {
                    delta=(uint16_t)((local_random(t)&32u)-16u);
                    where=(uint16_t)(W(BT_COURSE)+delta);
                    if(where>=256u && where<=768u)P(BT_COURSE,where);
                }
            }
            low=(uint16_t)(W(BT_COURSE)-16u);high=(uint16_t)(W(BT_COURSE)+16u);
            if(B(BT_HALF)){low>>=1;high>>=1;}
            if(W(BT_CENTER)<low){cx=(uint8_t)(cx+12u);glyph=0;}
            else if(W(BT_CENTER)>high){cx=(uint8_t)(cx-8u);glyph=8;}
            else glyph=24;
        }
        if(glyph>=0 && hud_glyph(a,page,cy,cx,(unsigned)glyph,8))return -1;
    }
course_done:
    where=W(BT_STRIPS)+W(BT_CENTERS);
    if(where>=65536u)return -1;
    attack.delay=B(BT_ATTACK_DELAY);attack.phase=B(BT_ATTACK_PHASE);
    attack.intensity=B(BT_ATTACK_INTENSITY);attack.sounding=B(BT_ATTACK_SOUND);
    result=blood_attack_update(&attack,hostile,B(BT_CAMERA),B(where),&sound);
    B(BT_ATTACK_DELAY)=attack.delay;B(BT_ATTACK_PHASE)=attack.phase;
    B(BT_ATTACK_INTENSITY)=attack.intensity;B(BT_ATTACK_SOUND)=attack.sounding;
    t->sound_event=(uint8_t)(sound==BLOOD_ATTACK_SOUND_STOP?1:
                            sound==BLOOD_ATTACK_SOUND_PLAY_2?3:0);
    if(result==BLOOD_ATTACK_FLIGHT_FAILURE){t->failed=1;return 2;}
    if(result==BLOOD_ATTACK_DRAW){
        unsigned left=(uint8_t)(attack.intensity*4u),right;
        if(hud_glyph(a,page,B(BT_HEIGHT_INDICATOR),left,0,8))return -1;
        right=(uint8_t)(-left);
        if(hud_glyph(a,page,B(BT_HEIGHT_INDICATOR),right,8,8))return -1;
        cy=(uint8_t)(B(BT_HEIGHT_INDICATOR)-3u);cx=right;
        do {if(hud_glyph(a,page,cy,cx,34,2))return -1;cx=(uint8_t)(cx+8u);}while(cx&128u);
        cx=(uint8_t)(-right);
        do {if(hud_glyph(a,page,cy,cx,32,2))return -1;cx=(uint8_t)(cx-8u);}while(!(cx&128u));
    }
    h=B(BT_CAMERA)>=B(BT_FLOOR)?B(BT_CAMERA)-B(BT_FLOOR):0;
    h=(uint8_t)((h&254u)+2u);
    if(hud_glyph(a,page,h,0,36,2)||hud_glyph(a,page,h,252,36,2))return -1;
    for(i=0;i<3u*(W(BT_SPEED)&255u);i++)
        if(hud_glyph(a,page,6,16u+4u*i,34,2))return -1;
    return 0;
}
int blood_terrain_step(BloodTerrain *t,const BloodTerrainTables *a,
                        const BloodFlightInput *input,uint8_t hostile,
                        uint8_t *page,size_t bytes,unsigned *sounds)
{
    int movement,result;
    if(!sounds || !page || bytes<BLOOD_TERRAIN_EGA_BYTES)return -1;
    *sounds=0;
    if(blood_terrain_control(t,input))return -1;
    movement=blood_terrain_advance(t,a);if(movement<0)return movement;
    if(t->sound_event)*sounds|=1u<<(t->sound_event-1u);
    /* Both original branches clear the viewport, including a collision that
     * returns before projection and the fatal-collision transition. */
    if(blood_terrain_clear(page,bytes))return -1;
    if(movement==2)return 2;
    if(!movement && blood_terrain_project(t))return -1;
    if(blood_terrain_draw(t,page,bytes))return -1;
    result=blood_terrain_hud(t,a,hostile,page,bytes);
    if(t->sound_event)*sounds|=1u<<(t->sound_event-1u);
    if(result)return result;
    return movement;
}
int blood_terrain_landing_begin(BloodTerrain *t)
{
    if(!valid(t))return -1;
    P(BT_SPEED,6);B(BT_NO_COLLISION)=1;t->sound_event=1;return 0;
}
int blood_terrain_landing_step(BloodTerrain *t,const BloodTerrainTables *a,
                                const BloodFlightInput *input,uint8_t *page,size_t bytes)
{
    unsigned speed;int rc;
    if(!valid(t)||!input||!a||!page||bytes<BLOOD_TERRAIN_EGA_BYTES)return -1;
    speed=W(BT_SPEED);
    if(blood_terrain_control(t,input))return -1;
    P(BT_SPEED,speed); /* original pushes speed around control call */
    rc=blood_terrain_advance(t,a);if(rc)return -1; /* begin inhibits collisions */
    if(blood_terrain_clear(page,bytes)||blood_terrain_project(t)||blood_terrain_draw(t,page,bytes))return -1;
    return B(BT_PHASE)==15;
}
