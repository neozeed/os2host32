#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "os2_nls.h"
#include "os2_nls_api.h"
#include "os2_personality.h"

#define MEM_BASE 0x1000u
#define MEM_SIZE 0x2000u

struct MockMemory {
    unsigned char bytes[MEM_SIZE];
};

static int failures = 0;

static void check_int(const char *what, uint32_t got, uint32_t expected)
{
    if (got != expected) {
        printf("FAIL: %s got=%u expected=%u\n", what,
               (unsigned)got, (unsigned)expected);
        failures++;
    }
}

static void check_byte(const char *what, unsigned char got,
                       unsigned char expected)
{
    if (got != expected) {
        printf("FAIL: %s got=%02X expected=%02X\n", what,
               (unsigned)got, (unsigned)expected);
        failures++;
    }
}

static uint32_t load_u32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void store_u32(unsigned char *p, uint32_t value)
{
    p[0] = (unsigned char)(value & 0xffu);
    p[1] = (unsigned char)((value >> 8) & 0xffu);
    p[2] = (unsigned char)((value >> 16) & 0xffu);
    p[3] = (unsigned char)((value >> 24) & 0xffu);
}

static os2_api_ret_t mock_validate(void *opaque, os2_addr32_t address,
                                   uint32_t length, uint32_t access)
{
    (void)opaque;
    (void)access;
    if (length == 0u)
        return 0u;
    if (address < MEM_BASE || address - MEM_BASE > MEM_SIZE)
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    if (length > MEM_SIZE - (address - MEM_BASE))
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    return 0u;
}

static os2_api_ret_t mock_write(void *opaque, os2_addr32_t address,
                                const void *source, uint32_t length)
{
    struct MockMemory *memory;
    memory = (struct MockMemory *)opaque;
    if (mock_validate(opaque, address, length, OS2_MEMORY_WRITE) != 0u)
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    if (length != 0u)
        memcpy(memory->bytes + (address - MEM_BASE), source, (size_t)length);
    return 0u;
}

static os2_api_ret_t mock_map_read(void *opaque, os2_addr32_t address,
                                   uint32_t length, const void **host_pointer)
{
    struct MockMemory *memory;
    memory = (struct MockMemory *)opaque;
    if (host_pointer == NULL)
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    if (mock_validate(opaque, address, length, OS2_MEMORY_READ) != 0u)
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    *host_pointer = memory->bytes + (address - MEM_BASE);
    return 0u;
}

