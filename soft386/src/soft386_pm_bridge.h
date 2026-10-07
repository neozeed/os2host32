#ifndef SOFT386_PM_BRIDGE_H
#define SOFT386_PM_BRIDGE_H

#include "soft386_system_bridge.h"

/* This entire object belongs to one process vessel. Native handles and function
 * pointers stay here; the CPU sees tokens and guest virtual addresses only. */
enum Soft386PmModule { S386_PMWIN, S386_PMGPI, S386_PMCTLS, S386_MSG,
                      S386_PMSHAPI, S386_HELPMGR, S386_PM_COUNT };
enum Soft386PmKind { S386_HWND=1, S386_HAB, S386_HMQ, S386_HPS, S386_HDC,
                    S386_HBITMAP, S386_HRGN, S386_HPOINTER, S386_HACCEL, S386_HENUM,
                    S386_HINI, S386_HSWITCH, S386_HHELP };
#define S386_PM_HANDLES 1024
#define S386_PM_MESSAGES 128
#define S386_PM_PROCS 64
#define S386_PM_RESOURCES 256
#define S386_PM_OLDPROCS 128
struct Soft386PmOldProc {
    uintptr_t native;
    uint32_t entry, hwnd, tid;
};
struct Soft386PmHandle {
    uint32_t native, token, kind, tid, proc, aux, width, height, stride, bits, parent;
    int pending_show, lifetime, owned;
};
struct Soft386PmMessage {
    uint8_t wire[28], native[28];
    uint16_t token;
    uint32_t proc, tid;
    uint64_t order;
    int used, posted, removed;
};
struct Soft386PmResource { uint32_t module; uint16_t type,id; uint32_t size; void *data; int registered; };
struct Soft386PmBridge {
    struct Soft386NativeModule module[S386_PM_COUNT];
    const char *paths[S386_PM_COUNT];
    void *opaque;
    uint32_t (*tid)(void *);
    int (*invoke)(void *, uint32_t tid, uint32_t entry,
                  const uint32_t *args, uint32_t argc, uint32_t *result);
    uint32_t (*veneer)(void *, uint32_t slot);
    int (*code_address)(void *, uint32_t entry);
    int (*scheduler_yield)(void *);
    int (*thread_hostcall)(void *, uint32_t tid);
    struct Soft386PmOldProc oldprocs[S386_PM_OLDPROCS];
    struct Soft386PmHandle handles[S386_PM_HANDLES];
    struct Soft386PmMessage messages[S386_PM_MESSAGES];
    struct Soft386PmResource resources[S386_PM_RESOURCES];
    uint32_t serial, queue_tid, owner_tid, nresources, dialog_param, dialog_window, pid;
    uint16_t message_serial;
    uint64_t enqueue_serial;
    int trace, disabled, waiting, closing;
};
void soft386_pm_init(struct Soft386PmBridge *, void *, uint32_t (*)(void *),
    int (*)(void *,uint32_t,uint32_t,const uint32_t *,uint32_t,uint32_t *));
int soft386_pm_open(struct Soft386PmBridge *, unsigned module);
int soft386_pm_export(struct Soft386PmBridge *, unsigned module, uint32_t ordinal);
void soft386_pm_close(struct Soft386PmBridge *);
void soft386_pm_quiesce_process(struct Soft386PmBridge *);
void soft386_pm_set_provider(struct Soft386PmBridge *, unsigned module, void *,
    void *(*)(void *,uint32_t));
void soft386_pm_set_scheduler_yield(struct Soft386PmBridge *, int (*)(void *));
void soft386_pm_set_thread_hostcall(struct Soft386PmBridge *, int (*)(void *,uint32_t));
uint32_t soft386_pm_dispatch(struct Soft386PmBridge *, const struct Soft386GuestMemoryOps *,
    unsigned module, uint32_t ordinal, uint32_t esp, int *handled);
int soft386_pm_add_resource(struct Soft386PmBridge *,uint32_t,uint16_t,uint16_t,const void *,uint32_t);
uint32_t soft386_pm_named_ordinal(unsigned module,const char *name);
const char *soft386_pm_api_name(unsigned module,uint32_t ordinal);
uint32_t soft386_pm_call_proc(struct Soft386PmBridge *, const struct Soft386GuestMemoryOps *,
    uint32_t slot, uint32_t esp);
#endif
