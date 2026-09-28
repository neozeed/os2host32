#ifndef OS2_MOU_BACKEND_H
#define OS2_MOU_BACKEND_H

/* Host pointing-device contract for backend-neutral OS/2 MOUCALLS semantics. */

struct Os2MouHostEvent;

typedef unsigned short Os2MouApiRet;

struct Os2MouBackendOps {
    Os2MouApiRet (*activate)(void *opaque, unsigned short *buttons,
                             unsigned short *mickeys,
                             unsigned short *row, unsigned short *col);
    Os2MouApiRet (*deactivate)(void *opaque);
    Os2MouApiRet (*read_event)(void *opaque, int wait,
                               struct Os2MouHostEvent *event,
                               int *present);
    Os2MouApiRet (*flush_events)(void *opaque);
    Os2MouApiRet (*set_pointer_position)(void *opaque,
                                         unsigned short row,
                                         unsigned short col);
};

#endif
