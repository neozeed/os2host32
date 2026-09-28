#ifndef OS2_DOSCALLS_WIN32_H
#define OS2_DOSCALLS_WIN32_H

#include "os2_doscalls.h"

/* Bind one native DOSCALLS session to the Win32 backend. */
void os2_doscalls_win32_session_init(struct Os2DosSession *session);
struct Os2DosSession *os2_doscalls_win32_session(void);

#endif
