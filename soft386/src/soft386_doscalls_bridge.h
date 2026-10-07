#ifndef SOFT386_DOSCALLS_BRIDGE_H
#define SOFT386_DOSCALLS_BRIDGE_H

#include <stdint.h>

struct Soft386GuestMemoryOps {
    void *opaque;
    int (*valid)(void *opaque, uint32_t address, uint32_t length, int write_access);
    int (*read)(void *opaque, uint32_t address, void *dst, uint32_t length);
    int (*write)(void *opaque, uint32_t address, const void *src, uint32_t length);
    int (*read_cstr)(void *opaque, uint32_t address, char *dst, uint32_t capacity);
    uint32_t (*read_u32)(void *opaque, uint32_t address);
};

#define SOFT386_DOS_ASYNC_SLOTS 32u
#define SOFT386_DOS_ABI_SCALAR  0x0001u
#define SOFT386_DOS_MAY_BLOCK   0x0002u
#define SOFT386_DOS_ABI_MARSHALLED 0x0004u

struct Soft386NativeDoscalls {
    void *opaque;
    void *(*get_proc)(void *opaque, uint32_t ordinal);
    void (*close)(void *opaque);
    void *async_jobs[SOFT386_DOS_ASYNC_SLOTS];
    uint32_t async_tids[SOFT386_DOS_ASYNC_SLOTS];
    int trace;
    int loaded;
};

/* Windows implementation loads DOSCALLS.DLL by ordinal.  On non-Windows hosts
 * this returns zero and leaves the bridge disabled; tests may inject a fake
 * provider with soft386_doscalls_bridge_set_provider(). */
int soft386_doscalls_bridge_open(struct Soft386NativeDoscalls *bridge,
                                 const char *path, int required, int trace);
void soft386_doscalls_bridge_close(struct Soft386NativeDoscalls *bridge);
void soft386_doscalls_bridge_set_provider(struct Soft386NativeDoscalls *bridge,
                                          void *opaque,
                                          void *(*get_proc)(void *, uint32_t),
                                          void (*close_fn)(void *),
                                          int trace);

/* True only for APIs whose OS/2-visible state may safely live in the native
 * DOSCALLS.DLL session.  Kernel-owned memory/thread/sync/module APIs are never
 * routed here. */
int soft386_doscalls_bridge_ordinal(uint32_t ordinal);
int soft386_doscalls_bridge_export(struct Soft386NativeDoscalls *bridge, uint32_t ordinal);

/* ABI metadata is intentionally separate from OS/2 semantics.  Scalar entries
 * need no API-specific implementation in Soft386; flags only describe how a
 * native call may safely cross the machine boundary. */
unsigned soft386_doscalls_bridge_abi(uint32_t ordinal, unsigned *nargs);

/* Dispatch one 32-bit cdecl DOSCALLS call.  esp points at the guest return
 * address, so argument #1 is at esp+4.  *handled is set when the ordinal belongs
 * to this bridge even if the native export is missing. */
uint32_t soft386_doscalls_bridge_dispatch(
    struct Soft386NativeDoscalls *bridge,
    const struct Soft386GuestMemoryOps *mem,
    uint32_t ordinal, uint32_t esp, int *handled);

/* Generic worker path for scalar APIs marked MAY_BLOCK.  Arguments are
 * snapshotted before the worker starts; worker threads never touch guest RAM. */
uint32_t soft386_doscalls_bridge_dispatch_async_scalar(
    struct Soft386NativeDoscalls *bridge,
    const struct Soft386GuestMemoryOps *mem,
    uint32_t ordinal, uint32_t esp, unsigned slot, uint32_t tid,
    int *waiting, int *handled);

#endif
