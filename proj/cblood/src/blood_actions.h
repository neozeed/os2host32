#ifndef BLOOD_ACTIONS_H
#define BLOOD_ACTIONS_H
#include <stddef.h>
#include <stdint.h>
#define BLOOD_NO_CHARACTER 0xffffu
#define BLOOD_CHARACTERS 64u
#define BLOOD_GAME_FLAG_BYTES 256u

/* Original first pair is a WANTED destination, not immutable birthplace.
 * Its Y high bit and current Y high bit can mark planet destruction.
 * Raw bytes are preserved here; do not mask those bits before game lookup. */
typedef struct BloodCharacter { uint8_t wanted_x, wanted_y, actual_x, actual_y; } BloodCharacter;
typedef struct BloodGame {
    BloodCharacter character[64];
    uint8_t flags[256];
    uint8_t visited_xy[256];   /* DS:0475: 128 coordinate pairs */
    uint8_t destroyed_xy[512]; /* DS:0575: 256 coordinate pairs */
    uint16_t destroyed_write, destroyed_count, visited_write;
    uint16_t active_record, active_bank, carried_record;
    uint16_t mode, pending_extension, grace_minutes;
    uint8_t planet_x, planet_y, onboard, hostile_flight;
} BloodGame;

int blood_game_bit(const BloodGame *s, unsigned bit);
int blood_game_set_bit(BloodGame *s, unsigned bit, int on);
/* Original 3EB2..3EF7. Permission bit6 is checked by the surrounding UI,
 * not this primitive. Bit5 is CAPTURED now, for possible later reward. */
int blood_take_aboard(BloodGame *s);
/* Original 3E60..3EB1; caller decides whether to clear mode bit8. */
int blood_release_aboard(BloodGame *s);
/* Original 3EF8..3F2B, with only graphical call 56FA omitted.
 * IsNotDestructed is intentionally NOT cleared: this is not planet destruction. */
int blood_disintegrate_aboard(BloodGame *s);
/* Original 4655..46E1: 0 destroyed world; 1 accessible (possibly empty).
 * Populates matched character BEFORE separate IsAlive/IsThere checks. */
int blood_select_planet(BloodGame *s);
/* Original 2C85..2C9E eligibility subset. */
int blood_can_converse(const BloodGame *s);
/* Original 45C5..4654; destroy planet, not onboard passenger. */
int blood_destroy_planet(BloodGame *s);
/* Original visited-world list 4577/459E. Cursor wrap reproduces the original
 * count/cursor conflation: old pairs beyond the new cursor are not searched. */
int blood_record_visit(BloodGame *s);
int blood_was_visited(const BloodGame *s);
/* Original post-teleport branch 3182, not a substitute duplicate counter. */
int blood_teleport_ending_flag(const BloodGame *s);

/* Original save payload: global DS:0373..07A3, dictionary first 256 bytes,
 * and the 4096-byte passenger image. The first seven file bytes duplicate
 * the first seven payload bytes. Unnamed fields are retained losslessly. */
#define BLOOD_SAVE_GLOBAL_SIZE 0x431u
#define BLOOD_SAVE_PAYLOAD_SIZE 0x1531u
#define BLOOD_SAVE_FILE_SIZE 0x1538u
typedef struct BloodSaveImage {
    uint8_t global_block[BLOOD_SAVE_GLOBAL_SIZE];
    uint8_t dictionary_prefix[256];
    uint8_t passenger_image[4096];
} BloodSaveImage;
int blood_save_decode(BloodSaveImage *s, const uint8_t *file, size_t length);
int blood_save_encode(const BloodSaveImage *s, uint8_t *file, size_t length);
#endif
