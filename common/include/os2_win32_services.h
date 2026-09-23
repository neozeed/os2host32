#ifndef OS2_WIN32_SERVICES_H
#define OS2_WIN32_SERVICES_H

#include "os2_personality.h"

struct Os2NlsState;

/* Win32 services shared by the native personality DLLs and Win64/WHP. */
os2_api_ret_t os2_win32_query_local_datetime(
    void *opaque,
    struct Os2LocalDateTime *value);

uint64_t os2_win32_monotonic_milliseconds(void *opaque);

/* Select an R1 built-in NLS profile from host defaults when possible.
 * Guest-visible formatting remains owned by os2_nls, not Win32 structures. */
void os2_win32_initialize_nls(struct Os2NlsState *state);

#endif
