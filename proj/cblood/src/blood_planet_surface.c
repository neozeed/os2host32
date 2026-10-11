/* Captain Blood DOS/EGA B2, R18 original-code reconstruction.
 * All addresses below are offsets in the exact R4 recovered MZ executable.
 *
 * There are two different landscape systems: this file recovers the orbital
 * planet texture (5B36..5D32); it does not generate the flight height strips
 * at7A8F or project the canyon. That distinction prevents a misleading port.
 */
#include "blood_planet_surface.h"
#include <string.h>
#include "blood_upcom.h"

static uint16_t next_random(uint16_t *rng)
{
    *rng = blood_planet_random16(*rng); /* R17's exhaustively verified ADC. */
    return *rng;
}

int blood_planet_surface_plan(uint8_t x, uint8_t y, BloodPlanetSurface *s)
{
    static const uint8_t spacing[3] = {8, 16, 32};
    uint16_t value;
    unsigned index;
    if (!s) return -1;
    memset(s, 0, sizeof(*s));
    s->rng = blood_planet_coordinate_seed(x, y);
    s->palette = (uint8_t)((next_random(&s->rng) >> 8) & 7u);
    s->rotation_step = (uint8_t)(((next_random(&s->rng) >> 8) & 3u) + 1u);
    value = next_random(&s->rng);
    if (value & 0x0800u) s->rotation_step = (uint8_t)(0u - s->rotation_step);
    /* The code patches the horizontal calls FIRST, then the vertical ones.
     * The grid is refined vertically first. Swapping these RNG draws changes
     * the planet even though the interpolation algorithm still looks right. */
    s->horizontal_modifier = (uint8_t)((next_random(&s->rng) & 14u) >> 1);
    s->vertical_modifier = (uint8_t)((next_random(&s->rng) & 14u) >> 1);
    s->amplitude_mask = 127;
    do { index = (next_random(&s->rng) & 6u) >> 1; } while (index == 3u);
    s->initial_spacing = spacing[index];
    s->stage = BLOOD_PLANET_SURFACE_PLANNED;
    return 0;
}

/* C division truncates negative values toward zero. The original SAR instead
 * rounds downward. Spell out the negative case; do not depend on a signed
 * right shift or on converting an out-of-range uint8_t into int8_t. */
static int arithmetic_shift_byte(uint8_t byte, unsigned bits)
{
    unsigned divisor = 1u << bits;
    if (byte < 128u) return (int)(byte / divisor);
    return -(int)(((256u - byte) + divisor - 1u) / divisor);
}

static uint8_t adjust(uint8_t midpoint, unsigned modifier, uint8_t mask,
                      uint16_t *rng)
{
    int delta = 0;
    int value;
    uint8_t random;
    switch (modifier) {
    case 0: /* 5D4D: bare RET, consuming NO random number. */
        break;
    case 1: /* 5D4E: positive masked displacement, clipped at255. */
        delta = (int)((uint8_t)next_random(rng) & mask);
        break;
    case 2: /* 5D5F: signed noise /4. */
    case 3: /* 5D66: signed noise /8. */
    case 4: /* 5D6D: signed noise /32, NOT /16. */
        random = (uint8_t)next_random(rng);
        delta = arithmetic_shift_byte(random, modifier == 4u ? 5u : modifier);
        break;
    case 5: /* 5D8B: OR F0; SAR 1 gives -8..-1. */
        random = (uint8_t)(next_random(rng) | 0xf0u);
        delta = arithmetic_shift_byte(random, 1);
        break;
    case 6: /* 5D96: fixed downward displacement. No RNG draw. */
        delta = -6;
        break;
    case 7: /* 5D9C: erode only values below128. No RNG draw. */
        if (midpoint < 128u) delta = -16;
        break;
    case 8: /* 5DA7: present in table, excluded by selection mask0E. */
        delta = -(int)((uint8_t)next_random(rng) & mask);
        break;
    default:
        break; /* Public entry validates this; no implicit fallback API. */
    }
    value = (int)midpoint + delta;
    if (value < 0) return 0;
    if (value > 255) return 255;
    return (uint8_t)value;
}

int blood_planet_surface_adjust(uint8_t midpoint, unsigned modifier,
                               uint8_t mask, uint16_t *rng, uint8_t *result)
{
    if (!rng || !result || modifier > 8u) return -1;
    *result = adjust(midpoint, modifier, mask, rng);
    return 0;
}

