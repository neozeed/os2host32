#ifndef BLOOD_GLOBE_H
#define BLOOD_GLOBE_H
#include <stdint.h>
#include <stddef.h>
#define BLOOD_GLOBE_STREAM_BYTES 2315u
typedef struct BloodStar {uint16_t x,y,depth;} BloodStar;
/* File 29E3/2A3C: the 512 initial three-word star records. Projection uses
 * the signed multiply and depth division from 2925, at centre (160,83). */
void blood_stars_init(BloodStar stars[512],uint16_t seed);
void blood_stars_draw(uint8_t pixels[64000],const BloodStar stars[512]);
/* B2 file 5E79/5FBC/60FF/6242: four quadrant passes into its 128x128
 * planar cache. Stream is the original numerical table at file 1A30. */
int blood_globe_quadrant(const uint8_t texture[16384],const uint8_t *stream,
                         size_t size,uint8_t longitude,unsigned quadrant,
                         uint8_t cache[8192]);
int blood_globe_draw(uint8_t pixels[64000],const uint8_t texture[16384],
                     const uint8_t *stream,size_t size,uint8_t longitude);
/* B2 file 4C03: bottom-aligned Oorx source, clipped by the tube aperture.
 * Restore the original background before every height, including zero. */
void blood_oorx_draw(uint8_t pixels[64000],const uint8_t background[64000],
                     const uint8_t sheets[5][4000],unsigned height);
#endif
