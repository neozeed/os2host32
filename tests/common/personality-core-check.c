#include "os2_api_catalog.h"
#include "os2_doscalls_core.h"

#include <stdio.h>
#include <string.h>

#define MOCK_MEMORY_SIZE 65536u

struct MockBackend {
    unsigned char memory[MOCK_MEMORY_SIZE];
    unsigned char output[256];
    uint32_t output_length;
    uint32_t file_position;
    uint32_t allocated_base;
    uint32_t allocated_size;
    int allocation_live;
    uint32_t set_base;
    uint32_t set_size;
    uint32_t set_flags;
    os2_addr32_t fail_write_address;
    int allocation_returns_zero;
};

static os2_api_ret_t mock_validate(void *opaque, os2_addr32_t address,
                                   uint32_t length, uint32_t access)
{
    struct MockBackend *mock;
    (void)access;
    mock = (struct MockBackend *)opaque;
    if (mock == NULL || (length != 0u && address == 0u))
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    if (address >= MOCK_MEMORY_SIZE || length > MOCK_MEMORY_SIZE - address)
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    return OS2_PERSONALITY_NO_ERROR;
}

static os2_api_ret_t mock_write_memory(void *opaque, os2_addr32_t address,
                                       const void *source, uint32_t length)
{
    struct MockBackend *mock;
    mock = (struct MockBackend *)opaque;
    if (mock_validate(opaque, address, length, OS2_MEMORY_WRITE) != 0u)
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    if (mock->fail_write_address != 0u &&
        address == mock->fail_write_address)
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    if (length != 0u)
        memcpy(mock->memory + address, source, (size_t)length);
    return OS2_PERSONALITY_NO_ERROR;
}

static os2_api_ret_t mock_map_read(void *opaque, os2_addr32_t address,
                                   uint32_t length,
                                   const void **host_pointer)
{
    struct MockBackend *mock;
    mock = (struct MockBackend *)opaque;
    if (host_pointer == NULL ||
        mock_validate(opaque, address, length, OS2_MEMORY_READ) != 0u)
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    *host_pointer = mock->memory + address;
    return OS2_PERSONALITY_NO_ERROR;
}

static os2_api_ret_t mock_query_type(void *opaque, os2_handle32_t handle,
                                     uint32_t *type, uint32_t *attributes)
{
    (void)opaque;
    if (handle != 1u)
        return OS2_PERSONALITY_ERROR_INVALID_HANDLE;
    *type = 1u;
    *attributes = 0u;
    return OS2_PERSONALITY_NO_ERROR;
}

static os2_api_ret_t mock_write_handle(void *opaque, os2_handle32_t handle,
                                       const void *buffer, uint32_t length,
                                       uint32_t *actual)
{
    struct MockBackend *mock;
    uint32_t room;
    mock = (struct MockBackend *)opaque;
    if (handle != 1u)
        return OS2_PERSONALITY_ERROR_INVALID_HANDLE;
    room = (uint32_t)sizeof(mock->output) - mock->output_length;
    if (length > room)
        length = room;
    if (length != 0u)
        memcpy(mock->output + mock->output_length, buffer, (size_t)length);
    mock->output_length += length;
    *actual = length;
    return OS2_PERSONALITY_NO_ERROR;
}

static os2_api_ret_t mock_seek(void *opaque, os2_handle32_t handle,
                               os2_long32_t distance, uint32_t method,
                               uint32_t *new_position)
{
    struct MockBackend *mock;
    mock = (struct MockBackend *)opaque;
    if (handle != 1u)
        return OS2_PERSONALITY_ERROR_INVALID_HANDLE;
    if (method == 0u)
        mock->file_position = (uint32_t)distance;
    else if (method == 1u)
        mock->file_position += (uint32_t)distance;
    else
        mock->file_position = 1000u + (uint32_t)distance;
    *new_position = mock->file_position;
    return OS2_PERSONALITY_NO_ERROR;
}

