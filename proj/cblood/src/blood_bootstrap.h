#ifndef BLOOD_BOOTSTRAP_H
#define BLOOD_BOOTSTRAP_H
#include <stddef.h>
#include <stdint.h>
/* R17: exact DOS engine seeding, not a portable source of host entropy.
 * Preserve this for reproduction; a native port may explicitly supply a seed.
 * The 8086 reads 32,768 little-endian words from physical 00000..0FFFF.
 * On return seed_low is the RELOCATED data segment, not the low checksum.
 */
int blood_bootstrap_seed(const uint8_t *low_memory, size_t length,
                         uint16_t relocated_ds, uint16_t *seed_low,
                         uint16_t *seed_high, uint32_t *sum_out);
/* Read-only forensic search. Given a known DOS data segment and an unmodified
 * initial galaxy, enumerate the 32,768 possible checksum carry totals. Only
 * wanted coordinate pairs are compared; character moves do not break matches.
 * A destroyed or script-edited wanted coordinate can prevent a match.
 * Returns the number of exact matches, or -1 for an invalid input. Writes at
 * most capacity matching high words; it never edits the original capture.
 */
int blood_bootstrap_find_seeds(uint16_t relocated_ds,
                               const uint8_t wanted_xy[128],
                               uint16_t *matches, size_t capacity);
#endif
