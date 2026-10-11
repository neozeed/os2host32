#ifndef BLOOD_WORLD_H
#define BLOOD_WORLD_H
#include "blood_actions.h"
/* 3C0B..3C8D only: caller supplies RNG state AFTER machine-dependent startup
 * seeding. Preserves 64 unique odd-X/7-bit-Y pairs, four starting families,
 * flags 40..49, ending flag999 reset, history cursors and cached-bank reset.
 * No terrain, DOS, UI, or implicit wall clock is involved. */
int blood_generate_galaxy(BloodGame *s, uint16_t *rng_low, uint16_t *rng_high,
                          uint16_t *cached_bank);
#endif
