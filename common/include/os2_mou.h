#ifndef OS2_MOU_H
#define OS2_MOU_H

/*
 * Backend-neutral OS/2 Base Mouse semantics/state.
 *
 * The common layer owns HMOU allocation, event filtering/queueing, pointer
 * location, device status, event mask, scale/threshold values and the logical
 * pointer shape.  A backend supplies only host device activation, observation,
 * flushing and (optionally) host pointer warping.
 */

#include <stdint.h>
#include "os2_mou_backend.h"

#ifndef __cdecl
#define __cdecl
#endif

#define OS2_MOU_NO_ERROR                    0U
#define OS2_MOU_ERROR_INVALID_FUNCTION      1U
#define OS2_MOU_ERROR_INVALID_HANDLE        6U
#define OS2_MOU_ERROR_INVALID_PARAMETER    87U
#define OS2_MOU_ERROR_MOUSE_NO_DEVICE     385U
#define OS2_MOU_ERROR_MOUSE_INV_HANDLE    386U
#define OS2_MOU_ERROR_MOUSE_INV_PARMS     387U
#define OS2_MOU_NO_ERROR_MOUSE_NO_DATA    393U
#define OS2_MOU_ERROR_MOUSE_REGISTER      415U
#define OS2_MOU_ERROR_MOUSE_DEREGISTER    416U
#define OS2_MOU_ERROR_INVALID_IOWAIT      435U

#define OS2_MOU_MOUSE_MOTION                 0x0001U
#define OS2_MOU_MOUSE_MOTION_WITH_BN1_DOWN   0x0002U
#define OS2_MOU_MOUSE_BN1_DOWN                0x0004U
#define OS2_MOU_MOUSE_MOTION_WITH_BN2_DOWN   0x0008U
#define OS2_MOU_MOUSE_BN2_DOWN                0x0010U
#define OS2_MOU_MOUSE_MOTION_WITH_BN3_DOWN   0x0020U
#define OS2_MOU_MOUSE_BN3_DOWN                0x0040U
#define OS2_MOU_EVENT_MASK_ALL                0x007fU

#define OS2_MOU_BUTTON1                       0x0001U
#define OS2_MOU_BUTTON2                       0x0002U
#define OS2_MOU_BUTTON3                       0x0004U

#define OS2_MOU_NOWAIT                        0U
#define OS2_MOU_WAIT                          1U

#define OS2_MOU_NODRAW                        0x0001U
#define OS2_MOU_DRAW                          0x0000U
#define OS2_MOU_MICKEYS                       0x0002U
#define OS2_MOU_PELS                          0x0000U
#define OS2_MOU_DISABLED                      0x0100U
#define OS2_MOU_STATUS_MICKEYS                0x0200U

#define OS2_MOU_MAX_HANDLES                   8U
#define OS2_MOU_MAX_EVENTS                   64U
#define OS2_MOU_MAX_SHAPE_BYTES             256U

#pragma pack(push, 2)
struct Os2MouPtrLoc {
    unsigned short row;
    unsigned short col;
};

struct Os2MouPtrShape {
    unsigned short cb;
    unsigned short col;
    unsigned short row;
    unsigned short colHot;
    unsigned short rowHot;
};

struct Os2MouEventInfo {
    unsigned short fs;
    uint32_t time;
    unsigned short row;
    unsigned short col;
};

struct Os2MouQueInfo {
    unsigned short cEvents;
    unsigned short cmaxEvents;
};

struct Os2MouScaleFact {
    unsigned short rowScale;
    unsigned short colScale;
};

struct Os2MouNoPtrRect {
    unsigned short row;
    unsigned short col;
    unsigned short cRow;
    unsigned short cCol;
};

struct Os2MouThreshold {
    unsigned short Length;
    unsigned short Level1;
    unsigned short Lev1Mult;
    unsigned short Level2;
    unsigned short lev2Mult;
};
#pragma pack(pop)

typedef char Os2MouPtrLoc_must_be_4[(sizeof(struct Os2MouPtrLoc) == 4) ? 1 : -1];
typedef char Os2MouPtrShape_must_be_10[(sizeof(struct Os2MouPtrShape) == 10) ? 1 : -1];
typedef char Os2MouEventInfo_must_be_10[(sizeof(struct Os2MouEventInfo) == 10) ? 1 : -1];
typedef char Os2MouQueInfo_must_be_4[(sizeof(struct Os2MouQueInfo) == 4) ? 1 : -1];
typedef char Os2MouScaleFact_must_be_4[(sizeof(struct Os2MouScaleFact) == 4) ? 1 : -1];
typedef char Os2MouNoPtrRect_must_be_8[(sizeof(struct Os2MouNoPtrRect) == 8) ? 1 : -1];
typedef char Os2MouThreshold_must_be_10[(sizeof(struct Os2MouThreshold) == 10) ? 1 : -1];

/* Host-normalized absolute mouse sample. No Win32 types cross this boundary. */
struct Os2MouHostEvent {
    unsigned short row;
    unsigned short col;
    unsigned short buttons;
    unsigned short changed_buttons;
    uint32_t time;
    int motion;
};

struct Os2MouHandleSlot {
    int in_use;
    unsigned short value;
};

