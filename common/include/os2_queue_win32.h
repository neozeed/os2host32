#ifndef OS2_QUEUE_WIN32_H
#define OS2_QUEUE_WIN32_H

#include "os2_queue.h"

void os2_queue_win32_session_init(struct Os2QueueSession *session);
struct Os2QueueSession *os2_queue_win32_session(void);

#endif
