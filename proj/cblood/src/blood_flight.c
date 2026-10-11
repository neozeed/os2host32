#include "blood_flight.h"
int blood_attack_update(BloodAttack *s, uint8_t gate, uint8_t player,
                         uint8_t ground, BloodAttackSound *sound)
{
    int exposed;
    if (!s || !sound) return -1;
    *sound = BLOOD_ATTACK_SOUND_NONE;
    if (!gate) return BLOOD_ATTACK_NONE;
    /* Original unsigned SUB borrows if below terrain; it is not a signed
     * eight-bit subtraction. Both absolute altitude and clearance matter. */
    exposed = player >= 64u && player >= ground && (unsigned)player - ground >= 12u;
    if (exposed) {
        if (s->delay) {
            if (s->delay == 255u) s->delay = 12;
            s->delay = (uint8_t)(s->delay - 1u);
            if (s->delay) goto decay;
        }
        if (s->phase == 1u) {
            s->sounding = 1;
            *sound = BLOOD_ATTACK_SOUND_PLAY_2;
        } else if (s->sounding) {
            s->sounding = 0;
            *sound = BLOOD_ATTACK_SOUND_STOP;
        }
        s->phase = (uint8_t)(s->phase - 1u);
        if (!s->phase) {
            s->phase = 4;
            s->intensity = (uint8_t)(s->intensity + 1u);
        }
    } else {
        s->delay = 255;
        goto decay;
    }
    goto result;
decay:
    if (s->sounding) {
        s->sounding = 0;
        *sound = BLOOD_ATTACK_SOUND_STOP;
    }
    s->phase = (uint8_t)(s->phase - 1u);
    if (!s->phase) {
        s->phase = 3;
        if (s->intensity) s->intensity = (uint8_t)(s->intensity - 1u);
    }
result:
    if (!s->intensity) return BLOOD_ATTACK_NONE;
    if (s->intensity > 32u) return BLOOD_ATTACK_FLIGHT_FAILURE;
    return BLOOD_ATTACK_DRAW;
}
