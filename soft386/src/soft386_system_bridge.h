#ifndef SOFT386_SYSTEM_BRIDGE_H
#define SOFT386_SYSTEM_BRIDGE_H

#include <stdint.h>
#include "soft386_doscalls_bridge.h"

struct Soft386NativeModule {
    void *opaque;
    void *(*get_proc)(void *opaque, uint32_t ordinal);
    void (*close)(void *opaque);
    int trace;
    int loaded;
    const char *label;
};

struct Soft386SystemBridge {
    struct Soft386NativeModule vio;
    struct Soft386NativeModule kbd;
    struct Soft386NativeModule ses;
    int trace;
};

struct Soft386C386ApiDesc {
    const char *module;
    const char *name;
    uint32_t ordinal;
    uint8_t count;
    uint8_t width[7];
};

/* Current native VIO/KBD ABI catalogue, used both by the C/386 helper lowering
 * pass and by the packed-frame bridge dispatcher. */
int soft386_c386_desc_index(const char *module, uint32_t ordinal);
const struct Soft386C386ApiDesc *soft386_c386_desc_by_id(uint32_t id);
uint32_t soft386_c386_desc_count(void);

#define SOFT386_SYS_VIO 0x01u
#define SOFT386_SYS_KBD 0x02u
#define SOFT386_SYS_SES 0x04u
#define SOFT386_SYS_ALL (SOFT386_SYS_VIO|SOFT386_SYS_KBD|SOFT386_SYS_SES)

int soft386_system_bridge_open_mask(struct Soft386SystemBridge *bridge, uint32_t mask,
                               const char *vio_path, int vio_required,
                               const char *kbd_path, int kbd_required,
                               const char *ses_path, int ses_required,
                               int trace);

int soft386_system_bridge_open(struct Soft386SystemBridge *bridge,
                               const char *vio_path, int vio_required,
                               const char *kbd_path, int kbd_required,
                               const char *ses_path, int ses_required,
                               int trace);
void soft386_system_bridge_close(struct Soft386SystemBridge *bridge);

/* Test/provider injection. module is "VIOCALLS", "KBDCALLS" or "SESMGR". */
int soft386_system_bridge_set_provider(struct Soft386SystemBridge *bridge,
                                       const char *module,
                                       void *opaque,
                                       void *(*get_proc)(void *, uint32_t),
                                       void (*close_fn)(void *), int trace);

/* Flat 32-bit cdecl dispatch: esp points at the guest return address. */
uint32_t soft386_system_bridge_dispatch32(
    struct Soft386SystemBridge *bridge,
    const struct Soft386GuestMemoryOps *mem,
    const char *module, uint32_t ordinal, uint32_t esp, int *handled);

/* Packed Microsoft C/386 far16 migration frame.  desc_id is 1-based. */
uint32_t soft386_system_bridge_dispatch_c386(
    struct Soft386SystemBridge *bridge,
    const struct Soft386GuestMemoryOps *mem,
    uint32_t desc_id, uint32_t esp, int *handled);

#endif
