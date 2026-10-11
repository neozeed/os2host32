#ifndef BLOOD_PLANET_SURFACE_H
#define BLOOD_PLANET_SURFACE_H

#include <stddef.h>
#include <stdint.h>
#include "blood_planet_ui.h"

/* The planet's orbital/detail texture, NOT the nine flight height strips.
 * B2 builds a 256-column by 64-row toroidal scalar field, then maps its eight
 * height bands to EGA colour indexes. No renderer, DOS API or CPU is needed. */
#define BLOOD_PLANET_SURFACE_WIDTH 256u
#define BLOOD_PLANET_SURFACE_HEIGHT 64u
#define BLOOD_PLANET_COLOUR_TABLE_BYTES 64u

enum BloodPlanetSurfaceStage {
    BLOOD_PLANET_SURFACE_PLANNED = 0,
    BLOOD_PLANET_SURFACE_HEIGHTS = 1,
    BLOOD_PLANET_SURFACE_COLOURS = 2
};

typedef struct BloodPlanetSurface {
    uint16_t rng;                 /* Original main ES:1138 appearance RNG. */
    uint8_t palette;              /* ES:113F / 8, in 0..7. */
    uint8_t rotation_step;        /* DS:1136; negative steps retained as bytes. */
    uint8_t initial_spacing;      /* 8,16,32; not the final spacing of 1. */
    uint8_t vertical_modifier;    /* Original selected 5C0C/5C1F call target. */
    uint8_t horizontal_modifier;  /* Original selected 5C48/5C5B call target. */
    uint8_t amplitude_mask;       /* Original ES:113E, halved after each pass. */
    uint8_t shear_step;           /* 0=none; otherwise 1..4 bytes more per row. */
    uint8_t shear_right;          /* 0=left rotation; 1=right. */
    uint8_t stage;                /* Guards accidental double colourization. */
} BloodPlanetSurface;

/* File 5B36..5BCB: coordinate seed and exact source selection order.
 * Replaces original self-modified CALL operands with two explicit indexes. */
int blood_planet_surface_plan(uint8_t x, uint8_t y, BloodPlanetSurface *state);

/* File 5D4D..5DB4: saturating midpoint modifier. Types 0..7 are selected by
 * B2. Type8 is the ninth, present-but-unselected table entry (masked subtract).
 * This standalone helper accepts any byte midpoint/mask and any RNG state.
 * Returns -1 for invalid pointers/type without changing caller state. */
int blood_planet_surface_adjust(uint8_t midpoint, unsigned modifier,
                               uint8_t amplitude_mask, uint16_t *rng,
                               uint8_t *result);

/* File 5B36..5C80: construct all 16,384 raw bytes. Both axes wrap. Initial
 * output contents are irrelevant; no uninitialized scratch bytes are read.
 * state and output must not overlap. Valid output on success only. */
int blood_planet_heightfield(uint8_t x, uint8_t y, uint8_t *out, size_t size,
                            BloodPlanetSurface *state);

/* File 5C81..5D32: optional row shear, then eight-band colour mapping.
 * palettes is the 64-byte original table at MZ file145A (main DS:115A), or a
 * caller-supplied index table. Entries must be 0..15. No artwork is embedded.
 * Requires a HEIGHTS-stage state. No mutation occurs on invalid arguments.
 * Original scratch copy of the globe's projection stream at5D33..5D4C is NOT
 * part of the texture. Projection tables are a separate renderer resource. */
int blood_planet_colourize(uint8_t *texture, size_t size,
                          const uint8_t *palettes, size_t palette_size,
                          BloodPlanetSurface *state);

/* Complete coordinate -> colour texture convenience operation. The palette
 * source, output and state must not overlap. Main gameplay RNG is untouched:
 * the original appearance algorithm uses its separate 16-bit recurrence. */
int blood_planet_surface_generate(uint8_t x, uint8_t y,
                                 const uint8_t *palettes, size_t palette_size,
                                 uint8_t *out, size_t size,
                                 BloodPlanetSurface *state);
/* File 5A1E..5A51: when hostile!=0, reseed the MAIN RNG from X, add
 * 32 high-bit markers and return its consumed state. A non-hostile pass leaves
 * both words and the texture unchanged. This is deliberately separate from
 * appearance generation: silently sharing/resetting the wrong RNG is a bug. */
int blood_planet_detail_markers(uint8_t x, int hostile, uint8_t *texture,
                               size_t size, uint16_t *main_rng_low,
                               uint16_t *main_rng_high);
#endif
