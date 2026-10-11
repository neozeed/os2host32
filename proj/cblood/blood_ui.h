#ifndef BLOOD_UI_H
#define BLOOD_UI_H
#include <stddef.h>
#include <stdint.h>
#define BU_PIXELS 64000u
#define BU_SHEET 4000u
typedef struct BloodEyes {
    uint8_t state[10], delay[10], countdown[10], picture[10];
    uint8_t quota, cadence, busy;
} BloodEyes;
typedef struct BloodTremor { uint8_t delay_x,delay_y;uint16_t delta_x,delta_y; } BloodTremor;
void blood_hand_tremor(BloodTremor *t,uint16_t minutes,uint16_t grace,uint8_t seconds,
                      int disabled,int *x,int *y,int low_x,int high_x,int low_y,int high_y);
typedef struct BloodHand { uint8_t state, delay, frame, pulse; } BloodHand;
typedef struct BloodAlienCache {
    uint16_t key, x, y, width, height;
    uint8_t pixels[4096];
} BloodAlienCache;
typedef struct BloodAlien {
    uint8_t *bank; size_t size; uint16_t descriptor;
    BloodAlienCache cache[4];
    uint16_t rng_low, rng_high;
    int error;
} BloodAlien;
/* B2 file offsets 50C0, 5778, 4CBE, 326C, 3FD1, 696A..6C9C.
 * Pixels are EGA indices. Sheets are mask + four 800-byte planes.
 * No colour-key transparency: (destination AND mask) OR source. */
int blood_ui_sprite(uint8_t *pixels, const uint8_t *sheet, size_t size,
                    unsigned row, unsigned height, int x, int y);
void blood_eyes_request(BloodEyes *e, uint16_t mask);
void blood_eye_step(BloodEyes *e, unsigned icon);
void blood_eyes_tick(BloodEyes *e);
void blood_eyes_draw(uint8_t *pixels, const BloodEyes *e,
                     const uint8_t sheets[5][BU_SHEET]);
void blood_eyes_draw_mask(uint8_t *pixels, const BloodEyes *e,
                          const uint8_t sheets[5][BU_SHEET],uint16_t visible);
void blood_hand_tick(BloodHand *h, uint8_t left);
void blood_hand_draw(uint8_t *pixels, const uint8_t *sheet,
                     const BloodHand *h, int x, int y, int long_arm);
void blood_ui_icon(uint8_t *pixels, const uint8_t *atlas, unsigned id,
                   uint8_t colour, unsigned x, unsigned y);
void blood_ui_text(uint8_t *pixels, const uint8_t font[472], const char *text,
                   unsigned x, unsigned y);
int blood_ui_available(uint8_t map[128], const uint8_t inverse[256],
                       const uint8_t *bank, size_t size);
int blood_alien_attach(BloodAlien *a, uint8_t *bank, size_t size);
int blood_alien_step(BloodAlien *a, uint8_t pixels[BU_PIXELS]);
void blood_alien_speech(BloodAlien *a, int speaking);
/* 6C9D: execute the bank's dedicated passenger pose before capture. */
int blood_alien_passenger_pose(BloodAlien *a, uint8_t pixels[BU_PIXELS]);
/* 548C/54E8 at 6CD8/6E3D: 80x70, x=40, y=40, four packed planes.
 * The original save reserves 4096 bytes; only the first 2800 are written. */
void blood_passenger_capture(uint8_t image[4096], const uint8_t pixels[BU_PIXELS]);
void blood_passenger_draw(uint8_t pixels[BU_PIXELS], const uint8_t image[4096]);
#endif
