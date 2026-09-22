/* R8 guest-owned synchronisation objects. Never use a host thread's mutex. */
#define MAX_MUTEXES 64u
#define MAX_QUEUES 16u
#define QUEUE_DEPTH 256u
#define SYNC_GENERATION_MAX 0x003fffffu
#define MUTEX_TAG 0x40000000u
#define QUEUE_TAG 0x80000000u
#define OS2_ERROR_TOO_MANY_SEM_REQUESTS 103u
#define OS2_ERROR_SEM_OWNER_DIED 105u
#define OS2_ERROR_NOT_OWNER 288u
#define OS2_ERROR_SEM_BUSY 301u
#define OS2_ERROR_QUE_DUPLICATE 332u
#define OS2_ERROR_QUE_ELEMENT_NOT_EXIST 333u
#define OS2_ERROR_QUE_NO_MEMORY 334u
#define OS2_ERROR_QUE_INVALID_NAME 335u
#define OS2_ERROR_QUE_INVALID_PRIORITY 336u
#define OS2_ERROR_QUE_INVALID_HANDLE 337u
#define OS2_ERROR_QUE_EMPTY 342u
#define OS2_ERROR_QUE_NAME_NOT_EXIST 343u
#define OS2_ERROR_QUE_UNABLE_TO_ADD 346u
#define OS2_ERROR_QUE_INVALID_WAIT 433u
struct GuestMutex {
    uint32_t generation, refs, owner, count;
    int used, abandoned;
    char name[EVENT_NAME_MAX + 1];
};
struct QueueEntry {
    uint32_t token, request, length, data, priority;
};
struct GuestQueue {
    uint32_t generation, refs, discipline, count, next_token, notify_event;
    int used;
    char name[EVENT_NAME_MAX + 1];
    struct QueueEntry entries[QUEUE_DEPTH];
};