int blood_planet_heightfield(uint8_t x, uint8_t y, uint8_t *out, size_t size,
                            BloodPlanetSurface *s)
{
    unsigned spacing, row, col, other, half;
    uint8_t midpoint;
    if (!out || !s || size < BLOOD_PLANET_TEXTURE_BYTES) return -1;
    if (blood_planet_surface_plan(x, y, s) != 0) return -1;
    spacing = s->initial_spacing;

    /* 5BD7..5BEF. This sparse grid contains 256,64 or16 random anchors.
     * The order is row-major; the later vertical refinements are column-major. */
    for (row = 0; row < 64u; row += spacing)
        for (col = 0; col < 256u; col += spacing)
            out[row * 256u + col] = (uint8_t)next_random(&s->rng);

    while (spacing > 1u) {
        half = spacing / 2u;
        /* 5BF1..5C2C. Insert vertical midpoints in each existing column.
         * ADD AL followed by RCR carries the ninth sum bit into the average:
         * this is (a+b)/2, NOT an overflowing byte sum divided by two. */
        for (col = 0; col < 256u; col += spacing) {
            for (row = 0; row < 64u; row += spacing) {
                other = (row + spacing) & 63u;
                midpoint = (uint8_t)(((unsigned)out[row*256u + col] +
                                     out[other*256u + col]) / 2u);
                out[(row+half)*256u + col] =
                    adjust(midpoint, s->vertical_modifier, s->amplitude_mask, &s->rng);
            }
        }
        /* 5C2D..5C6E. Insert horizontal midpoints in all rows now available,
         * including rows just created above. The last interval wraps to col0. */
        for (row = 0; row < 64u; row += half) {
            for (col = 0; col < 256u; col += spacing) {
                other = (col + spacing) & 255u;
                midpoint = (uint8_t)(((unsigned)out[row*256u + col] +
                                     out[row*256u + other]) / 2u);
                out[row*256u + col+half] =
                    adjust(midpoint, s->horizontal_modifier, s->amplitude_mask, &s->rng);
            }
        }
        spacing = half;
        s->amplitude_mask >>= 1;
    }
    s->stage = BLOOD_PLANET_SURFACE_HEIGHTS;
    return 0;
}

static int palettes_valid(const uint8_t *p, size_t n)
{
    size_t i;
    if (!p || n < BLOOD_PLANET_COLOUR_TABLE_BYTES) return 0;
    for (i = 0; i < BLOOD_PLANET_COLOUR_TABLE_BYTES; ++i)
        if (p[i] > 15u) return 0;
    return 1;
}

int blood_planet_colourize(uint8_t *t, size_t size,
                          const uint8_t *palettes, size_t palette_size,
                          BloodPlanetSurface *s)
{
    uint8_t row_copy[256];
    uint8_t draw;
    unsigned row, col, shift;
    size_t i;
    if (!t || !s || size < BLOOD_PLANET_TEXTURE_BYTES || s->palette >= 8u ||
        s->stage != BLOOD_PLANET_SURFACE_HEIGHTS ||
        !palettes_valid(palettes, palette_size)) return -1;
    s->shear_step = 0;
    s->shear_right = 0;
    /* 5C81: always consume the decision draw, even without a shear. */
    draw = (uint8_t)next_random(&s->rng);
    if (draw < 0x40u) {
        draw = (uint8_t)next_random(&s->rng);
        s->shear_step = (uint8_t)((draw & 3u) + 1u);
        s->shear_right = (uint8_t)((draw >> 7) & 1u);
        /* 5CAC /5CDA. The first row is ALREADY offset by one step.
         * ADD AL wraps every256; shifting by zero is valid on the last row
         * for step4. The historical scratch row at4000 is not an input. */
        for (row = 0; row < 64u; ++row) {
            shift = ((row + 1u) * s->shear_step) & 255u;
            memcpy(row_copy, t + row*256u, sizeof(row_copy));
            for (col = 0; col < 256u; ++col) {
                unsigned source = s->shear_right ? (col + 256u - shift) & 255u
                                                : (col + shift) & 255u;
                t[row*256u + col] = row_copy[source];
            }
        }
    }
    /* 5D11..5D32: 256 raw heights -> eight bands -> one of eight palettes.
     * This is an index lookup, not RGB interpolation. Markers are added later
     * by the separate detail-view path; freshly generated colours are0..15. */
    for (i = 0; i < BLOOD_PLANET_TEXTURE_BYTES; ++i)
        t[i] = palettes[(unsigned)s->palette * 8u + (t[i] >> 5)];
    s->stage = BLOOD_PLANET_SURFACE_COLOURS;
    return 0;
}

int blood_planet_surface_generate(uint8_t x, uint8_t y,
                                 const uint8_t *palettes, size_t palette_size,
                                 uint8_t *out, size_t size,
                                 BloodPlanetSurface *s)
{
    if (!out || !s || size < BLOOD_PLANET_TEXTURE_BYTES ||
        !palettes_valid(palettes, palette_size)) return -1;
    if (blood_planet_heightfield(x, y, out, size, s) != 0) return -1;
    return blood_planet_colourize(out, size, palettes, palette_size, s);
}

int blood_planet_detail_markers(uint8_t x, int hostile, uint8_t *texture,
                               size_t size, uint16_t *lo, uint16_t *hi)
{
    BloodUpcom v;
    unsigned i, row;
    if (!texture || size < BLOOD_PLANET_TEXTURE_BYTES || !lo || !hi) return -1;
    if (!hostile) return 0;
    /* Reuse the R16 verified general RNG, not the appearance recurrence. */
    memset(&v, 0, sizeof(v));
    v.rng_low = x;
    v.rng_high = x;
    for (i = 0; i < 32u; ++i) {
        do { row = (blood_upcom_random(&v) >> 8) & 31u; } while (row >= 28u);
        texture[(row + 16u)*256u + i*8u] |= 0x80u;
    }
    *lo = v.rng_low;
    *hi = v.rng_high;
    return 0;
}
