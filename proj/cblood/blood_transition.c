#include "blood_transition.h"
#include <string.h>
static void clear_edge(uint8_t *p,unsigned y){
 if(y>0&&y<199)memset(p+(y-1)*320,0,3*320);
}
static void jagged(uint8_t *p,unsigned y,BloodUpcom *random){
 unsigned position=1,x,direction;clear_edge(p,y);
 for(x=0;x<320;x++){
  direction=(blood_upcom_random(random)>>8)&3u;
  if(direction==1){if(position==2){y--;position--;}else{y++;position++;}}
  else if(direction==2){if(position){y--;position--;}else{y++;position++;}}
  if(y<200)p[y*320+x]=15;
 }
}
static void pulse(uint8_t *p,unsigned radius){
 unsigned i;
 for(i=0;i<3;i++)blood_effect_circle(p,160,82,radius+2*i,15,1);
}
void blood_transition_step(uint8_t p[64000],unsigned step,unsigned bottom,BloodUpcom *random){
 unsigned x;
 if(step<5){if(step)pulse(p,10+20*(step-1));pulse(p,10+20*step);}
 else if(step<9){if(step==5)pulse(p,90);jagged(p,18,random);jagged(p,bottom,random);}
 else if(step==9){
  clear_edge(p,18);clear_edge(p,bottom);
  for(x=0;x<320;x++){
   p[18*320+x]=p[bottom*320+x]=7;
   p[17*320+x]=p[19*320+x]=p[(bottom-1)*320+x]=p[(bottom+1)*320+x]=1;
  }
 }
}
void blood_title_exchange(uint8_t front[64000],uint8_t back[64000],unsigned *cursor,unsigned count){
 unsigned at=*cursor,i;
 uint8_t temporary;
 for(i=0;i<count;i++){
  temporary=front[at];front[at]=back[at];back[at]=temporary;
  at+=149;if(at>=64000)at-=64000;
 }
 *cursor=at;
}
static unsigned packed_byte(const uint8_t *p,unsigned x,unsigned y,unsigned plane){
 unsigned v=0,b;for(b=0;b<8;b++)if(p[y*320+x+b]&(1u<<plane))v|=128u>>b;return v;
}
static void unpack_byte(uint8_t *p,unsigned x,unsigned y,unsigned plane,unsigned value){
 unsigned b;
 uint8_t *pixel;
 for(b=0;b<8;b++){
  pixel=&p[y*320+x+b];*pixel=(uint8_t)((*pixel&~(1u<<plane))|((value&(128u>>b))?(1u<<plane):0));
 }
}
void blood_portrait_dissolve(uint8_t p[64000],const uint8_t target[64000],BloodUpcom *random,unsigned trials){
 unsigned i,y,x,different,plane,v;
 for(i=0;i<trials;i++){
  y=44+((blood_upcom_random(random)>>8)&63u);
  x=8*(6+((blood_upcom_random(random)>>8)&7u));different=0;
  for(plane=0;plane<4;plane++)different|=packed_byte(p,x,y,plane)^packed_byte(target,x,y,plane);
  if(!different)continue;
  if((blood_upcom_random(random)&255u)<=16){memcpy(p+y*320+x,target+y*320+x,8);}
  else{
   v=blood_upcom_random(random);unpack_byte(p,x,y,0,v>>8);unpack_byte(p,x,y,1,v&255);
   v=blood_upcom_random(random);unpack_byte(p,x,y,2,v>>8);unpack_byte(p,x,y,3,v&255);
  }
 }
}
void blood_portrait_finish(uint8_t p[64000],const uint8_t target[64000]){
 unsigned y;
 for(y=40;y<110;y++)memcpy(p+y*320+40,target+y*320+40,80);
}
