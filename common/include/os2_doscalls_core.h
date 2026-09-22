#ifndef OS2_DOSCALLS_CORE_H
#define OS2_DOSCALLS_CORE_H

#include "os2_personality.h"

/* Loader-neutral implementations used by both DOSCALLS.DLL and WHP. */
os2_api_ret_t os2_core_DosGetDateTime(
    struct Os2PersonalityContext *context,
    os2_addr32_t datetime_address);

os2_api_ret_t os2_core_DosQueryHType(
    struct Os2PersonalityContext *context,
    os2_handle32_t handle,
    os2_addr32_t type_address,
    os2_addr32_t attributes_address);

os2_api_ret_t os2_core_DosSetFilePtr(
    struct Os2PersonalityContext *context,
    os2_handle32_t handle,
    os2_long32_t distance,
    uint32_t method,
    os2_addr32_t new_position_address);

os2_api_ret_t os2_core_DosWrite(
    struct Os2PersonalityContext *context,
    os2_handle32_t handle,
    os2_addr32_t buffer_address,
    uint32_t count,
    os2_addr32_t actual_address);

os2_api_ret_t os2_core_DosAllocMem(
    struct Os2PersonalityContext *context,
    os2_addr32_t base_address,
    uint32_t size,
    uint32_t flags,
    uint32_t reserved);

os2_api_ret_t os2_core_DosFreeMem(
    struct Os2PersonalityContext *context,
    os2_addr32_t base);

os2_api_ret_t os2_core_DosSetMem(
    struct Os2PersonalityContext *context,
    os2_addr32_t base,
    uint32_t size,
    uint32_t flags);

os2_api_ret_t os2_core_DosQuerySysInfo(
    struct Os2PersonalityContext *context,
    uint32_t first,
    uint32_t last,
    os2_addr32_t buffer_address,
    uint32_t buffer_size);

#endif
