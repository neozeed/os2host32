#include "blood_effects.h"
#include <string.h>
static int high_product(uint16_t a,unsigned b){
 int32_t aa=a<32768?(int32_t)a:(int32_t)a-65536;
 int32_t bb=(b&65535)<32768?(int32_t)(b&65535):(int32_t)(b&65535)-65536;
 int32_t v=aa*bb;return v>=0?v/65536:-((-v+65535)/65536);
}
static void random_star(BloodStarField *s,BloodMovingStar *p){
 p->x=blood_upcom_random(&s->random);p->y=blood_upcom_random(&s->random);
 p->depth=blood_upcom_random(&s->random)&4095;
}
void blood_starfield_init(BloodStarField *s,uint16_t low,uint16_t high,
 unsigned count,unsigned numerator,int x,int y){
 unsigned i;
 memset(s,0,sizeof(*s));s->random.rng_low=low;s->random.rng_high=high;
 s->count=count>512?512:count;s->numerator=numerator;s->cx=x;s->cy=y;
 for(i=0;i<512;i++){s->stars[i].mask=65535;random_star(s,&s->stars[i]);}
}
void blood_starfield_step(BloodStarField *s,uint8_t p[64000],unsigned speed){
 unsigned i,b,at,mask,scale;
 int x,yy,y;
 BloodMovingStar *v;
 for(i=0;i<s->count;i++){
  v=&s->stars[i];at=v->address;mask=v->mask>>8;
  if(at<8000)for(b=0;b<8;b++)if(mask&(128u>>b))p[(at/40)*320+(at%40)*8+b]=0;
  if(v->depth<speed){random_star(s,v);v->depth|=0x1800;continue;}
  v->depth=(uint16_t)(v->depth-speed);
  scale=((s->numerator<<16)|65535u)/(v->depth+16u);
  x=high_product(v->x,scale)+s->cx;yy=high_product(v->y,scale);
  y=(yy>=0?yy/2:-((-yy+1)/2))+s->cy;
  if(x<0||x>=320||y<=20||y>=146){v->depth=0;continue;}
  v->address=(uint16_t)(y*40+x/8);v->mask=(uint16_t)(((128u>>(x&7))<<8)|8);
  p[y*320+x]=(uint8_t)(v->depth>=0xa00?7:v->depth>=0x400?9:x&7);
 }
}
size_t blood_globe_filter(const uint8_t *src,size_t size,uint16_t mask,uint8_t *out){
 size_t in=0,n=0;uint16_t rows=mask,columns=mask;
 unsigned keep,yes;
 while(in<size&&src[in]!=255){
  keep=rows&1;rows=(uint16_t)((rows>>1)|(rows<<15));
  if(keep){out[n++]=src[in++];
   while(in<size&&src[in]!=255){yes=columns&1;columns=(uint16_t)((columns>>1)|(columns<<15));if(yes)out[n++]=src[in];in++;}
   if(in>=size)return 0;
   out[n++]=src[in++];
  }else{while(in<size&&src[in++]!=255){} }
 }
 if(in>=size)return 0;
 out[n++]=255;return n;
}
static void pixel(uint8_t *p,int x,int y,uint8_t c,int xor_mode){
 if(x<0||x>=320||y<0||y>=200)return;
 if(xor_mode)p[y*320+x]^=c;else p[y*320+x]=c;
}
static void pair(uint8_t *p,int x,int y,uint8_t c,int xor_mode){
 if(x<0||x>=319||y<20||y>=146)return;
 pixel(p,x,y,c,xor_mode);pixel(p,x+1,y,c,xor_mode);
}
void blood_effect_circle(uint8_t p[64000],int cx,int cy,unsigned radius,uint8_t c,int xo){
 int x=(int)radius,y=0;unsigned square=radius*radius,xx=square,yy=0;
 for(;;){
  pair(p,cx+2*x,cy+y,c,xo);pair(p,cx-2*x,cy+y,c,xo);
  pair(p,cx-2*x,cy-y,c,xo);pair(p,cx+2*x,cy-y,c,xo);
  pair(p,cx+2*y,cy+x,c,xo);pair(p,cx-2*y,cy+x,c,xo);
  pair(p,cx-2*y,cy-x,c,xo);pair(p,cx+2*y,cy-x,c,xo);
  if(x<y)break;
  yy+=(unsigned)(2*y+1);y++;
  if(xx+yy>=square){xx-=(unsigned)(2*x-1);x--;}
 }
}
void blood_effect_line(uint8_t p[64000],int x,int y,int ex,int ey,uint8_t c,int xo){
 unsigned dx,dy,acc,n,sum;
 int sy;
 if(x>ex){int t=x;x=ex;ex=t;t=y;y=ey;ey=t;}
 dx=(unsigned)(ex-x)&255u;dy=(unsigned)(ey>=y?ey-y:y-ey)&255u;
 sy=ey>=y?1:-1;acc=0;
 if(dx>dy){for(n=0;n<=dx;n++){
  pixel(p,x,y,c,xo);x++;sum=acc+dy;acc=sum&255;
  if(sum>255||acc>=dx){y+=sy;acc=(acc-dx)&255;}
 }}else{for(n=0;n<=dy;n++){
  pixel(p,x,y,c,xo);y+=sy;sum=acc+dx;acc=sum&255;
  if(sum>255||acc>=dy){x++;acc=(acc-dy)&255;}
 }}
}
void blood_static_init(uint8_t src[65536],BloodUpcom *r){
 unsigned i;uint16_t v;
 for(i=0;i<0x2100;i+=2){v=blood_upcom_random(r);src[i]=(uint8_t)v;src[i+1]=(uint8_t)(v>>8);}
}
void blood_static_draw(uint8_t p[64000],const uint8_t *src,unsigned off){
 unsigned y,x,at,mask,c,plane;
 off&=255;
 for(y=0;y<126;y++)for(x=0;x<256;x++){
  at=off+y*32+x/8;mask=128u>>(x&7);c=0;
  for(plane=0;plane<4;plane++)if(src[at+plane*4032]&mask)c|=1u<<plane;
  p[(20+y)*320+x+32]=(uint8_t)c;
 }
}
