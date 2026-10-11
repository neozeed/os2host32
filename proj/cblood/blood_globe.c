#include "blood_globe.h"
#include "blood_upcom.h"
#include <string.h>
void blood_stars_init(BloodStar stars[512],uint16_t seed)
{
    unsigned i;
    BloodUpcom random;memset(&random,0,sizeof(random));random.rng_low=random.rng_high=seed;
    for(i=0;i<512;i++){
        stars[i].x=blood_upcom_random(&random);
        stars[i].y=blood_upcom_random(&random);
        stars[i].depth=blood_upcom_random(&random)&0xfff;
    }
}
static int signed_high(uint16_t value,unsigned scale)
{
    int32_t signed_value=value<32768u?(int32_t)value:(int32_t)value-65536;
    int32_t signed_scale=scale<32768u?(int32_t)scale:(int32_t)scale-65536;
    int32_t product=signed_value*signed_scale;
    return product>=0?product/65536:-((-product+65535)/65536);
}
void blood_stars_draw(uint8_t p[64000],const BloodStar stars[512])
{
    unsigned i,z,scale;
    int x,yy,y;
    /* B2 creates 512 records but its orbital view projects only 511. */
    for(i=0;i<511;i++){
        /* 29E3 patches DX's immediate at 295C to 12: DX:AX=000C:FFFF. */
        z=stars[i].depth;scale=0xcffffu/(z+16u);
        x=signed_high(stars[i].x,scale)+160;
        yy=signed_high(stars[i].y,scale);
        y=(yy>=0?yy/2:-((-yy+1)/2))+83;
        if(x<0||x>=320||y<=20||y>=146)continue;
        /* The near-star path retains BL's bit index in the original. */
        p[y*320+x]=(uint8_t)(z>=0xa00?7:z>=0x400?9:x&7);
    }
}
int blood_globe_quadrant(const uint8_t texture[16384],const uint8_t *stream,
                         size_t size,uint8_t longitude,unsigned q,uint8_t cache[8192])
{
    size_t pos=0;int bp;unsigned right=q>=2,lower=q&1;
    unsigned latitude,skip,pixel,plane;
    uint8_t planes[4],colour,column;
    int di;
    if(!texture||!stream||!cache||q>=4)return -1;
    bp=lower?(right?0x3f8:0x3f7):(right?0x408:0x407);
    while(pos<size&&stream[pos]!=255){
        latitude=stream[pos++];
        bp+=lower?16:-16;di=bp;
        if(latitude>31)return -1;
        latitude=lower?32+latitude:31-latitude;
        /* The positive-longitude passes skip two central samples. */
        if(right)for(skip=0;skip<2;skip++){
            if(pos>=size)return -1;
            if(stream[pos++]==255)goto next_row;
        }
        while(pos<size){
            planes[0]=planes[1]=planes[2]=planes[3]=0;colour=0;
            if(stream[pos]==255){pos++;break;}
            for(pixel=0;pixel<8;pixel++){
                if(pos>=size)return -1;
                if(stream[pos]!=255){
                    column=(uint8_t)(longitude+(right?stream[pos]:-stream[pos]));
                    colour=texture[latitude*256+column];pos++;
                }
                /* Original unrolled SHR/RCR (left) or SHR/RCL (right).
                 * On padding the residual colour is shifted four more bits. */
                for(plane=0;plane<4;plane++){
                    planes[plane]|=(uint8_t)((colour&1u)<<(right?7-pixel:pixel));
                    colour>>=1;
                }
            }
            if(di<0||di>=2048)return -1;
            for(plane=0;plane<4;plane++)cache[plane*2048+(unsigned)di]=planes[plane];
            di+=right?1:-1;
        }
next_row:;
    }
    return pos<size?0:-1;
}
int blood_globe_draw(uint8_t p[64000],const uint8_t texture[16384],const uint8_t *stream,
                     size_t size,uint8_t longitude)
{
    uint8_t cache[8192]={0};
    unsigned y,x,c,off,bit,plane,q,colour;
    if(!p)return -1;
    /* 5AE2 retains the star background in the cache's untouched corners. */
    for(y=0;y<96;y++)for(x=0;x<128;x++){
        c=p[(y+36)*320+x+64];off=256+y*16+x/8;bit=128u>>(x&7);
        for(plane=0;plane<4;plane++)if(c&(1u<<plane))cache[plane*2048+off]|=(uint8_t)bit;
    }
    for(q=0;q<4;q++)if(blood_globe_quadrant(texture,stream,size,longitude,q,cache))return -1;
    /* B2 file 5E37 copies cache+100, 16 bytes x 96 rows, to EGA+5A8. */
    for(y=0;y<96;y++)for(x=0;x<128;x++){
        off=256+y*16+x/8;bit=128u>>(x&7);colour=0;
        for(plane=0;plane<4;plane++)if(cache[plane*2048+off]&bit)colour|=1u<<plane;
        p[(y+36)*320+x+64]=(uint8_t)colour;
    }
    return 0;
}
void blood_oorx_draw(uint8_t p[64000],const uint8_t background[64000],
                     const uint8_t sheets[5][4000],unsigned height)
{
    unsigned y,x,bit,off,aperture,colour,plane,dest;
    if(height>68)height=68;
    for(y=0;y<68;y++)memcpy(p+(53+y)*320+208,background+(53+y)*320+208,32);
    for(y=0;y<height;y++)for(x=0;x<32;x++){
        bit=128u>>(x&7);off=(186-height+y)*4+x/8;
        aperture=(119+y)*4+x/8;colour=0;
        if(!(sheets[4][aperture]&bit))continue;
        for(plane=0;plane<4;plane++)if(sheets[2][(plane+1)*800+off]&bit)colour|=1u<<plane;
        dest=(53+y)*320+208+x;
        p[dest]=(uint8_t)((p[dest]&(sheets[2][off]&bit?15u:0u))|colour);
    }
}
