#ifndef OS2_SESMGR_H
#define OS2_SESMGR_H

/* Backend-neutral OS/2 Session Manager state and semantics.
 *
 * STARTDATA validation, session-id allocation, related-session ownership,
 * shared session metadata, title/query policy, and stop semantics live here.
 * Host process/console creation, liveness, termination and shared-registry
 * storage/locking are supplied by a backend.
 */

#include <stdint.h>
#include <stddef.h>

#ifndef __cdecl
#define __cdecl
#endif

typedef uint32_t Os2SesmgrApiRet;
typedef uint32_t Os2SesmgrU32;
typedef uintptr_t Os2SesmgrNative;

#define OS2_SESMGR_NO_ERROR                       0UL
#define OS2_SESMGR_ERROR_NOT_ENOUGH_MEMORY        8UL
#define OS2_SESMGR_ERROR_INVALID_PARAMETER       87UL
#define OS2_SESMGR_ERROR_INVALID_SESSION_ID     369UL
#define OS2_SESMGR_ERROR_INVALID_CALL           418UL
#define OS2_SESMGR_ERROR_INVALID_STOP_OPTION    458UL
#define OS2_SESMGR_ERROR_PROCESS_NOT_PARENT     460UL
#define OS2_SESMGR_ERROR_RETRY_SUB_ALLOC        463UL

#define OS2_SESMGR_RELATED_INDEPENDENT 0U
#define OS2_SESMGR_RELATED_CHILD       1U
#define OS2_SESMGR_FGBG_FORE           0U
#define OS2_SESMGR_FGBG_BACK           1U
#define OS2_SESMGR_TRACEOPT_NONE       0U
#define OS2_SESMGR_INHERIT_SHELL       0U
#define OS2_SESMGR_INHERIT_PARENT      1U
#define OS2_SESMGR_TYPE_DEFAULT        0U
#define OS2_SESMGR_TYPE_FULLSCREEN     1U
#define OS2_SESMGR_TYPE_WINDOWABLEVIO  2U
#define OS2_SESMGR_TYPE_PM             3U

#define OS2_SESMGR_CONTROL_VISIBLE      0x0000U
#define OS2_SESMGR_CONTROL_INVISIBLE    0x0001U
#define OS2_SESMGR_CONTROL_MAXIMIZE     0x0002U
#define OS2_SESMGR_CONTROL_MINIMIZE     0x0004U
#define OS2_SESMGR_CONTROL_NOAUTOCLOSE  0x0008U
#define OS2_SESMGR_CONTROL_SETPOS       0x8000U
#define OS2_SESMGR_CONTROL_KNOWN_MASK   0x800FU

#define OS2_SESMGR_LOCAL_MAX             64U
#define OS2_SESMGR_SHARED_MAX           128U
#define OS2_SESMGR_TEXT_MAX             260U
#define OS2_SESMGR_REGISTRY_MAGIC 0x4F325352UL
#define OS2_SESMGR_REGISTRY_VERSION       1UL
#define OS2_SESMGR_NATIVE_INVALID ((Os2SesmgrNative)0)

struct Os2SesmgrBackendOps;

struct Os2SesmgrIdentity {
    Os2SesmgrU32 pid;
    Os2SesmgrU32 token_low;
    Os2SesmgrU32 token_high;
};

struct Os2SesmgrStartRequest {
    unsigned short source_length;
    unsigned short related;
    unsigned short fgbg;
    unsigned short trace_opt;
    const char *title;
    const char *program;
    const char *inputs;
    const char *term_queue;
    const char *environment;
    unsigned short inherit_opt;
    unsigned short session_type;
    const char *icon_file;
    Os2SesmgrU32 program_handle;
    unsigned short program_control;
    unsigned short init_x;
    unsigned short init_y;
    unsigned short init_cx;
    unsigned short init_cy;
    unsigned short reserved;
    char *object_buffer;
    Os2SesmgrU32 object_buffer_length;
};

struct Os2SesmgrSessionInfo {
    Os2SesmgrU32 task_id;
    Os2SesmgrU32 pid;
    char program[OS2_SESMGR_TEXT_MAX];
    char title[OS2_SESMGR_TEXT_MAX];
};

struct Os2SesmgrSharedRecord {
    Os2SesmgrU32 in_use;
    Os2SesmgrU32 session_id;
    struct Os2SesmgrIdentity process;
    struct Os2SesmgrIdentity owner;
    char program[OS2_SESMGR_TEXT_MAX];
    char title[OS2_SESMGR_TEXT_MAX];
};

struct Os2SesmgrRegistry {
    Os2SesmgrU32 magic;
    Os2SesmgrU32 version;
    Os2SesmgrU32 next_session_id;
    struct Os2SesmgrSharedRecord sessions[OS2_SESMGR_SHARED_MAX];
};

struct Os2SesmgrLaunch {
    Os2SesmgrU32 pid;
    Os2SesmgrNative process;
    Os2SesmgrNative initial_thread;
    Os2SesmgrNative group;
};

struct Os2SesmgrOwnedSession {
    Os2SesmgrU32 session_id;
    Os2SesmgrU32 pid;
    Os2SesmgrNative process;
    Os2SesmgrNative group;
};

struct Os2SesmgrSession {
    void *backend_opaque;
    const struct Os2SesmgrBackendOps *backend;
    struct Os2SesmgrOwnedSession owned[OS2_SESMGR_LOCAL_MAX];
};

void os2_sesmgr_session_init(struct Os2SesmgrSession *session,
                             void *backend_opaque,
                             const struct Os2SesmgrBackendOps *backend);
void os2_sesmgr_session_destroy(struct Os2SesmgrSession *session);

Os2SesmgrApiRet os2_sesmgr_DosSmSetTitle(struct Os2SesmgrSession *session,
                                         Os2SesmgrU32 session_id,
                                         const char *title);
Os2SesmgrApiRet os2_sesmgr_DosStopSession(struct Os2SesmgrSession *session,
                                          Os2SesmgrU32 scope,
                                          Os2SesmgrU32 session_id);
Os2SesmgrApiRet os2_sesmgr_DosStartSession(struct Os2SesmgrSession *session,
                                           struct Os2SesmgrStartRequest *request,
                                           Os2SesmgrU32 *session_id,
                                           Os2SesmgrU32 *pid_out);
Os2SesmgrApiRet os2_sesmgr_QuerySessions(struct Os2SesmgrSession *session,
                                         struct Os2SesmgrSessionInfo *entries,
                                         Os2SesmgrU32 capacity,
                                         Os2SesmgrU32 *count);

void os2_sesmgr_set_object_buffer(struct Os2SesmgrStartRequest *request,
                                  const char *text);

#endif
