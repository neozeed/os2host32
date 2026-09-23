/*
 * Loader-neutral DOSCALLS semantics shared by the native DLL personality and
 * the Win64 Windows Hypervisor Platform loader.
 */

#include "os2_doscalls_core.h"

#include <stddef.h>

#define OS2_U32_MAX 0xffffffffu

void os2_personality_context_init(struct Os2PersonalityContext *context,
                                  void *opaque,
                                  const struct Os2PersonalityOps *ops)
{
    if (context == NULL)
        return;
    context->opaque = opaque;
    context->ops = ops;
    context->nls = NULL;
}

void os2_personality_context_set_nls(struct Os2PersonalityContext *context,
                                     struct Os2NlsState *nls)
{
    if (context != NULL)
        context->nls = nls;
}

static int context_has_memory(const struct Os2PersonalityContext *context)
{
    return context != NULL && context->ops != NULL &&
           context->ops->validate_memory != NULL &&
           context->ops->write_memory != NULL;
}

static os2_api_ret_t validate_memory(
    struct Os2PersonalityContext *context,
    os2_addr32_t address,
    uint32_t length,
    uint32_t access)
{
    if (!context_has_memory(context))
        return OS2_PERSONALITY_ERROR_INVALID_FUNCTION;
    if (length != 0u && address == 0u)
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    if (length != 0u && address > OS2_U32_MAX - (length - 1u))
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    return context->ops->validate_memory(context->opaque, address,
                                         length, access);
}

static os2_api_ret_t write_u32(struct Os2PersonalityContext *context,
                               os2_addr32_t address,
                               uint32_t value)
{
    os2_api_ret_t rc;
    rc = validate_memory(context, address, 4u, OS2_MEMORY_WRITE);
    if (rc != OS2_PERSONALITY_NO_ERROR)
        return rc;
    return context->ops->write_memory(context->opaque, address,
                                      &value, 4u);
}

os2_api_ret_t os2_core_DosGetDateTime(
    struct Os2PersonalityContext *context,
    os2_addr32_t datetime_address)
{
    struct Os2LocalDateTime value;
    os2_api_ret_t rc;
    unsigned char packed[12];
    uint16_t timezone;

    if (context == NULL || context->ops == NULL ||
        context->ops->query_local_datetime == NULL)
        return OS2_PERSONALITY_ERROR_INVALID_FUNCTION;
    rc = validate_memory(context, datetime_address, 12u, OS2_MEMORY_WRITE);
    if (rc != OS2_PERSONALITY_NO_ERROR)
        return rc;

    value.hours = 0u;
    value.minutes = 0u;
    value.seconds = 0u;
    value.hundredths = 0u;
    value.day = 0u;
    value.month = 0u;
    value.year = 0u;
    value.timezone_minutes_west = 0;
    value.weekday = 0u;
    rc = context->ops->query_local_datetime(context->opaque, &value);
    if (rc != OS2_PERSONALITY_NO_ERROR)
        return rc;

    timezone = (uint16_t)value.timezone_minutes_west;
    packed[0] = value.hours;
    packed[1] = value.minutes;
    packed[2] = value.seconds;
    packed[3] = value.hundredths;
    packed[4] = value.day;
    packed[5] = value.month;
    packed[6] = (unsigned char)(value.year & 0xffu);
    packed[7] = (unsigned char)(value.year >> 8);
    packed[8] = (unsigned char)(timezone & 0xffu);
    packed[9] = (unsigned char)(timezone >> 8);
    packed[10] = value.weekday;
    packed[11] = 0u;

    return context->ops->write_memory(context->opaque, datetime_address,
                                      packed, 12u);
}

os2_api_ret_t os2_core_DosQueryHType(
    struct Os2PersonalityContext *context,
    os2_handle32_t handle,
    os2_addr32_t type_address,
    os2_addr32_t attributes_address)
{
    os2_api_ret_t rc;
    uint32_t type;
    uint32_t attributes;

    if (context == NULL || context->ops == NULL ||
        context->ops->query_handle_type == NULL)
        return OS2_PERSONALITY_ERROR_INVALID_FUNCTION;

    rc = validate_memory(context, type_address, 4u, OS2_MEMORY_WRITE);
    if (rc != OS2_PERSONALITY_NO_ERROR)
        return rc;
    rc = validate_memory(context, attributes_address, 4u, OS2_MEMORY_WRITE);
    if (rc != OS2_PERSONALITY_NO_ERROR)
        return rc;

    type = 0u;
    attributes = 0u;
    rc = context->ops->query_handle_type(context->opaque, handle,
                                         &type, &attributes);
    if (rc != OS2_PERSONALITY_NO_ERROR)
        return rc;
    rc = context->ops->write_memory(context->opaque, type_address,
                                    &type, 4u);
    if (rc != OS2_PERSONALITY_NO_ERROR)
        return rc;
    return context->ops->write_memory(context->opaque, attributes_address,
                                      &attributes, 4u);
}

os2_api_ret_t os2_core_DosSetFilePtr(
    struct Os2PersonalityContext *context,
    os2_handle32_t handle,
    os2_long32_t distance,
    uint32_t method,
    os2_addr32_t new_position_address)
{
    os2_api_ret_t rc;
    uint32_t new_position;

    if (context == NULL || context->ops == NULL ||
        context->ops->set_file_pointer == NULL)
        return OS2_PERSONALITY_ERROR_INVALID_FUNCTION;
    if (method > 2u)
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    rc = validate_memory(context, new_position_address, 4u,
                         OS2_MEMORY_WRITE);
    if (rc != OS2_PERSONALITY_NO_ERROR)
        return rc;

    new_position = 0u;
    rc = context->ops->set_file_pointer(context->opaque, handle, distance,
                                        method, &new_position);
    if (rc != OS2_PERSONALITY_NO_ERROR)
        return rc;
    return context->ops->write_memory(context->opaque,
                                      new_position_address,
                                      &new_position, 4u);
}

