#ifndef BLOOD_PLANET_UI_H
#define BLOOD_PLANET_UI_H
#include <stddef.h>
#include <stdint.h>
#define BLOOD_PLANET_TEXTURE_BYTES 16384u
#define BLOOD_EGA_PAGE_BYTES 32000u /* four planes, each 40*200 bytes */
#define BLOOD_EGA_TWO_PAGES_BYTES 64000u
/* Original 57E2 hit test. Returns -1 without a click; 0..9 for a live icon;
 * 255 for a click outside every live icon. enabled must contain exact 0/1
 * settled icon states; animated 0x80/0x81 states are not clickable. */
int blood_console_hit(uint16_t x, uint16_t y, uint8_t buttons,
                       const uint8_t enabled[10]);
/* Original 56FA bit order: bit0 controls icon9, not icon0. */
void blood_console_settled_icons(uint16_t mask, uint8_t enabled[10]);
/* 59D7..59EA: returning from an accessible previously landed-on world enables
 * icon9 ('?'). A world that is not accessible takes the restricted return. */
uint16_t blood_console_after_planet(uint16_t mode, int accessible, int visited);
/* Main console 2364..239D: returns a FILE-offset entry, or zero for no action.
 * Caller must use the actual hit/enable policy above, not bypass icon gating.
 */
uint16_t blood_console_action_entry(uint8_t icon);
/* Special input seeds at 7AAA..7ABC. The game re-seeds both main RNG words
 * and the terrain RNG from these packed coordinates on terrain entry. */
uint16_t blood_planet_coordinate_seed(uint8_t x, uint8_t y);
/* Planet appearance RNG 5DB5..5DF0. Returns next value with the original ADC
 * carry, including its non-linear overflow detail; not a guessed multiplier. */
uint16_t blood_planet_random16(uint16_t seed);
/* Render ONE COMPLETE detail-view pass of original 6385..65B4 with no input
 * event during the pass. zoom=0 is first detail view; zoom=1 is its doubled
 * magnification. Texture is the original generated 16 KiB byte buffer, NOT
 * an EGX asset or EGA bitplanes. Low nibble is colour; bit7 requests a blinking
 * marker. Both EGA pages are updated in x=0..319,y=21..144; outside is retained.
 * Returns -1 on malformed sizes/zoom. Does not run generation, input or timing.
 */
int blood_planet_detail(const uint8_t *texture, size_t texture_size,
                        uint8_t *pages, size_t pages_size, unsigned zoom,
                        uint8_t longitude);
/* Original 5A79: toggle on any returned click, then continue only for icon8.
 * New entry resets zoom to zero separately. Returns -1 for a null pointer. */
int blood_planet_detail_click(uint8_t *zoom, uint8_t icon);
/* Original 5A87..5A9C clears the transient high-bit markers on detail exit. */
int blood_planet_clear_markers(uint8_t *texture, size_t texture_size);
/* Called only after a whole detail pass with NO exit click. Original updates
 * longitude by three times the signed-looking byte step, modulo 256. */
uint8_t blood_planet_next_longitude(uint8_t longitude, uint8_t rotation_step);
#endif