struct Os2MouSession {
    void *backend_opaque;
    const struct Os2MouBackendOps *backend;
    struct Os2MouHandleSlot handles[OS2_MOU_MAX_HANDLES];
    unsigned short next_handle;
    unsigned short open_count;
    unsigned short event_mask;
    unsigned short dev_status;
    unsigned short buttons;
    unsigned short mickeys;
    struct Os2MouPtrLoc pointer;
    struct Os2MouPtrLoc last_host_pointer;
    int have_last_host_pointer;
    struct Os2MouScaleFact scale;
    struct Os2MouThreshold threshold;
    struct Os2MouPtrShape shape;
    unsigned char shape_bytes[OS2_MOU_MAX_SHAPE_BYTES];
    int pointer_drawn;
    int exclusion_active;
    struct Os2MouNoPtrRect exclusion;
    struct Os2MouEventInfo events[OS2_MOU_MAX_EVENTS];
    unsigned short event_head;
    unsigned short event_count;
};

void os2_mou_session_init(struct Os2MouSession *session,
                          void *backend_opaque,
                          const struct Os2MouBackendOps *backend);

Os2MouApiRet os2_mou_MouOpen(struct Os2MouSession *session,
                              const char *driver_name,
                              unsigned short *hmou);
Os2MouApiRet os2_mou_MouClose(struct Os2MouSession *session,
                               unsigned short hmou);
Os2MouApiRet os2_mou_MouFlushQue(struct Os2MouSession *session,
                                  unsigned short hmou);
Os2MouApiRet os2_mou_MouGetPtrPos(struct Os2MouSession *session,
                                   struct Os2MouPtrLoc *loc,
                                   unsigned short hmou);
Os2MouApiRet os2_mou_MouSetPtrPos(struct Os2MouSession *session,
                                   const struct Os2MouPtrLoc *loc,
                                   unsigned short hmou);
Os2MouApiRet os2_mou_MouGetPtrShape(struct Os2MouSession *session,
                                     unsigned char *buffer,
                                     struct Os2MouPtrShape *shape,
                                     unsigned short hmou);
Os2MouApiRet os2_mou_MouSetPtrShape(struct Os2MouSession *session,
                                     const unsigned char *buffer,
                                     const struct Os2MouPtrShape *shape,
                                     unsigned short hmou);
Os2MouApiRet os2_mou_MouGetDevStatus(struct Os2MouSession *session,
                                      unsigned short *status,
                                      unsigned short hmou);
Os2MouApiRet os2_mou_MouSetDevStatus(struct Os2MouSession *session,
                                      const unsigned short *status,
                                      unsigned short hmou);
Os2MouApiRet os2_mou_MouGetNumButtons(struct Os2MouSession *session,
                                       unsigned short *buttons,
                                       unsigned short hmou);
Os2MouApiRet os2_mou_MouGetNumMickeys(struct Os2MouSession *session,
                                       unsigned short *mickeys,
                                       unsigned short hmou);
Os2MouApiRet os2_mou_MouReadEventQue(struct Os2MouSession *session,
                                      struct Os2MouEventInfo *event,
                                      unsigned short *wait,
                                      unsigned short hmou);
Os2MouApiRet os2_mou_MouGetNumQueEl(struct Os2MouSession *session,
                                     struct Os2MouQueInfo *info,
                                     unsigned short hmou);
Os2MouApiRet os2_mou_MouGetEventMask(struct Os2MouSession *session,
                                      unsigned short *mask,
                                      unsigned short hmou);
Os2MouApiRet os2_mou_MouSetEventMask(struct Os2MouSession *session,
                                      const unsigned short *mask,
                                      unsigned short hmou);
Os2MouApiRet os2_mou_MouGetScaleFact(struct Os2MouSession *session,
                                      struct Os2MouScaleFact *scale,
                                      unsigned short hmou);
Os2MouApiRet os2_mou_MouSetScaleFact(struct Os2MouSession *session,
                                      const struct Os2MouScaleFact *scale,
                                      unsigned short hmou);
Os2MouApiRet os2_mou_MouGetThreshold(struct Os2MouSession *session,
                                      struct Os2MouThreshold *threshold,
                                      unsigned short hmou);
Os2MouApiRet os2_mou_MouSetThreshold(struct Os2MouSession *session,
                                      const struct Os2MouThreshold *threshold,
                                      unsigned short hmou);
Os2MouApiRet os2_mou_MouRemovePtr(struct Os2MouSession *session,
                                   const struct Os2MouNoPtrRect *rect,
                                   unsigned short hmou);
Os2MouApiRet os2_mou_MouDrawPtr(struct Os2MouSession *session,
                                 unsigned short hmou);
Os2MouApiRet os2_mou_MouSynch(struct Os2MouSession *session,
                               unsigned short wait);
Os2MouApiRet os2_mou_MouInitReal(struct Os2MouSession *session,
                                  const char *driver_name);
Os2MouApiRet os2_mou_MouRegister(struct Os2MouSession *session,
                                  const char *module_name,
                                  const char *entry_name,
                                  uint32_t functions);
Os2MouApiRet os2_mou_MouDeRegister(struct Os2MouSession *session);

/* Scheduler-friendly nonblocking event primitive for future WHP/OS2SS. */
Os2MouApiRet os2_mou_try_read(struct Os2MouSession *session,
                               struct Os2MouEventInfo *event,
                               int *present,
                               unsigned short hmou);

#endif