static os2_api_ret_t mock_allocate(void *opaque, uint32_t size,
                                   uint32_t flags, uint32_t reserved,
                                   os2_addr32_t *base)
{
    struct MockBackend *mock;
    (void)flags;
    (void)reserved;
    mock = (struct MockBackend *)opaque;
    if (mock->allocation_live)
        return OS2_PERSONALITY_ERROR_NOT_ENOUGH_MEMORY;
    if (mock->allocation_returns_zero) {
        *base = 0u;
        return OS2_PERSONALITY_NO_ERROR;
    }
    mock->allocated_base = 0x2000u;
    mock->allocated_size = size;
    mock->allocation_live = 1;
    *base = mock->allocated_base;
    return OS2_PERSONALITY_NO_ERROR;
}

static os2_api_ret_t mock_free(void *opaque, os2_addr32_t base)
{
    struct MockBackend *mock;
    mock = (struct MockBackend *)opaque;
    if (!mock->allocation_live || base != mock->allocated_base)
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    mock->allocation_live = 0;
    return OS2_PERSONALITY_NO_ERROR;
}

static os2_api_ret_t mock_set_memory(void *opaque, os2_addr32_t base,
                                     uint32_t size, uint32_t flags)
{
    struct MockBackend *mock;
    mock = (struct MockBackend *)opaque;
    mock->set_base = base;
    mock->set_size = size;
    mock->set_flags = flags;
    return OS2_PERSONALITY_NO_ERROR;
}

static os2_api_ret_t mock_datetime(void *opaque,
                                        struct Os2LocalDateTime *value)
{
    (void)opaque;
    value->hours = 23u;
    value->minutes = 58u;
    value->seconds = 57u;
    value->hundredths = 42u;
    value->day = 31u;
    value->month = 12u;
    value->year = 1999u;
    value->timezone_minutes_west = -60;
    value->weekday = 5u;
    return OS2_PERSONALITY_NO_ERROR;
}

static uint64_t mock_milliseconds(void *opaque)
{
    (void)opaque;
    return 123456u;
}

static const struct Os2PersonalityOps mock_ops = {
    mock_validate,
    mock_write_memory,
    mock_map_read,
    mock_query_type,
    mock_write_handle,
    mock_seek,
    mock_allocate,
    mock_free,
    mock_set_memory,
    mock_datetime,
    mock_milliseconds
};

static uint32_t load_u32(const unsigned char *memory, uint32_t address)
{
    uint32_t value;
    memcpy(&value, memory + address, sizeof(value));
    return value;
}

static int check(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        return 0;
    }
    return 1;
}

