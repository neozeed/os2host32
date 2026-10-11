#include "blood_planet_ui.h"

int blood_console_hit(uint16_t x, uint16_t y, uint8_t buttons,
                       const uint8_t enabled[10])
{
    static const uint8_t xs[10] = {0,48,48,80,112,144,176,208,240,240};
    static const uint8_t ys[10] = {152,120,152,168,176,152,176,168,152,120};
    uint8_t dh, dl;
    int i;
    if (!enabled || !(buttons & 0xc0u)) return -1;
    /* AX+2 wraps as a WORD, DL+2 as a BYTE. Bounds tests are unsigned. */
    dh = (uint8_t)((uint16_t)(x + 2u) >> 1);
    dl = (uint8_t)((uint8_t)y + 2u);
    for (i = 9; i >= 0; --i) {
        unsigned left = xs[i] >> 1;
        if (enabled[i] == 1 && dh >= left && dh < left + 16u &&
            dl >= ys[i] && dl < (unsigned)ys[i] + 24u) return i;
    }
    return 255;
}
void blood_console_settled_icons(uint16_t mask, uint8_t enabled[10])
{
    unsigned i;
    if (!enabled) return;
    for (i = 0; i < 10; ++i) enabled[i] = (uint8_t)((mask >> (9u-i)) & 1u);
}
uint16_t blood_console_after_planet(uint16_t mode, int accessible, int visited)
{
    return (uint16_t)((accessible && visited) ? (mode | 0xe1u) : (mode & 0x1e0u));
}
uint16_t blood_console_action_entry(uint8_t icon)
{
    /* Button names other than '?' are not inferred from geometry. */
    switch (icon) {
    case 1: return 0x3ef8; /* only when a passenger is aboard */
    case 2: return 0x3c8e; /* save/load control */
    case 3: return 0x58c4; /* planetary view */
    case 4: return 0x6e44; /* galaxy map */
    case 9: return 0x73be; /* fast descent, still generates final landscape */
    default: return 0;
    }
}
uint16_t blood_planet_coordinate_seed(uint8_t x, uint8_t y)
{
    return (uint16_t)((uint16_t)x | ((uint16_t)y << 8));
}
uint16_t blood_planet_random16(uint16_t seed)
{
    uint16_t a;
    uint32_t carry_source;
    a = (uint16_t)(seed + seed);
    a = (uint16_t)(a + seed);
    a = (uint16_t)(a + a);
    carry_source = (uint32_t)a + seed;
    a = (uint16_t)carry_source;
    a = (uint16_t)((uint32_t)a * 2u + (carry_source >> 16));
    a = (uint16_t)(a + seed);
    a = (uint16_t)(a * 4u);
    a = (uint16_t)(a + seed);
    a = (uint16_t)(a * 4u);
    a = (uint16_t)(a + seed);
    a = (uint16_t)(a * 4u);
    a = (uint16_t)(a + seed);
    a = (uint16_t)(a * 2u);
    a = (uint16_t)(a + seed);
    a = (uint16_t)(a * 4u);
    a = (uint16_t)(a + seed);
    a = (uint16_t)(a * 4u);
    a = (uint16_t)(a + seed);
    return (uint16_t)(a + 1u);
}
int blood_planet_detail(const uint8_t *t, size_t n, uint8_t *pages,
                        size_t page_n, unsigned zoom, uint8_t longitude)
{
    /* The original is a dithered resampler, NOT nearest-neighbour scaling.
     * Each row below lists eight source offsets consumed by 65B5. */
    static const unsigned normal[2][8] = {
        {1,0,2,1,3,2,4,3}, {0,256,1,257,2,258,3,259}
    };
    static const unsigned doubled[4][8] = {
        {0,0,1,0,1,1,2,1}, {0,257,256,0,1,258,257,1},
        {256,257,0,257,257,2,1,258}, {0,256,257,0,1,257,258,1}
    };
    unsigned column, band, line, bit, plane;
    unsigned rows = zoom ? 4u : 2u;
    unsigned bands = zoom ? 31u : 62u;
    unsigned initial = zoom ? 0x1000u + (uint8_t)(longitude + 40u) : longitude;
    if (!t || !pages || n < BLOOD_PLANET_TEXTURE_BYTES ||
        page_n < BLOOD_EGA_TWO_PAGES_BYTES || zoom > 1u) return -1;
    for (column = 0; column < 40u; ++column) {
        for (band = 0; band < bands; ++band) {
            unsigned at = initial + column * (zoom ? 2u : 4u) + band * 256u;
            unsigned marker = t[at] | t[at+1];
            if (!zoom) marker |= t[at+2] | t[at+3];
            for (line = 0; line < rows; ++line) {
                uint8_t packed[4] = {0,0,0,0};
                const unsigned *sample = zoom ? doubled[line] : normal[line];
                unsigned out = (21u + band*rows + line) * 40u + column;
                for (bit = 0; bit < 8u; ++bit) {
                    unsigned colour = t[at + sample[bit]];
                    for (plane = 0; plane < 4u; ++plane)
                        packed[plane] = (uint8_t)((packed[plane] << 1) | ((colour >> plane) & 1u));
                }
                for (plane = 0; plane < 4u; ++plane) {
                    uint8_t a = packed[plane], b = a;
                    if (marker & 0x80u) {
                        uint8_t mask = zoom ? 0xf0u : 0xc0u;
                        a |= mask; b &= (uint8_t)~mask;
                    }
                    pages[plane*8000u + out] = a;
                    pages[BLOOD_EGA_PAGE_BYTES + plane*8000u + out] = b;
                }
            }
        }
    }
    return 0;
}
uint8_t blood_planet_next_longitude(uint8_t longitude, uint8_t rotation_step)
{
    return (uint8_t)(longitude + 3u*rotation_step);
}

int blood_planet_detail_click(uint8_t *zoom, uint8_t icon)
{
    if (!zoom) return -1;
    *zoom ^= 1u;
    return icon == 8u;
}
int blood_planet_clear_markers(uint8_t *texture, size_t texture_size)
{
    size_t i;
    if (!texture || texture_size < BLOOD_PLANET_TEXTURE_BYTES) return -1;
    for (i = 0; i < BLOOD_PLANET_TEXTURE_BYTES; ++i) texture[i] &= 0x7fu;
    return 0;
}
