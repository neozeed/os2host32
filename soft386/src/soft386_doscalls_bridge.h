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

struct Soft386NativeDoscalls {
    void *opaque;
    void *(*get_proc)(void *opaque, uint32_t ordinal);
    void (*close)(void *opaque);
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

/* Dispatch one 32-bit cdecl DOSCALLS call.  esp points at the guest return
 * address, so argument #1 is at esp+4.  *handled is set when the ordinal belongs
 * to this bridge even if the native export is missing. */
uint32_t soft386_doscalls_bridge_dispatch(
    struct Soft386NativeDoscalls *bridge,
    const struct Soft386GuestMemoryOps *mem,
    uint32_t ordinal, uint32_t esp, int *handled);

#endif
