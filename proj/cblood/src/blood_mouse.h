#ifndef BLOOD_MOUSE_H
#define BLOOD_MOUSE_H
#include "blood_terrain.h"
/* B2 file 4D1C..4D90 + 75A4..75C4. DOS buttons: bit0 left, bit1 right.
 * 4F14 cockpit bounds are retained during flight. Logical x is DOS CX/2.
 * Flight initialization inhibits the late-game trembling path (DS:10F3=1).
 * Host recenters at logical (160,175) after each consumed sample.
 * This function does not reproduce driver sensitivity or frame scheduling. */
BloodFlightInput blood_mouse_flight(int x, int y, uint8_t dos_buttons);
#endif
