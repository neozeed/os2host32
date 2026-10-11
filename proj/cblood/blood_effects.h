#ifndef BLOOD_EFFECTS_H
#define BLOOD_EFFECTS_H
#include "blood_globe.h"
#include "blood_upcom.h"
/* Original cache records from 29E3, including previous EGA address/mask. */
typedef struct BloodMovingStar {uint16_t address,mask,x,y,depth;} BloodMovingStar;
typedef struct BloodStarField {
 BloodMovingStar stars[512]; BloodUpcom random;
 unsigned count,numerator;int cx,cy;
} BloodStarField;
void blood_starfield_init(BloodStarField *s,uint16_t low,uint16_t high,
                         unsigned count,unsigned numerator,int x,int y);
void blood_starfield_step(BloodStarField *s,uint8_t pixels[64000],unsigned speed);
size_t blood_globe_filter(const uint8_t *src,size_t size,uint16_t mask,uint8_t *out);
/* 27A3/285E: horizontally doubled circle, two-pixel stores, viewport clip. */
void blood_effect_circle(uint8_t pixels[64000],int x,int y,unsigned radius,
                         uint8_t colour,int xor_mode);
/* 23FC: original byte-accumulator line raster (including its 8-bit deltas). */
void blood_effect_line(uint8_t pixels[64000],int x,int y,int endx,int endy,
                       uint8_t colour,int xor_mode);
/* 754C/55A2: same random source and four-plane window used by failed flight. */
void blood_static_init(uint8_t source[65536],BloodUpcom *random);
void blood_static_draw(uint8_t pixels[64000],const uint8_t *source,unsigned offset);
#endif
