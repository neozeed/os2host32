/*
 * sesmgr.c - thin OS/2 SESMGR ABI veneer.
 *
 * Session Manager semantics/state live in common/sesmgr/os2_sesmgr.c.
 * Win32 process/console/registry mechanics live in
 * common/win32/os2_sesmgr_win32.c.
 */

#include <stdint.h>
#include <string.h>

#include "os2_sesmgr.h"
#include "os2_sesmgr_win32.h"

#ifndef __cdecl
#define __cdecl
#endif

#pragma pack(push, 2)
struct O2StartData {
    unsigned short Length;
    unsigned short Related;
    unsigned short FgBg;
    unsigned short TraceOpt;
    char *PgmTitle;
    char *PgmName;
    unsigned char *PgmInputs;
    unsigned char *TermQ;
    unsigned char *Environment;
    unsigned short InheritOpt;
    unsigned short SessionType;
    char *IconFile;
    uint32_t PgmHandle;
    unsigned short PgmControl;
    unsigned short InitXPos;
    unsigned short InitYPos;
    unsigned short InitXSize;
    unsigned short InitYSize;
    unsigned short Reserved;
    char *ObjectBuffer;
    uint32_t ObjectBuffLen;
};
#pragma pack(pop)

#if defined(UINTPTR_MAX) && UINTPTR_MAX == 0xffffffffUL
typedef char O2StartData_must_be_60_bytes[
    (sizeof(struct O2StartData) == 60U) ? 1 : -1];
#endif

struct O2SessionInfoWire {
    uint32_t taskId;
    uint32_t pid;
    char program[OS2_SESMGR_TEXT_MAX];
    char title[OS2_SESMGR_TEXT_MAX];
};

static struct Os2SesmgrSession g_session;
static int g_session_ready;

static int ensure_session(void)
{
    if (g_session_ready)
        return 1;
    if (!os2_sesmgr_win32_init(&g_session))
        return 0;
    g_session_ready = 1;
    return 1;
}

static void normalize_start_data(const struct O2StartData *sd,
                                 struct Os2SesmgrStartRequest *request)
{
    memset(request, 0, sizeof(*request));
    if (sd == NULL)
        return;
    request->source_length = sd->Length;
    request->related = sd->Related;
    request->fgbg = sd->FgBg;
    request->trace_opt = sd->TraceOpt;
    request->title = sd->PgmTitle;
    request->program = sd->PgmName;
    request->inputs = (const char *)sd->PgmInputs;
    if (sd->Length >= 24U)
        request->term_queue = (const char *)sd->TermQ;
    if (sd->Length >= 28U)
        request->environment = (const char *)sd->Environment;

    request->inherit_opt = OS2_SESMGR_INHERIT_SHELL;
    if (sd->Length >= 30U)
        request->inherit_opt = sd->InheritOpt;

    request->session_type = OS2_SESMGR_TYPE_DEFAULT;
    if (sd->Length >= 32U)
        request->session_type = sd->SessionType;

    if (sd->Length >= 36U)
        request->icon_file = sd->IconFile;
    if (sd->Length >= 40U)
        request->program_handle = sd->PgmHandle;

    request->program_control = OS2_SESMGR_CONTROL_VISIBLE;
    if (sd->Length >= 42U)
        request->program_control = sd->PgmControl;

    if (sd->Length >= 50U) {
        request->init_x = sd->InitXPos;
        request->init_y = sd->InitYPos;
        request->init_cx = sd->InitXSize;
        request->init_cy = sd->InitYSize;
    }
    if (sd->Length >= 52U)
        request->reserved = sd->Reserved;
    if (sd->Length >= 60U) {
        request->object_buffer = sd->ObjectBuffer;
        request->object_buffer_length = sd->ObjectBuffLen;
    }
}

/* SESMGR.5 -- historical Session Manager title update. */
uint32_t __cdecl DosSmSetTitle(uint32_t sessionId, char *title)
{
    if (!ensure_session())
        return OS2_SESMGR_ERROR_NOT_ENOUGH_MEMORY;
    return os2_sesmgr_DosSmSetTitle(&g_session, sessionId, title);
}

/* Private host query used by reconstructed CMD32 PS. */
uint32_t __cdecl O2HostQuerySessions(struct O2SessionInfoWire *entries,
                                     uint32_t capacity,
                                     uint32_t *count)
{
    if (!ensure_session())
        return OS2_SESMGR_ERROR_NOT_ENOUGH_MEMORY;
    return os2_sesmgr_QuerySessions(
        &g_session, (struct Os2SesmgrSessionInfo *)entries, capacity, count);
}

/* SESMGR.8 */
uint32_t __cdecl DosStopSession(uint32_t scope, uint32_t sessionId)
{
    if (!ensure_session())
        return OS2_SESMGR_ERROR_NOT_ENOUGH_MEMORY;
    return os2_sesmgr_DosStopSession(&g_session, scope, sessionId);
}

/* SESMGR.17 */
uint32_t __cdecl DosStartSession(struct O2StartData *sd,
                                 uint32_t *sessionId,
                                 uint32_t *pidOut)
{
    struct Os2SesmgrStartRequest request;
    normalize_start_data(sd, &request);
    if (!ensure_session()) {
        if (sessionId != NULL)
            *sessionId = 0UL;
        if (pidOut != NULL)
            *pidOut = 0UL;
        return OS2_SESMGR_ERROR_NOT_ENOUGH_MEMORY;
    }
    return os2_sesmgr_DosStartSession(&g_session, &request,
                                      sessionId, pidOut);
}

#ifndef OS2_SESMGR_NO_DLLMAIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved)
{
    (void)hinst;
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        if (!ensure_session())
            return FALSE;
    } else if (reason == DLL_PROCESS_DETACH) {
        if (g_session_ready) {
            os2_sesmgr_win32_destroy(&g_session);
            g_session_ready = 0;
        }
    }
    return TRUE;
}
#endif