os2_api_ret_t os2_core_DosWrite(
    struct Os2PersonalityContext *context,
    os2_handle32_t handle,
    os2_addr32_t buffer_address,
    uint32_t count,
    os2_addr32_t actual_address)
{
    os2_api_ret_t rc;
    const void *buffer;
    uint32_t actual;

    if (context == NULL || context->ops == NULL ||
        context->ops->write_handle == NULL ||
        context->ops->map_read_memory == NULL)
        return OS2_PERSONALITY_ERROR_INVALID_FUNCTION;

    rc = validate_memory(context, actual_address, 4u, OS2_MEMORY_WRITE);
    if (rc != OS2_PERSONALITY_NO_ERROR)
        return rc;

    buffer = NULL;
    if (count != 0u) {
        rc = validate_memory(context, buffer_address, count,
                             OS2_MEMORY_READ);
        if (rc != OS2_PERSONALITY_NO_ERROR)
            return rc;
        rc = context->ops->map_read_memory(context->opaque,
                                           buffer_address, count, &buffer);
        if (rc != OS2_PERSONALITY_NO_ERROR)
            return rc;
        if (buffer == NULL)
            return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    }

    actual = 0u;
    rc = context->ops->write_handle(context->opaque, handle, buffer,
                                    count, &actual);
    {
        os2_api_ret_t write_rc;
        write_rc = context->ops->write_memory(context->opaque, actual_address,
                                              &actual, 4u);
        if (write_rc != OS2_PERSONALITY_NO_ERROR)
            return write_rc;
    }
    return rc;
}

os2_api_ret_t os2_core_DosAllocMem(
    struct Os2PersonalityContext *context,
    os2_addr32_t base_address,
    uint32_t size,
    uint32_t flags,
    uint32_t reserved)
{
    os2_api_ret_t rc;
    os2_addr32_t base;

    if (context == NULL || context->ops == NULL ||
        context->ops->allocate_memory == NULL)
        return OS2_PERSONALITY_ERROR_INVALID_FUNCTION;
    if (size == 0u)
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    rc = validate_memory(context, base_address, 4u, OS2_MEMORY_WRITE);
    if (rc != OS2_PERSONALITY_NO_ERROR)
        return rc;

    base = 0u;
    rc = context->ops->allocate_memory(context->opaque, size, flags,
                                       reserved, &base);
    if (rc != OS2_PERSONALITY_NO_ERROR)
        return rc;
    if (base == 0u)
        return OS2_PERSONALITY_ERROR_NOT_ENOUGH_MEMORY;

    rc = context->ops->write_memory(context->opaque, base_address,
                                    &base, 4u);
    if (rc != OS2_PERSONALITY_NO_ERROR) {
        if (context->ops->free_memory != NULL)
            (void)context->ops->free_memory(context->opaque, base);
        return rc;
    }
    return OS2_PERSONALITY_NO_ERROR;
}

os2_api_ret_t os2_core_DosFreeMem(
    struct Os2PersonalityContext *context,
    os2_addr32_t base)
{
    if (context == NULL || context->ops == NULL ||
        context->ops->free_memory == NULL)
        return OS2_PERSONALITY_ERROR_INVALID_FUNCTION;
    if (base == 0u)
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    return context->ops->free_memory(context->opaque, base);
}

os2_api_ret_t os2_core_DosSetMem(
    struct Os2PersonalityContext *context,
    os2_addr32_t base,
    uint32_t size,
    uint32_t flags)
{
    if (context == NULL || context->ops == NULL ||
        context->ops->set_memory == NULL)
        return OS2_PERSONALITY_ERROR_INVALID_FUNCTION;
    if (base == 0u || size == 0u)
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    return context->ops->set_memory(context->opaque, base, size, flags);
}

os2_api_ret_t os2_core_DosQuerySysInfo(
    struct Os2PersonalityContext *context,
    uint32_t first,
    uint32_t last,
    os2_addr32_t buffer_address,
    uint32_t buffer_size)
{
    os2_api_ret_t rc;
    uint32_t count;
    uint32_t index;
    os2_addr32_t output;

    if (context == NULL || context->ops == NULL)
        return OS2_PERSONALITY_ERROR_INVALID_FUNCTION;
    if (first == 0u || last < first)
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;

    count = last - first + 1u;
    if (count > OS2_U32_MAX / 4u || buffer_size < count * 4u)
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    rc = validate_memory(context, buffer_address, count * 4u,
                         OS2_MEMORY_WRITE);
    if (rc != OS2_PERSONALITY_NO_ERROR)
        return rc;

    output = buffer_address;
    for (index = first; index <= last; ++index) {
        uint32_t value;
        switch (index) {
        case 10u:
            value = 4096u;
            break;
        case 11u:
            value = 20u;
            break;
        case 12u:
        case 13u:
            value = 0u;
            break;
        case 14u:
            if (context->ops->monotonic_milliseconds != NULL)
                value = (uint32_t)context->ops->monotonic_milliseconds(
                    context->opaque);
            else
                value = 0u;
            break;
        default:
            value = 0u;
            break;
        }
        rc = write_u32(context, output, value);
        if (rc != OS2_PERSONALITY_NO_ERROR)
            return rc;
        output += 4u;
        if (index == OS2_U32_MAX)
            break;
    }
    return OS2_PERSONALITY_NO_ERROR;
}
