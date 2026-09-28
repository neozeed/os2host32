/*
 * MOUCALLS.dll - thin exported OS/2 Base Mouse ABI veneer.
 *
 * MOUCALLS R2 starts backend-neutral from its first native implementation:
 * OS/2 mouse state/event semantics live in common/mou, while Win32 pointer
 * observation/warping lives in common/win32/os2_mou_win32.c.
 */

#include "os2_mou.h"
#include "os2_mou_win32.h"

#ifndef __cdecl
#define __cdecl
#endif

static struct Os2MouSession *mou_session(void)
{
    return os2_mou_win32_session();
}

unsigned short __cdecl MouGetPtrShape(unsigned char *buffer,
                                       struct Os2MouPtrShape *shape,
                                       unsigned short hmou)
{
    return os2_mou_MouGetPtrShape(mou_session(), buffer, shape, hmou);
}

unsigned short __cdecl MouSetPtrShape(const unsigned char *buffer,
                                       const struct Os2MouPtrShape *shape,
                                       unsigned short hmou)
{
    return os2_mou_MouSetPtrShape(mou_session(), buffer, shape, hmou);
}

unsigned short __cdecl MouGetNumMickeys(unsigned short *mickeys,
                                         unsigned short hmou)
{
    return os2_mou_MouGetNumMickeys(mou_session(), mickeys, hmou);
}

unsigned short __cdecl MouGetThreshold(struct Os2MouThreshold *threshold,
                                        unsigned short hmou)
{
    return os2_mou_MouGetThreshold(mou_session(), threshold, hmou);
}

unsigned short __cdecl MouGetScaleFact(struct Os2MouScaleFact *scale,
                                        unsigned short hmou)
{
    return os2_mou_MouGetScaleFact(mou_session(), scale, hmou);
}

unsigned short __cdecl MouFlushQue(unsigned short hmou)
{
    return os2_mou_MouFlushQue(mou_session(), hmou);
}

unsigned short __cdecl MouGetNumButtons(unsigned short *buttons,
                                         unsigned short hmou)
{
    return os2_mou_MouGetNumButtons(mou_session(), buttons, hmou);
}

unsigned short __cdecl MouClose(unsigned short hmou)
{
    return os2_mou_MouClose(mou_session(), hmou);
}

unsigned short __cdecl MouSetThreshold(const struct Os2MouThreshold *threshold,
                                        unsigned short hmou)
{
    return os2_mou_MouSetThreshold(mou_session(), threshold, hmou);
}

unsigned short __cdecl MouSetScaleFact(const struct Os2MouScaleFact *scale,
                                        unsigned short hmou)
{
    return os2_mou_MouSetScaleFact(mou_session(), scale, hmou);
}

unsigned short __cdecl MouGetNumQueEl(struct Os2MouQueInfo *info,
                                       unsigned short hmou)
{
    return os2_mou_MouGetNumQueEl(mou_session(), info, hmou);
}

unsigned short __cdecl MouDeRegister(void)
{
    return os2_mou_MouDeRegister(mou_session());
}

unsigned short __cdecl MouGetEventMask(unsigned short *mask,
                                        unsigned short hmou)
{
    return os2_mou_MouGetEventMask(mou_session(), mask, hmou);
}

unsigned short __cdecl MouSetEventMask(const unsigned short *mask,
                                        unsigned short hmou)
{
    return os2_mou_MouSetEventMask(mou_session(), mask, hmou);
}

unsigned short __cdecl MouOpen(const char *driver_name, unsigned short *hmou)
{
    return os2_mou_MouOpen(mou_session(), driver_name, hmou);
}

unsigned short __cdecl MouRemovePtr(const struct Os2MouNoPtrRect *rect,
                                     unsigned short hmou)
{
    return os2_mou_MouRemovePtr(mou_session(), rect, hmou);
}

unsigned short __cdecl MouGetPtrPos(struct Os2MouPtrLoc *loc,
                                     unsigned short hmou)
{
    return os2_mou_MouGetPtrPos(mou_session(), loc, hmou);
}

unsigned short __cdecl MouReadEventQue(struct Os2MouEventInfo *event,
                                        unsigned short *wait,
                                        unsigned short hmou)
{
    return os2_mou_MouReadEventQue(mou_session(), event, wait, hmou);
}

unsigned short __cdecl MouSetPtrPos(const struct Os2MouPtrLoc *loc,
                                     unsigned short hmou)
{
    return os2_mou_MouSetPtrPos(mou_session(), loc, hmou);
}

unsigned short __cdecl MouGetDevStatus(unsigned short *status,
                                        unsigned short hmou)
{
    return os2_mou_MouGetDevStatus(mou_session(), status, hmou);
}

unsigned short __cdecl MouSynch(unsigned short wait)
{
    return os2_mou_MouSynch(mou_session(), wait);
}

unsigned short __cdecl MouRegister(const char *module_name,
                                    const char *entry_name,
                                    uint32_t functions)
{
    return os2_mou_MouRegister(mou_session(), module_name, entry_name,
                                functions);
}

unsigned short __cdecl MouSetDevStatus(const unsigned short *status,
                                        unsigned short hmou)
{
    return os2_mou_MouSetDevStatus(mou_session(), status, hmou);
}

unsigned short __cdecl MouDrawPtr(unsigned short hmou)
{
    return os2_mou_MouDrawPtr(mou_session(), hmou);
}

unsigned short __cdecl MouInitReal(const char *driver_name)
{
    return os2_mou_MouInitReal(mou_session(), driver_name);
}
