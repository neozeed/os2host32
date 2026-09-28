#ifndef OS2_VIO_WIN32_H
#define OS2_VIO_WIN32_H

#include "os2_vio.h"

/* Initialize a VIO session using the current process Win32 console. */
void os2_vio_win32_session_init(struct Os2VioSession *session);

/* Pure mapping helper kept separately testable on non-Windows hosts. */
unsigned short os2_vio_win32_attribute(unsigned char os2_attribute);

#endif
