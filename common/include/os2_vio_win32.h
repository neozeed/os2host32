#ifndef OS2_VIO_WIN32_H
#define OS2_VIO_WIN32_H

#include "os2_vio.h"

/* Renderer-owned viewport bookkeeping; no Win32 state in common VIO. */
struct Os2VioWin32State {
    struct Os2VioSession *session;
    short left, top;
    int viewport_valid;
};
/* State must live as long as the session. */
void os2_vio_win32_session_init(struct Os2VioSession *session,
                               struct Os2VioWin32State *state);

/* Pure mapping helper kept separately testable on non-Windows hosts. */
unsigned short os2_vio_win32_attribute(unsigned char os2_attribute);

#endif