static const struct Os2PersonalityOps mock_ops = {
    mock_validate,
    mock_write,
    mock_map_read,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

static void test_core(void)
{
    struct Os2NlsState state;
    uint32_t cp[4];
    uint32_t actual;
    uint32_t rc;
    unsigned char info[OS2_NLS_COUNTRYINFO_SIZE];
    unsigned char text437[5];
    unsigned char text850[5];
    unsigned char dbcs[8];

    os2_nls_state_init(&state);
    memset(cp, 0, sizeof(cp));
    actual = 99u;
    rc = os2_nls_query_cp(&state, 8u, cp, &actual);
    check_int("query cp exact rc", rc, 0u);
    check_int("query cp exact actual", actual, 8u);
    check_int("query cp first", cp[0], 437u);
    check_int("query cp second", cp[1], 850u);

    memset(cp, 0, sizeof(cp));
    actual = 99u;
    rc = os2_nls_query_cp(&state, 4u, cp, &actual);
    check_int("query cp short rc", rc, OS2_NLS_ERROR_CPLIST_TOO_SMALL);
    check_int("query cp short actual", actual, 4u);
    check_int("query cp short first", cp[0], 437u);

    actual = 99u;
    rc = os2_nls_query_cp(&state, 2u, cp, &actual);
    check_int("query cp partial dword rc", rc, OS2_NLS_ERROR_CPLIST_TOO_SMALL);
    check_int("query cp partial dword actual", actual, 0u);

    rc = os2_nls_set_process_cp(&state, 850u);
    check_int("set cp850", rc, 0u);
    memset(cp, 0, sizeof(cp));
    actual = 0u;
    rc = os2_nls_query_cp(&state, sizeof(cp), cp, &actual);
    check_int("query after set rc", rc, 0u);
    check_int("query after set current", cp[0], 850u);
    check_int("query after set alternate", cp[1], 437u);

    rc = os2_nls_set_process_cp(&state, 932u);
    check_int("reject unsupported cp", rc, OS2_NLS_ERROR_INVALID_CODE_PAGE);
    check_int("unsupported cp leaves current", state.current_codepage, 850u);

    memset(info, 0xcc, sizeof(info));
    actual = 0u;
    rc = os2_nls_query_country_info(&state, 0u, 0u, sizeof(info), info, &actual);
    check_int("country default rc", rc, 0u);
    check_int("country info size", actual, OS2_NLS_COUNTRYINFO_SIZE);
    check_int("country default", load_u32(info + 0), 1u);
    check_int("country current cp", load_u32(info + 4), 850u);
    check_byte("country currency", info[12], (unsigned char)'$');
    check_byte("country decimal", info[19], (unsigned char)'.');
    check_byte("country US date separator", info[21], (unsigned char)'-');

    actual = 0u;
    rc = os2_nls_query_country_info(&state, 44u, 437u, 12u, info, &actual);
    check_int("country truncated rc", rc, OS2_NLS_ERROR_TABLE_TRUNCATED);
    check_int("country truncated actual", actual, OS2_NLS_COUNTRYINFO_SIZE);
    check_int("UK country id", load_u32(info), 44u);
    check_int("UK requested cp", load_u32(info + 4), 437u);

    actual = 0u;
    rc = os2_nls_query_country_info(&state, 999u, 0u, sizeof(info), info, &actual);
    check_int("unknown country", rc, OS2_NLS_ERROR_NO_CTRY_CODE);

    text437[0] = (unsigned char)'a';
    text437[1] = 0x82u; /* e acute */
    text437[2] = 0x81u; /* u diaeresis */
    text437[3] = 0xe5u; /* sigma */
    text437[4] = 0xffu;
    rc = os2_nls_map_case(&state, 1u, 437u, text437, sizeof(text437));
    check_int("CP437 map rc", rc, 0u);
    check_byte("CP437 ascii", text437[0], (unsigned char)'A');
    check_byte("CP437 e acute", text437[1], 0x90u);
    check_byte("CP437 u diaeresis", text437[2], 0x9au);
    check_byte("CP437 sigma", text437[3], 0xe4u);
    check_byte("CP437 high unchanged", text437[4], 0xffu);

    text850[0] = (unsigned char)'z';
    text850[1] = 0x83u; /* a circumflex */
    text850[2] = 0x88u; /* e circumflex */
    text850[3] = 0x9bu; /* o slash */
    text850[4] = 0xffu;
    rc = os2_nls_map_case(&state, 44u, 850u, text850, sizeof(text850));
    check_int("CP850 map rc", rc, 0u);
    check_byte("CP850 ascii", text850[0], (unsigned char)'Z');
    check_byte("CP850 a circumflex", text850[1], 0xb6u);
    check_byte("CP850 e circumflex", text850[2], 0xd2u);
    check_byte("CP850 o slash", text850[3], 0x9du);
    check_byte("CP850 high unchanged", text850[4], 0xffu);

    memset(dbcs, 0xcc, sizeof(dbcs));
    rc = os2_nls_query_dbcs_env(&state, 0u, 437u, sizeof(dbcs), dbcs);
    check_int("CP437 dbcs rc", rc, 0u);
    check_byte("CP437 dbcs start", dbcs[0], 0u);
    check_byte("CP437 dbcs terminator", dbcs[1], 0u);
    check_byte("CP437 dbcs zero fill", dbcs[7], 0u);
}

static void test_guest_api(void)
{
    struct MockMemory memory;
    struct Os2NlsState state;
    struct Os2PersonalityContext context;
    uint32_t rc;
    unsigned char *p;

    memset(&memory, 0, sizeof(memory));
    os2_nls_state_init(&state);
    os2_personality_context_init(&context, &memory, &mock_ops);
    os2_personality_context_set_nls(&context, &state);

    rc = os2_nls_api_DosSetProcessCp(&context, 850u);
    check_int("guest set cp", rc, 0u);
    rc = os2_nls_api_DosQueryCp(&context, 8u, MEM_BASE + 0x100u,
                                MEM_BASE + 0x120u);
    check_int("guest query cp rc", rc, 0u);
    p = memory.bytes + 0x100u;
    check_int("guest query sees switched cp", load_u32(p), 850u);
    check_int("guest query alternate", load_u32(p + 4), 437u);
    check_int("guest query actual", load_u32(memory.bytes + 0x120u), 8u);

    /* COUNTRYCODE { country=0, codepage=0 } chooses process state. */
    store_u32(memory.bytes + 0x200u, 0u);
    store_u32(memory.bytes + 0x204u, 0u);
    rc = os2_nls_api_DosQueryCtryInfo(&context, OS2_NLS_COUNTRYINFO_SIZE,
                                      MEM_BASE + 0x200u, MEM_BASE + 0x300u,
                                      MEM_BASE + 0x380u);
    check_int("guest country rc", rc, 0u);
    check_int("guest country current cp", load_u32(memory.bytes + 0x304u), 850u);
    check_int("guest country actual", load_u32(memory.bytes + 0x380u),
              OS2_NLS_COUNTRYINFO_SIZE);

    /* Semantic lookup errors do not fabricate COUNTRYINFO size/results. */
    store_u32(memory.bytes + 0x200u, 999u);
    store_u32(memory.bytes + 0x204u, 0u);
    store_u32(memory.bytes + 0x380u, 0x12345678u);
    rc = os2_nls_api_DosQueryCtryInfo(&context, OS2_NLS_COUNTRYINFO_SIZE,
                                      MEM_BASE + 0x200u, MEM_BASE + 0x300u,
                                      MEM_BASE + 0x380u);
    check_int("guest unknown country rc", rc, OS2_NLS_ERROR_NO_CTRY_CODE);
    check_int("guest unknown country preserves actual",
              load_u32(memory.bytes + 0x380u), 0x12345678u);

    store_u32(memory.bytes + 0x200u, 0u);
    store_u32(memory.bytes + 0x204u, 0u);
    rc = os2_nls_api_DosQueryCtryInfo(&context, 0u, MEM_BASE + 0x200u, 0u,
                                      MEM_BASE + 0x380u);
    check_int("guest null COUNTRYINFO", rc,
              OS2_PERSONALITY_ERROR_INVALID_PARAMETER);

    memory.bytes[0x400u] = (unsigned char)'a';
    memory.bytes[0x401u] = 0x83u;
    rc = os2_nls_api_DosMapCase(&context, 2u, MEM_BASE + 0x200u,
                                MEM_BASE + 0x400u);
    check_int("guest map rc", rc, 0u);
    check_byte("guest map ascii", memory.bytes[0x400u], (unsigned char)'A');
    check_byte("guest map cp850", memory.bytes[0x401u], 0xb6u);

    memset(memory.bytes + 0x500u, 0xcc, 8u);
    rc = os2_nls_api_DosQueryDBCSEnv(&context, 8u, MEM_BASE + 0x200u,
                                     MEM_BASE + 0x500u);
    check_int("guest dbcs rc", rc, 0u);
    check_byte("guest dbcs zero", memory.bytes[0x500u], 0u);
    check_byte("guest dbcs zero fill", memory.bytes[0x507u], 0u);

    rc = os2_nls_api_DosQueryCp(&context, 8u, 0u, MEM_BASE + 0x600u);
    check_int("guest invalid buffer", rc, OS2_PERSONALITY_ERROR_INVALID_PARAMETER);
    rc = os2_nls_api_DosQueryCp(&context, 8u, MEM_BASE + 0x600u, 0u);
    check_int("guest invalid actual", rc, OS2_PERSONALITY_ERROR_INVALID_PARAMETER);
}

int main(void)
{
    test_core();
    test_guest_api();
    if (failures != 0) {
        printf("FAIL: %d NLS checks failed\n", failures);
        return 1;
    }
    printf("PASS: OS/2 NLS core, process state, extended case maps and guest ABI\n");
    return 0;
}
