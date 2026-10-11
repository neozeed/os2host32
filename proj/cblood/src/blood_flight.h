#ifndef BLOOD_FLIGHT_H
#define BLOOD_FLIGHT_H
#include <stdint.h>
/* Original terrain DS offsets 202C,202D,202E,203D. These are byte counters,
 * not seconds. The surrounding flyover decides how often to call this. */
typedef struct BloodAttack {
    uint8_t delay, phase, intensity, sounding;
} BloodAttack;
typedef enum BloodAttackResult {
    BLOOD_ATTACK_NONE = 0,
    BLOOD_ATTACK_DRAW = 1,
    BLOOD_ATTACK_FLIGHT_FAILURE = 2
} BloodAttackResult;
typedef enum BloodAttackSound {
    BLOOD_ATTACK_SOUND_NONE = 0,
    BLOOD_ATTACK_SOUND_STOP = 1,
    BLOOD_ATTACK_SOUND_PLAY_2 = 2
} BloodAttackSound;
/* Original 7879..790D, stopping before rasterization or failure transition.
 * hostile_gate is DS:1134, as set by blood_select_planet. player/ground are
 * unsigned height bytes, not screen Y. Return negative for invalid pointers. */
int blood_attack_update(BloodAttack *state, uint8_t hostile_gate,
                         uint8_t player_height, uint8_t ground_height,
                         BloodAttackSound *sound);
#endif