int main(void)
{
    struct MockBackend mock;
    struct Os2PersonalityContext context;
    const struct Os2ApiDescriptor *api;
    os2_api_ret_t rc;
    int ok;

    memset(&mock, 0, sizeof(mock));
    os2_personality_context_init(&context, &mock, &mock_ops);
    ok = 1;

    api = os2_api_lookup("doscalls", 282u);
    ok &= check(api != NULL && strcmp(api->api_name, "DosWrite") == 0,
                "catalog resolves DOSCALLS.282");
    ok &= check(api != NULL && api->route == OS2_API_ROUTE_SHARED,
                "DOSCALLS.282 is marked shared");
    ok &= check(os2_api_lookup("PMWIN", 763u) != NULL,
                "catalog covers system DLL ordinals");

    rc = os2_core_DosGetDateTime(&context, 0x180u);
    ok &= check(rc == 0u &&
                mock.memory[0x180u] == 23u &&
                mock.memory[0x181u] == 58u &&
                mock.memory[0x182u] == 57u &&
                mock.memory[0x183u] == 42u &&
                mock.memory[0x184u] == 31u &&
                mock.memory[0x185u] == 12u &&
                mock.memory[0x186u] == 0xcfu &&
                mock.memory[0x187u] == 0x07u &&
                mock.memory[0x188u] == 0xc4u &&
                mock.memory[0x189u] == 0xffu &&
                mock.memory[0x18au] == 5u &&
                mock.memory[0x18bu] == 0u,
                "DosGetDateTime has one shared byte layout");

    rc = os2_core_DosQueryHType(&context, 1u, 0x100u, 0x104u);
    ok &= check(rc == 0u && load_u32(mock.memory, 0x100u) == 1u &&
                load_u32(mock.memory, 0x104u) == 0u,
                "DosQueryHType writes shared results");

    memcpy(mock.memory + 0x200u, "hello", 5u);
    rc = os2_core_DosWrite(&context, 1u, 0x200u, 5u, 0x108u);
    ok &= check(rc == 0u && mock.output_length == 5u &&
                memcmp(mock.output, "hello", 5u) == 0 &&
                load_u32(mock.memory, 0x108u) == 5u,
                "DosWrite maps an application buffer through the backend");
    rc = os2_core_DosWrite(&context, 1u, 0u, 1u, 0x108u);
    ok &= check(rc == OS2_PERSONALITY_ERROR_INVALID_PARAMETER,
                "DosWrite rejects a null non-empty buffer");

    rc = os2_core_DosSetFilePtr(&context, 1u, 25, 0u, 0x10cu);
    ok &= check(rc == 0u && load_u32(mock.memory, 0x10cu) == 25u,
                "DosSetFilePtr shares method validation and result storage");
    rc = os2_core_DosSetFilePtr(&context, 1u, 0, 3u, 0x10cu);
    ok &= check(rc == OS2_PERSONALITY_ERROR_INVALID_PARAMETER,
                "DosSetFilePtr rejects an invalid origin");

    rc = os2_core_DosAllocMem(&context, 0x110u, 8192u, 0x13u, 0u);
    ok &= check(rc == 0u && load_u32(mock.memory, 0x110u) == 0x2000u &&
                mock.allocation_live,
                "DosAllocMem stores the backend address token");
    rc = os2_core_DosSetMem(&context, 0x2000u, 4096u, 0x11u);
    ok &= check(rc == 0u && mock.set_base == 0x2000u &&
                mock.set_size == 4096u && mock.set_flags == 0x11u,
                "DosSetMem delegates loader-specific page work");
    rc = os2_core_DosFreeMem(&context, 0x2000u);
    ok &= check(rc == 0u && !mock.allocation_live,
                "DosFreeMem delegates loader-specific allocation tracking");

    mock.allocation_returns_zero = 1;
    rc = os2_core_DosAllocMem(&context, 0x114u, 4096u, 0x13u, 0u);
    ok &= check(rc == OS2_PERSONALITY_ERROR_NOT_ENOUGH_MEMORY &&
                !mock.allocation_live,
                "DosAllocMem rejects a successful backend with a null token");
    mock.allocation_returns_zero = 0;

    mock.fail_write_address = 0x114u;
    rc = os2_core_DosAllocMem(&context, 0x114u, 4096u, 0x13u, 0u);
    ok &= check(rc == OS2_PERSONALITY_ERROR_INVALID_PARAMETER &&
                !mock.allocation_live,
                "DosAllocMem rolls back when pointer writeback fails");
    mock.fail_write_address = 0u;

    rc = os2_core_DosGetDateTime(&context, 0u);
    ok &= check(rc == OS2_PERSONALITY_ERROR_INVALID_PARAMETER,
                "DosGetDateTime rejects a null output pointer");

    rc = os2_core_DosQuerySysInfo(&context, 10u, 14u, 0x300u, 20u);
    ok &= check(rc == 0u &&
                load_u32(mock.memory, 0x300u) == 4096u &&
                load_u32(mock.memory, 0x304u) == 20u &&
                load_u32(mock.memory, 0x308u) == 0u &&
                load_u32(mock.memory, 0x30cu) == 0u &&
                load_u32(mock.memory, 0x310u) == 123456u,
                "DosQuerySysInfo produces one common value set");

    if (!ok)
        return 1;
    printf("PASS: shared DOSCALLS personality core and ordinal catalogue\n");
    return 0;
}
