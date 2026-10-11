#ifndef BLOOD_CONVERSATION_H
#define BLOOD_CONVERSATION_H
#include "blood_upcom.h"
#include "blood_actions.h"
/* DS:01B2 is presentation state, distinct from the VM's DS:085C reply.
 * Only its first eight icons are displayed. Extra expansion bytes are kept
 * for differential verification of 4498/451E, rather than drawn as new slots. */
#define BLOOD_REPLY_ICONS 32u
/* B2 3F2C: both random words start at minutes + seconds + service clock. */
void blood_conversation_seed(uint16_t minutes, uint16_t seconds,
                             uint16_t service_clock, uint16_t *low, uint16_t *high);
int blood_conversation_begin(BloodUpcom *v, const uint8_t dictionary[2560],
                             const BloodGame *g, uint8_t icons[BLOOD_REPLY_ICONS]);
int blood_conversation_click(BloodUpcom *v, uint8_t phrase[8],
                             const uint8_t dictionary[2560], const BloodGame *g,
                             uint8_t icons[BLOOD_REPLY_ICONS]);
int blood_conversation_expand(const BloodUpcom *v, const uint8_t dictionary[2560],
                              const BloodGame *g, uint8_t icons[BLOOD_REPLY_ICONS]);
unsigned blood_conversation_hover(const uint8_t icons[BLOOD_REPLY_ICONS],
                                  unsigned visible, const uint8_t iconmap[128],
                                  int tip_x, int tip_y);
#endif
