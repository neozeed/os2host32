/* Guest-address marshalling for OS/2 NLS APIs. */
#include "os2_nls_api.h"
#include "os2_nls.h"

#include <stddef.h>
#include <string.h>

#define OS2_U32_MAX 0xffffffffu

static int have_memory(const struct Os2PersonalityContext *context)
{
    return context != NULL && context->ops != NULL && context->nls != NULL &&
           context->ops->validate_memory != NULL &&
           context->ops->write_memory != NULL &&
           context->ops->map_read_memory != NULL;
}

static os2_api_ret_t validate(struct Os2PersonalityContext *context,
                              os2_addr32_t address, uint32_t length,
                              uint32_t access)
{
    if (!have_memory(context))
        return OS2_PERSONALITY_ERROR_INVALID_FUNCTION;
    if (length != 0u && address == 0u)
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    if (length != 0u && address > OS2_U32_MAX - (length - 1u))
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    return context->ops->validate_memory(context->opaque, address,
                                         length, access);
}

static uint32_t load_u32_le(const unsigned char *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void store_u32_le(unsigned char *p, uint32_t value)
{
    p[0] = (unsigned char)(value & 0xffu);
    p[1] = (unsigned char)((value >> 8) & 0xffu);
    p[2] = (unsigned char)((value >> 16) & 0xffu);
    p[3] = (unsigned char)((value >> 24) & 0xffu);
}

static os2_api_ret_t read_country_code(struct Os2PersonalityContext *context,
                                       os2_addr32_t address,
                                       uint32_t *country,
                                       uint32_t *codepage)
{
    const void *mapped;
    os2_api_ret_t rc;
    if (country == NULL || codepage == NULL)
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    *country = 0u;
    *codepage = 0u;
    if (address == 0u)
        return OS2_PERSONALITY_NO_ERROR;
    rc = validate(context, address, 8u, OS2_MEMORY_READ);
    if (rc != 0u)
        return rc;
    mapped = NULL;
    rc = context->ops->map_read_memory(context->opaque, address, 8u, &mapped);
    if (rc != 0u)
        return rc;
    if (mapped == NULL)
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    *country = load_u32_le((const unsigned char *)mapped);
    *codepage = load_u32_le((const unsigned char *)mapped + 4u);
    return OS2_PERSONALITY_NO_ERROR;
}

static os2_api_ret_t write_u32(struct Os2PersonalityContext *context,
                               os2_addr32_t address, uint32_t value)
{
    unsigned char packed[4];
    os2_api_ret_t rc;
    rc = validate(context, address, 4u, OS2_MEMORY_WRITE);
    if (rc != 0u)
        return rc;
    store_u32_le(packed, value);
    return context->ops->write_memory(context->opaque, address, packed, 4u);
}

os2_api_ret_t os2_nls_api_DosSetProcessCp(
    struct Os2PersonalityContext *context, uint32_t codepage)
{
    if (context == NULL || context->nls == NULL)
        return OS2_PERSONALITY_ERROR_INVALID_FUNCTION;
    return (os2_api_ret_t)os2_nls_set_process_cp(context->nls, codepage);
}

os2_api_ret_t os2_nls_api_DosQueryCp(
    struct Os2PersonalityContext *context, uint32_t cb,
    os2_addr32_t codepages_address, os2_addr32_t actual_address)
{
    uint32_t list[OS2_NLS_MAX_PREPARED_CP + 1u];
    unsigned char packed[(OS2_NLS_MAX_PREPARED_CP + 1u) * 4u];
    uint32_t actual;
    uint32_t copy;
    uint32_t i;
    uint32_t rc;
    os2_api_ret_t mrc;

    if (!have_memory(context))
        return OS2_PERSONALITY_ERROR_INVALID_FUNCTION;
    mrc = validate(context, actual_address, 4u, OS2_MEMORY_WRITE);
    if (mrc != 0u)
        return mrc;
    if (cb != 0u) {
        mrc = validate(context, codepages_address, cb, OS2_MEMORY_WRITE);
        if (mrc != 0u)
            return mrc;
    }

    memset(list, 0, sizeof(list));
    memset(packed, 0, sizeof(packed));
    actual = 0u;
    rc = os2_nls_query_cp(context->nls, cb, list, &actual);
    copy = actual;
    for (i = 0u; i < copy / 4u; ++i)
        store_u32_le(packed + i * 4u, list[i]);
    if (copy != 0u) {
        mrc = context->ops->write_memory(context->opaque, codepages_address,
                                         packed, copy);
        if (mrc != 0u)
            return mrc;
    }
    mrc = write_u32(context, actual_address, actual);
    return mrc != 0u ? mrc : (os2_api_ret_t)rc;
}

os2_api_ret_t os2_nls_api_DosQueryCtryInfo(
    struct Os2PersonalityContext *context, uint32_t cb,
    os2_addr32_t countrycode_address, os2_addr32_t countryinfo_address,
    os2_addr32_t actual_address)
{
    unsigned char packed[OS2_NLS_COUNTRYINFO_SIZE];
    uint32_t country;
    uint32_t codepage;
    uint32_t actual;
    uint32_t copy;
    uint32_t rc;
    os2_api_ret_t mrc;

    if (!have_memory(context))
        return OS2_PERSONALITY_ERROR_INVALID_FUNCTION;
    mrc = read_country_code(context, countrycode_address, &country, &codepage);
    if (mrc != 0u)
        return mrc;
    mrc = validate(context, actual_address, 4u, OS2_MEMORY_WRITE);
    if (mrc != 0u)
        return mrc;
    if (countryinfo_address == 0u)
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    if (cb != 0u) {
        mrc = validate(context, countryinfo_address, cb, OS2_MEMORY_WRITE);
        if (mrc != 0u)
            return mrc;
    }

    memset(packed, 0, sizeof(packed));
    actual = 0u;
    rc = os2_nls_query_country_info(context->nls, country, codepage,
                                    cb, packed, &actual);
    copy = cb < OS2_NLS_COUNTRYINFO_SIZE ? cb : OS2_NLS_COUNTRYINFO_SIZE;
    if ((rc == OS2_NLS_NO_ERROR || rc == OS2_NLS_ERROR_TABLE_TRUNCATED) &&
        copy != 0u) {
        mrc = context->ops->write_memory(context->opaque,
                                         countryinfo_address, packed, copy);
        if (mrc != 0u)
            return mrc;
    }
    if (rc != OS2_NLS_NO_ERROR && rc != OS2_NLS_ERROR_TABLE_TRUNCATED)
        return (os2_api_ret_t)rc;
    mrc = write_u32(context, actual_address, actual);
    return mrc != 0u ? mrc : (os2_api_ret_t)rc;
}

os2_api_ret_t os2_nls_api_DosQueryDBCSEnv(
    struct Os2PersonalityContext *context, uint32_t cb,
    os2_addr32_t countrycode_address, os2_addr32_t buffer_address)
{
    unsigned char local[32];
    uint32_t country;
    uint32_t codepage;
    uint32_t rc;
    os2_api_ret_t mrc;
    uint32_t done;
    uint32_t chunk;

    if (!have_memory(context))
        return OS2_PERSONALITY_ERROR_INVALID_FUNCTION;
    if (cb == 0u)
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    mrc = read_country_code(context, countrycode_address, &country, &codepage);
    if (mrc != 0u)
        return mrc;
    mrc = validate(context, buffer_address, cb, OS2_MEMORY_WRITE);
    if (mrc != 0u)
        return mrc;

    /* R1 codepages are SBCS. Ask the common core once, then stream zeros to
     * arbitrary guest buffer sizes without allocating based on guest input. */
    memset(local, 0, sizeof(local));
    rc = os2_nls_query_dbcs_env(context->nls, country, codepage,
                                (uint32_t)sizeof(local), local);
    if (rc != OS2_NLS_NO_ERROR)
        return (os2_api_ret_t)rc;
    done = 0u;
    while (done < cb) {
        chunk = cb - done;
        if (chunk > (uint32_t)sizeof(local))
            chunk = (uint32_t)sizeof(local);
        mrc = context->ops->write_memory(context->opaque,
                                         buffer_address + done, local, chunk);
        if (mrc != 0u)
            return mrc;
        done += chunk;
        if (done != 0u)
            memset(local, 0, sizeof(local));
    }
    return OS2_NLS_NO_ERROR;
}

os2_api_ret_t os2_nls_api_DosMapCase(
    struct Os2PersonalityContext *context, uint32_t cb,
    os2_addr32_t countrycode_address, os2_addr32_t buffer_address)
{
    unsigned char local[256];
    const void *mapped;
    uint32_t country;
    uint32_t codepage;
    uint32_t done;
    uint32_t chunk;
    uint32_t rc;
    os2_api_ret_t mrc;

    if (!have_memory(context))
        return OS2_PERSONALITY_ERROR_INVALID_FUNCTION;
    mrc = read_country_code(context, countrycode_address, &country, &codepage);
    if (mrc != 0u)
        return mrc;
    if (cb == 0u)
        return (os2_api_ret_t)os2_nls_map_case(context->nls, country,
                                               codepage, NULL, 0u);
    mrc = validate(context, buffer_address, cb,
                   OS2_MEMORY_READ | OS2_MEMORY_WRITE);
    if (mrc != 0u)
        return mrc;

    done = 0u;
    while (done < cb) {
        chunk = cb - done;
        if (chunk > (uint32_t)sizeof(local))
            chunk = (uint32_t)sizeof(local);
        mapped = NULL;
        mrc = context->ops->map_read_memory(context->opaque,
                                            buffer_address + done,
                                            chunk, &mapped);
        if (mrc != 0u)
            return mrc;
        if (mapped == NULL)
            return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
        memcpy(local, mapped, (size_t)chunk);
        rc = os2_nls_map_case(context->nls, country, codepage, local, chunk);
        if (rc != OS2_NLS_NO_ERROR)
            return (os2_api_ret_t)rc;
        mrc = context->ops->write_memory(context->opaque,
                                         buffer_address + done, local, chunk);
        if (mrc != 0u)
            return mrc;
        done += chunk;
    }
    return OS2_NLS_NO_ERROR;
}
