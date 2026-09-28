#ifndef OS2_KBD_BACKEND_H
#define OS2_KBD_BACKEND_H

/* Host/input-engine contract for backend-neutral OS/2 KBDCALLS semantics. */

struct Os2KbdHostEvent;

typedef unsigned short Os2KbdApiRet;

struct Os2KbdBackendOps {
    Os2KbdApiRet (*read_event)(void *opaque, int wait,
                               struct Os2KbdHostEvent *event,
                               int *present);
    Os2KbdApiRet (*peek_event)(void *opaque,
                               struct Os2KbdHostEvent *event,
                               int *present);
    Os2KbdApiRet (*flush)(void *opaque);
    Os2KbdApiRet (*echo_bytes)(void *opaque, const char *bytes,
                               unsigned short count);
    unsigned long (*milliseconds)(void *opaque);
};

#endif
