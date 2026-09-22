#ifndef OS2_WIN32_SERVICES_H
#define OS2_WIN32_SERVICES_H

#include "os2_personality.h"

/* Win32 services shared by the native personality DLLs and Win64/WHP. */
os2_api_ret_t os2_win32_query_local_datetime(
    void *opaque,
    struct Os2LocalDateTime *value);

uint64_t os2_win32_monotonic_milliseconds(void *opaque);

#endif
