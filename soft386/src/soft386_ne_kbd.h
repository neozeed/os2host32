#ifndef SOFT386_NE_KBD_H
#define SOFT386_NE_KBD_H

#include <stdint.h>

/* Native/host keyboard backend for OS/2 1.x NE scalar keyboard calls.
 * The guest 16-bit Pascal frame is decoded by soft386_os2.c. */
uint32_t soft386_ne_kbd_flush(uint16_t hkbd);

#endif
