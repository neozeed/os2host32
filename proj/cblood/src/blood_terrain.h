#ifndef BLOOD_TERRAIN_H
#define BLOOD_TERRAIN_H
/* Captain Blood DOS/EGA: flight-heightfield, movement and landscape projection.
 * This is a source reconstruction, NOT an x86 interpreter. Original offsets
 * in comments refer to the recovered MZ FILE, including the R19 plaintext-tail
 * correction. All host I/O and scheduling stay outside this module.
 *
 * The original rotates nine 1024-byte buffers and reuses a scratch allocation
 * for recipes, profiles and draw commands. Keeping its relative offsets here
 * lets us import/cross-check a capture without storing any DOS segment or host
 * pointer in game data. This arena is an INTERNAL work layout, not a save format.
 */
#include <stddef.h>
#include <stdint.h>

#define BLOOD_TERRAIN_BYTES 65536u
#define BLOOD_TERRAIN_TABLE_BYTES 612u /* main DS:149C..16FF */
#define BLOOD_TERRAIN_EGA_BYTES 32000u /* 4 planes * 8000 bytes */

typedef struct BloodTerrainTables {
    uint8_t data[BLOOD_TERRAIN_TABLE_BYTES];
    uint8_t pixel_masks[8];
    uint8_t drift_half; /* original canyon helper reads MAIN DS:2033 */
    uint8_t glyphs[38]; /* original main DS:1700..1725, one-bit HUD shapes */
} BloodTerrainTables;

typedef struct BloodTerrain {
    uint8_t mem[BLOOD_TERRAIN_BYTES];
    uint16_t rng_low, rng_high; /* same shared two-word generator as UpCom */
    uint8_t canyon_modifier, edge_modifier;
    uint8_t ordinary_modifier, projection_bias;
    uint8_t stationary_bias, initialized;
    uint8_t sound_event, failed;
    uint32_t operations; /* diagnostic, not original saved state */
} BloodTerrain;

typedef struct BloodFlightInput {
    int16_t horizontal, vertical; /* original logical offsets from (175,160) */
    uint8_t alternate_axis;      /* original main DS:0240 */
    uint8_t button_code;         /* original (DS:01A0 & 80h) | DS:0241 */
    uint8_t speed_event;         /* original consumed DS:01A0 byte */
    uint8_t mouse_mode;          /* original DS:1063 */
    uint8_t bios_modifiers;      /* original BIOS byte 0040:0017 */
} BloodFlightInput;

/* Named locations used by capture tools and tests. No unaligned host casts. */
enum BloodTerrainOffset {
    BT_CAMERA=0x2000, BT_DEPTH=0x2002, BT_PHASE=0x2004, BT_LAYER=0x2005,
    BT_CENTER=0x2008, BT_DISTANCE=0x200a, BT_SPEED=0x200c,
    BT_KEY_DIVIDER=0x200e, BT_SPARE=0x2010, BT_LOCAL_RNG=0x2012,
    BT_RECIPE=0x2014, BT_SECTION=0x2016, BT_SECTION_LEFT=0x2018,
    BT_SECTION_SEED=0x201a, BT_FLOOR=0x201c, BT_INTERPOLATED=0x201e,
    BT_INTERP_DELAY=0x201f, BT_PROGRESS=0x2020, BT_CANYON_GRID=0x2022,
    BT_EDGE_GRID=0x2024, BT_CANYON_BASE=0x2026, BT_CANYON_MASK=0x2027,
    BT_EDGE_LEFT=0x2028, BT_EDGE_RIGHT=0x202a,
    BT_ATTACK_DELAY=0x202c, BT_ATTACK_PHASE=0x202d, BT_ATTACK_INTENSITY=0x202e,
    BT_STEER_INDICATOR=0x202f, BT_INDICATOR_DIV=0x2030, BT_INDICATOR_PHASE=0x2031,
    BT_IMPACT=0x2032, BT_HALF=0x2033, BT_SPAN=0x2034,
    BT_HEIGHT_INDICATOR=0x2036, BT_PREVIOUS_HEIGHT=0x2038,
    BT_COURSE=0x203a, BT_SOUND_DIV=0x203c, BT_ATTACK_SOUND=0x203d,
    BT_NO_COLLISION=0x203e, BT_IMPACTS=0x203f, BT_COMMAND_END=0x2040,
    BT_STRIPS=0x2042, BT_CENTERS=0x2052, BT_HEIGHT_LUT=0x2100,
    BT_VSCALE=0x2200, BT_HSCALE=0x2300, BT_HORIZON=0x2400,
    BT_COLOURS=0x2500, BT_SCREEN_ROWS=0x2600, BT_FIRST_STRIP=0x2700,
    BT_RECIPES=0x4b00
};
uint16_t blood_terrain_word(const BloodTerrain *t, unsigned offset);
void blood_terrain_put_word(BloodTerrain *t, unsigned offset, uint16_t value);

