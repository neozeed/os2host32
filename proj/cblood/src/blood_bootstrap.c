#include "blood_bootstrap.h"
#include "blood_upcom.h"
#include <string.h>
int blood_bootstrap_seed(const uint8_t *m, size_t n, uint16_t ds,
                         uint16_t *lo, uint16_t *hi, uint32_t *sum_out)
{
    uint32_t sum = 0;
    size_t p;
    if (!m || n < 65536u || !lo || !hi) return -1;
    /* ADD AX,[SI]; ADC BX,0 accumulates a full 32-bit sum. The following
     * MOV AX,relocated_DS overwrites its low half before storing the seed.
     * Maximum sum is 32768*65535: uint32_t is sufficient without wrapping. */
    for (p = 0; p < 65536u; p += 2)
        sum += (uint32_t)m[p] | ((uint32_t)m[p+1] << 8);
    *lo = ds;
    *hi = (uint16_t)(sum >> 16);
    if (sum_out) *sum_out = sum;
    return 0;
}
int blood_bootstrap_find_seeds(uint16_t ds, const uint8_t xy[128],
                               uint16_t *matches, size_t capacity)
{
    BloodUpcom v;
    uint16_t generated[64];
    unsigned h, i, j, attempts, x, y;
    int count = 0;
    if (!xy || (capacity && !matches)) return -1;
    for (h = 0; h < 32768u; ++h) {
        memset(&v, 0, sizeof(v));
        v.rng_low = ds;
        v.rng_high = (uint16_t)h;
        attempts = 0;
        for (i = 0; i < 64u;) {
            uint16_t coord;
            if (++attempts > 1000000u) return -1;
            x = ((blood_upcom_random(&v) >> 8) & 0xfeu) + 1u;
            y = (blood_upcom_random(&v) >> 8) & 0x7fu;
            coord = (uint16_t)(x | (y << 8));
            for (j = 0; j < i; ++j)
                if (generated[j] == coord) break;
            if (j < i) continue; /* Original duplicate rejection consumes RNG. */
            if (x != xy[2*i] || y != xy[2*i+1]) break;
            generated[i++] = coord;
        }
        if (i == 64u) {
            if ((size_t)count < capacity) matches[count] = (uint16_t)h;
            ++count;
        }
    }
    return count;
}
