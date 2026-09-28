/*
 * quecalls.c - exported OS/2 QUECALLS ABI veneer.
 *
 * QUECALLS R2 keeps the existing four names/ordinals/signatures here while
 * moving queue object semantics/state into common/queue/os2_queue.c and
 * Win32 synchronization/process mechanics into common/win32/os2_queue_win32.c.
 *
 * Queue payload pointers are converted to/from opaque 32-bit OS/2 values.
 * Neither this veneer nor the common layer dereferences or copies payloads.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "os2_queue.h"
#include "os2_queue_win32.h"

#ifndef __cdecl
#define __cdecl
#endif

typedef Os2QueueApiRet O2APIRET;
typedef Os2QueueU32 O2ULONG;
typedef Os2QueueHandle O2HQUEUE;
typedef struct Os2QueueRequestData O2REQUESTDATA;

static struct Os2QueueSession *queue_session(void)
{
    return os2_queue_win32_session();
}

static int audio_trace_enabled(void)
{
    const char *value;
    value = getenv("LE2PE_TRACE_AUDIO");
    return value != NULL && *value != '\0';
}

O2APIRET __cdecl DosCreateQueue(O2HQUEUE *phq, O2ULONG flags,
                                const char *name)
{
    O2APIRET rc;
    rc = os2_queue_DosCreateQueue(queue_session(), phq, flags, name);
    if (audio_trace_enabled()) {
        fprintf(stderr, "QUECALLS: DosCreateQueue name=%s flags=%lu -> rc=%lu hq=%lu\n",
                name != NULL ? name : "(null)", (unsigned long)flags,
                (unsigned long)rc,
                (unsigned long)((rc == 0UL && phq != NULL) ? *phq : 0UL));
        fflush(stderr);
    }
    return rc;
}

O2APIRET __cdecl DosOpenQueue(O2ULONG *pOwnerPid, O2HQUEUE *phq,
                              const char *name)
{
    O2APIRET rc;
    rc = os2_queue_DosOpenQueue(queue_session(), pOwnerPid, phq, name);
    if (audio_trace_enabled()) {
        fprintf(stderr, "QUECALLS: DosOpenQueue name=%s -> rc=%lu hq=%lu owner=%lu\n",
                name != NULL ? name : "(null)", (unsigned long)rc,
                (unsigned long)((rc == 0UL && phq != NULL) ? *phq : 0UL),
                (unsigned long)((rc == 0UL && pOwnerPid != NULL) ? *pOwnerPid : 0UL));
        fflush(stderr);
    }
    return rc;
}

O2APIRET __cdecl DosWriteQueue(O2HQUEUE hq, O2ULONG request,
                               O2ULONG cbData, void *pData,
                               O2ULONG priority)
{
    O2APIRET rc;
    O2ULONG data_value;
    data_value = (O2ULONG)(uintptr_t)pData;
    rc = os2_queue_DosWriteQueue(queue_session(), hq, request, cbData,
                                 data_value, priority);
    if (audio_trace_enabled()) {
        fprintf(stderr, "QUECALLS: DosWriteQueue hq=%lu request=%lu len=%lu data=%p pri=%lu rc=%lu\n",
                (unsigned long)hq, (unsigned long)request,
                (unsigned long)cbData, pData, (unsigned long)priority,
                (unsigned long)rc);
        fflush(stderr);
    }
    return rc;
}

O2APIRET __cdecl DosReadQueue(O2HQUEUE hq, O2REQUESTDATA *request,
                              O2ULONG *pcbData, void **ppData,
                              O2ULONG element, O2ULONG wait,
                              unsigned char *pPriority, O2ULONG hev)
{
    O2APIRET rc;
    O2ULONG data_value;
    data_value = 0UL;
    rc = os2_queue_DosReadQueue(queue_session(), hq, request, pcbData,
                                &data_value, element, wait, pPriority, hev);
    if (rc == 0UL && ppData != NULL)
        *ppData = (void *)(uintptr_t)data_value;
    if (audio_trace_enabled()) {
        fprintf(stderr, "QUECALLS: DosReadQueue hq=%lu -> rc=%lu request=%lu len=%lu data=%p pri=%u\n",
                (unsigned long)hq, (unsigned long)rc,
                (unsigned long)((rc == 0UL && request != NULL) ? request->data : 0UL),
                (unsigned long)((rc == 0UL && pcbData != NULL) ? *pcbData : 0UL),
                (rc == 0UL && ppData != NULL) ? *ppData : NULL,
                (unsigned)((rc == 0UL && pPriority != NULL) ? *pPriority : 0U));
        fflush(stderr);
    }
    return rc;
}