/* Extract numeric original tables, not executable instructions. The Python
 * intake checks complete hashes; C requires exact file length and table shape. */
int blood_terrain_tables(BloodTerrainTables *out, const uint8_t *exe, size_t bytes);
/* 7A8F: initialize controls, projection tables and relative pointers. */
int blood_terrain_init(BloodTerrain *t, const BloodTerrainTables *tables,
                       uint8_t x, uint8_t y);
/* 7BC2..7C82: recipes plus nine original strips. */
int blood_terrain_prepare(BloodTerrain *t, const BloodTerrainTables *tables);
/* 7D0D / 7DE7: produce and refine the current spare strip. */
int blood_terrain_generate_strip(BloodTerrain *t, const BloodTerrainTables *tables);
int blood_terrain_interpolate_strip(BloodTerrain *t);
/* 7734: update height and the projection constants originally patched in CS. */
void blood_terrain_height(BloodTerrain *t, uint8_t height);
/* 75A4 after the DOS input call. Does not poll/recenter a host mouse. */
int blood_terrain_control(BloodTerrain *t, const BloodFlightInput *input);
/* 811F..8216: distance, rotating strips, collisions; no pixel output.
 * Return 0 normal, 1 collision, 2 fatal collision, negative invalid state. */
int blood_terrain_advance(BloodTerrain *t, const BloodTerrainTables *tables);
/* 8217..85AB: produce the original flight point list. */
int blood_terrain_project(BloodTerrain *t);
/* 85AC: draw its point list into a planar EGA page, retaining all other pixels. */
int blood_terrain_draw(const BloodTerrain *t, uint8_t *page, size_t bytes);
/* Clear exactly the flight viewport, x32..287,y20..145. */
int blood_terrain_clear(uint8_t *page, size_t bytes);
/* 7766..79AC: moving aiming marks, course suggestions and hostile-fire
 * indication. Return 0 normal, 2 hostile failure, negative invalid state.
 * sound_event uses 1=stop, 2=play original sound4, 3=play original sound2.
 * No hardware or timing operations occur here. */
int blood_terrain_hud(BloodTerrain *t, const BloodTerrainTables *tables,
                       uint8_t hostile, uint8_t *page, size_t bytes);
/* One normal flight update. A positive result is an original collision/failure
 * boundary, not a host error. Sound is returned as a bounded event bitmask:
 * bit0 stop, bit1 sound4, bit2 sound2. No clock is advanced implicitly. */
int blood_terrain_step(BloodTerrain *t, const BloodTerrainTables *tables,
                        const BloodFlightInput *input, uint8_t hostile,
                        uint8_t *page, size_t bytes, unsigned *sound_events);
/* 7483/7492: the deliberate final moving approach. The original forces speed6
 * and inhibits collisions, but still accepts steering/altitude input. Each step
 * renders a new moving frame; returns1 only upon reaching phase15. The caller
 * must perform at least one step even when begin sees phase15. */
int blood_terrain_landing_begin(BloodTerrain *t);
int blood_terrain_landing_step(BloodTerrain *t, const BloodTerrainTables *tables,
                                const BloodFlightInput *input,
                                uint8_t *page, size_t bytes);
/* Shortcut entry through 74B5; generates terrain, not a cached scene. */
int blood_terrain_shortcut(BloodTerrain *t, const BloodTerrainTables *tables,
                           uint8_t x, uint8_t y);
/* 74B5..7529: final frame preparation and actual final advance/projection. */
int blood_terrain_finish(BloodTerrain *t, const BloodTerrainTables *tables,
                         uint8_t *page, size_t bytes);
/* 8650..8766: progressive 8*16 landscape slices. callback is optional and
 * receives the current EGA page after each pass (same borrowed buffer).
 * Host may schedule/display these, but it must not re-enter or mutate t. */
typedef void (*BloodLandscapeFrame)(void *opaque, const uint8_t *page,
                                    unsigned layer, unsigned slice);
int blood_terrain_landscape(BloodTerrain *t, uint8_t *page, size_t bytes,
                             BloodLandscapeFrame frame, void *opaque);
/* Single projector for differential testing/stepped stationary presentation.
 * Source is the strip at 4B00; caller sets LAYER/PHASE and stationary_bias. */
int blood_terrain_project_stationary(BloodTerrain *t);
#endif
