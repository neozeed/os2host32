#ifndef BLOOD_TRANSITION_H
#define BLOOD_TRANSITION_H
#include "blood_effects.h"
/* Native translations; offsets refer to the verified B2 executable file.
 * 28A7/2769: five XOR pulses and four random edge flashes, then 2609 frame.
 * Steps 0..8 are visible service intervals; step 9 restores the frame. */
void blood_transition_step(uint8_t pixels[64000],unsigned step,unsigned bottom,
                           BloodUpcom *random);
/* 4AB4: exchange both pages in CX += 149 (mod 64000) order. */
void blood_title_exchange(uint8_t front[64000],uint8_t back[64000],
                          unsigned *cursor,unsigned count);
/* 6CE1: 100 x 200 random byte trials followed by the 539A final rectangle. */
void blood_portrait_dissolve(uint8_t pixels[64000],const uint8_t target[64000],
                            BloodUpcom *random,unsigned trials);
void blood_portrait_finish(uint8_t pixels[64000],const uint8_t target[64000]);
#endif
