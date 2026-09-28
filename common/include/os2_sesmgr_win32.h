#ifndef OS2_SESMGR_WIN32_H
#define OS2_SESMGR_WIN32_H

#include "os2_sesmgr.h"

/* Initialize/destroy one native Win32 SESMGR backend instance. */
int os2_sesmgr_win32_init(struct Os2SesmgrSession *session);
void os2_sesmgr_win32_destroy(struct Os2SesmgrSession *session);

#endif
