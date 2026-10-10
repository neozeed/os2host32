#ifndef OS2_PERSONALITY_H
#define OS2_PERSONALITY_H

/*
 * Loader-neutral OS/2 personality interface.
 *
 * OS/2 applications use a 32-bit linear address space.  The native Win32
 * loader can normally treat an OS/2 address as a process pointer, while the
 * Win64/WHP loader must translate it into its separately mapped guest RAM.
 * Shared API implementations therefore exchange explicit 32-bit addresses
 * through this interface instead of dereferencing application pointers.
 */

#include <stdint.h>

typedef uint32_t os2_api_ret_t;
typedef uint32_t os2_addr32_t;
typedef uint32_t os2_handle32_t;
typedef int32_t  os2_long32_t;

#define OS2_PERSONALITY_NO_ERROR                 0u
#define OS2_PERSONALITY_ERROR_INVALID_FUNCTION   1u
#define OS2_PERSONALITY_ERROR_INVALID_HANDLE     6u
#define OS2_PERSONALITY_ERROR_NOT_ENOUGH_MEMORY  8u
#define OS2_PERSONALITY_ERROR_INVALID_PARAMETER 87u

#define OS2_MEMORY_READ  0x00000001u
#define OS2_MEMORY_WRITE 0x00000002u

struct Os2LocalDateTime {
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint8_t hundredths;
    uint8_t day;
    uint8_t month;
    uint16_t year;
    int16_t timezone_minutes_west;
    uint8_t weekday;
};

/* Backend byte counts; the shared core clamps them to OS/2 ULONG fields.
 * Native hosts can supply host budgets; a jar supplies its own arena budget.
 * available_virtual is the backend's maximum single-allocation budget. */
struct Os2MemoryStatus {
    uint64_t total_physical;
    uint64_t available_physical;
    uint64_t available_commit;
    uint64_t available_virtual;
};

struct Os2PersonalityOps {
    os2_api_ret_t (*validate_memory)(void *opaque, os2_addr32_t address,
                                    uint32_t length, uint32_t access);
    os2_api_ret_t (*write_memory)(void *opaque, os2_addr32_t address,
                                 const void *source, uint32_t length);
    os2_api_ret_t (*map_read_memory)(void *opaque, os2_addr32_t address,
                                    uint32_t length,
                                    const void **host_pointer);

    os2_api_ret_t (*query_handle_type)(void *opaque, os2_handle32_t handle,
                                      uint32_t *type, uint32_t *attributes);
    os2_api_ret_t (*write_handle)(void *opaque, os2_handle32_t handle,
                                 const void *buffer, uint32_t length,
                                 uint32_t *actual);
    os2_api_ret_t (*set_file_pointer)(void *opaque, os2_handle32_t handle,
                                     os2_long32_t distance, uint32_t method,
                                     uint32_t *new_position);

    os2_api_ret_t (*allocate_memory)(void *opaque, uint32_t size,
                                    uint32_t flags, uint32_t reserved,
                                    os2_addr32_t *base);
    os2_api_ret_t (*free_memory)(void *opaque, os2_addr32_t base);
    os2_api_ret_t (*set_memory)(void *opaque, os2_addr32_t base,
                               uint32_t size, uint32_t flags);

    os2_api_ret_t (*query_local_datetime)(
        void *opaque, struct Os2LocalDateTime *value);
    uint64_t (*monotonic_milliseconds)(void *opaque);
    os2_api_ret_t (*query_memory_status)(void *opaque,
                                        struct Os2MemoryStatus *value);
};

struct Os2NlsState;

struct Os2PersonalityContext {
    void *opaque;
    const struct Os2PersonalityOps *ops;
    struct Os2NlsState *nls;
};

void os2_personality_context_init(struct Os2PersonalityContext *context,
                                  void *opaque,
                                  const struct Os2PersonalityOps *ops);
void os2_personality_context_set_nls(struct Os2PersonalityContext *context,
                                     struct Os2NlsState *nls);

#endif
