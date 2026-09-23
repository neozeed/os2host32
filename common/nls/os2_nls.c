/*
 * Loader-independent OS/2 National Language Support core.
 *
 * R1 deliberately supports a small, explicit western SBCS registry instead
 * of forwarding guest-visible semantics to Win32 locale structures.  The
 * registry/state can be used unchanged by the native DOSCALLS/NLS veneers,
 * WHP and a future software-x86 loader.
 */
#include "os2_nls.h"

#include <stddef.h>
#include <string.h>

#include "os2_nls_tables.inc"

static const unsigned char no_dbcs_ranges[2] = { 0u, 0u };

static const struct Os2NlsCodepage codepages[] = {
    { 437u, 0u, no_dbcs_ranges, 2u, os2_cp437_upper, os2_cp437_lower },
    { 850u, 0u, no_dbcs_ranges, 2u, os2_cp850_upper, os2_cp850_lower }
};

/* COUNTRYINFO strings are bytes in the selected OEM code page.  Pound is
 * 0x9c in both CP437 and CP850.  These two profiles are intentionally small:
 * R1 prefers tested data over pretending to implement every COUNTRY.SYS row.
 */
static const struct Os2NlsCountryProfile countries[] = {
    {
        1u, 437u, 850u, 0u,
        { '$', 0, 0, 0, 0 }, { ',', 0 }, { '.', 0 }, { '-', 0 }, { ':', 0 },
        0u, 2u, 0u, { ',', 0 }
    },
    {
        44u, 437u, 850u, 1u,
        { 0x9cu, 0, 0, 0, 0 }, { ',', 0 }, { '.', 0 }, { '/', 0 }, { ':', 0 },
        0u, 2u, 1u, { ',', 0 }
    }
};

static void store_u32(unsigned char *p, uint32_t value)
{
    p[0] = (unsigned char)(value & 0xffu);
    p[1] = (unsigned char)((value >> 8) & 0xffu);
    p[2] = (unsigned char)((value >> 16) & 0xffu);
    p[3] = (unsigned char)((value >> 24) & 0xffu);
}

const struct Os2NlsCodepage *os2_nls_codepage(uint32_t codepage)
{
    uint32_t i;
    for (i = 0u; i < (uint32_t)(sizeof(codepages) / sizeof(codepages[0])); ++i) {
        if (codepages[i].id == codepage)
            return &codepages[i];
    }
    return NULL;
}

int os2_nls_has_codepage(uint32_t codepage)
{
    return os2_nls_codepage(codepage) != NULL;
}

const struct Os2NlsCountryProfile *os2_nls_country(uint32_t country)
{
    uint32_t i;
    for (i = 0u; i < (uint32_t)(sizeof(countries) / sizeof(countries[0])); ++i) {
        if (countries[i].country == country)
            return &countries[i];
    }
    return NULL;
}

void os2_nls_state_init(struct Os2NlsState *state)
{
    if (state == NULL)
        return;
    memset(state, 0, sizeof(*state));
    state->country = 1u;
    state->current_codepage = 437u;
    state->prepared_codepages[0] = 437u;
    state->prepared_codepages[1] = 850u;
    state->prepared_count = 2u;
}

int os2_nls_set_country(struct Os2NlsState *state, uint32_t country)
{
    const struct Os2NlsCountryProfile *profile;
    if (state == NULL)
        return 0;
    profile = os2_nls_country(country);
    if (profile == NULL)
        return 0;
    state->country = country;
    return 1;
}

static uint32_t resolve_country(const struct Os2NlsState *state,
                                uint32_t requested,
                                const struct Os2NlsCountryProfile **profile)
{
    uint32_t country;
    if (state == NULL || profile == NULL)
        return OS2_NLS_ERROR_INVALID_PARAMETER;
    country = requested != 0u ? requested : state->country;
    *profile = os2_nls_country(country);
    return *profile != NULL ? OS2_NLS_NO_ERROR : OS2_NLS_ERROR_NO_CTRY_CODE;
}

static uint32_t resolve_codepage(const struct Os2NlsState *state,
                                 uint32_t requested,
                                 const struct Os2NlsCodepage **cp)
{
    uint32_t codepage;
    if (state == NULL || cp == NULL)
        return OS2_NLS_ERROR_INVALID_PARAMETER;
    codepage = requested != 0u ? requested : state->current_codepage;
    *cp = os2_nls_codepage(codepage);
    return *cp != NULL ? OS2_NLS_NO_ERROR : OS2_NLS_ERROR_CODE_PAGE_NOT_FOUND;
}

static int country_supports_codepage(const struct Os2NlsCountryProfile *country,
                                     uint32_t codepage)
{
    if (country == NULL)
        return 0;
    return codepage == country->primary_codepage ||
           (country->secondary_codepage != 0u &&
            codepage == country->secondary_codepage);
}

uint32_t os2_nls_set_process_cp(struct Os2NlsState *state,
                                uint32_t codepage)
{
    uint32_t i;
    if (state == NULL)
        return OS2_NLS_ERROR_INVALID_PARAMETER;
    if (!os2_nls_has_codepage(codepage))
        return OS2_NLS_ERROR_INVALID_CODE_PAGE;
    for (i = 0u; i < state->prepared_count; ++i) {
        if (state->prepared_codepages[i] == codepage) {
            state->current_codepage = codepage;
            return OS2_NLS_NO_ERROR;
        }
    }
    return OS2_NLS_ERROR_INVALID_CODE_PAGE;
}

uint32_t os2_nls_query_cp(const struct Os2NlsState *state,
                          uint32_t buffer_bytes,
                          uint32_t *out,
                          uint32_t *actual_bytes)
{
    uint32_t ordered[OS2_NLS_MAX_PREPARED_CP + 1u];
    uint32_t count;
    uint32_t i;
    uint32_t slots;
    uint32_t copied;
    uint32_t total;

    if (state == NULL || actual_bytes == NULL ||
        (buffer_bytes != 0u && out == NULL))
        return OS2_NLS_ERROR_INVALID_PARAMETER;

    count = 0u;
    ordered[count++] = state->current_codepage;
    for (i = 0u; i < state->prepared_count; ++i) {
        if (state->prepared_codepages[i] != state->current_codepage &&
            count < (uint32_t)(sizeof(ordered) / sizeof(ordered[0])))
            ordered[count++] = state->prepared_codepages[i];
    }

    total = count * 4u;
    slots = buffer_bytes / 4u;
    copied = slots < count ? slots : count;
    for (i = 0u; i < copied; ++i)
        out[i] = ordered[i];
    *actual_bytes = copied * 4u;
    return buffer_bytes < total ? OS2_NLS_ERROR_CPLIST_TOO_SMALL
                                : OS2_NLS_NO_ERROR;
}

uint32_t os2_nls_query_country_info(const struct Os2NlsState *state,
                                    uint32_t requested_country,
                                    uint32_t requested_codepage,
                                    uint32_t buffer_bytes,
                                    unsigned char *buffer,
                                    uint32_t *actual_bytes)
{
    const struct Os2NlsCountryProfile *country;
    const struct Os2NlsCodepage *cp;
    unsigned char packed[OS2_NLS_COUNTRYINFO_SIZE];
    uint32_t rc;
    uint32_t copy;

    if (state == NULL || actual_bytes == NULL ||
        (buffer_bytes != 0u && buffer == NULL))
        return OS2_NLS_ERROR_INVALID_PARAMETER;

    rc = resolve_country(state, requested_country, &country);
    if (rc != 0u)
        return rc;
    rc = resolve_codepage(state, requested_codepage, &cp);
    if (rc != 0u || !country_supports_codepage(country, cp != NULL ? cp->id : 0u))
        return OS2_NLS_ERROR_NO_CTRY_CODE;

    memset(packed, 0, sizeof(packed));
    store_u32(packed + 0u, country->country);
    store_u32(packed + 4u, cp->id);
    store_u32(packed + 8u, country->date_format);
    memcpy(packed + 12u, country->currency, 5u);
    memcpy(packed + 17u, country->thousands_separator, 2u);
    memcpy(packed + 19u, country->decimal_separator, 2u);
    memcpy(packed + 21u, country->date_separator, 2u);
    memcpy(packed + 23u, country->time_separator, 2u);
    packed[25] = country->currency_format;
    packed[26] = country->decimal_places;
    packed[27] = country->time_format;
    memcpy(packed + 32u, country->list_separator, 2u);

    *actual_bytes = OS2_NLS_COUNTRYINFO_SIZE;
    copy = buffer_bytes < OS2_NLS_COUNTRYINFO_SIZE ? buffer_bytes
                                                   : OS2_NLS_COUNTRYINFO_SIZE;
    if (copy != 0u)
        memcpy(buffer, packed, (size_t)copy);
    return buffer_bytes < OS2_NLS_COUNTRYINFO_SIZE
        ? OS2_NLS_ERROR_TABLE_TRUNCATED : OS2_NLS_NO_ERROR;
}

uint32_t os2_nls_query_dbcs_env(const struct Os2NlsState *state,
                                uint32_t requested_country,
                                uint32_t requested_codepage,
                                uint32_t buffer_bytes,
                                unsigned char *buffer)
{
    const struct Os2NlsCountryProfile *country;
    const struct Os2NlsCodepage *cp;
    uint32_t rc;
    uint32_t copy;

    if (state == NULL || buffer == NULL || buffer_bytes == 0u)
        return OS2_NLS_ERROR_INVALID_PARAMETER;
    rc = resolve_country(state, requested_country, &country);
    if (rc != 0u)
        return rc;
    (void)country;
    rc = resolve_codepage(state, requested_codepage, &cp);
    if (rc != 0u)
        return rc;
    if (!country_supports_codepage(country, cp->id))
        return OS2_NLS_ERROR_CODE_PAGE_NOT_FOUND;

    memset(buffer, 0, (size_t)buffer_bytes);
    copy = cp->lead_range_bytes < buffer_bytes ? cp->lead_range_bytes
                                               : buffer_bytes;
    if (copy != 0u)
        memcpy(buffer, cp->lead_ranges, (size_t)copy);
    return cp->lead_range_bytes > buffer_bytes
        ? OS2_NLS_ERROR_TABLE_TRUNCATED : OS2_NLS_NO_ERROR;
}

uint32_t os2_nls_map_case(const struct Os2NlsState *state,
                          uint32_t requested_country,
                          uint32_t requested_codepage,
                          unsigned char *buffer,
                          uint32_t buffer_bytes)
{
    const struct Os2NlsCountryProfile *country;
    const struct Os2NlsCodepage *cp;
    uint32_t rc;
    uint32_t i;

    if (state == NULL || (buffer_bytes != 0u && buffer == NULL))
        return OS2_NLS_ERROR_INVALID_PARAMETER;
    rc = resolve_country(state, requested_country, &country);
    if (rc != 0u)
        return rc;
    (void)country;
    rc = resolve_codepage(state, requested_codepage, &cp);
    if (rc != 0u)
        return rc;
    if (!country_supports_codepage(country, cp->id))
        return OS2_NLS_ERROR_CODE_PAGE_NOT_FOUND;
    for (i = 0u; i < buffer_bytes; ++i)
        buffer[i] = cp->upper_map[(unsigned int)buffer[i]];
    return OS2_NLS_NO_ERROR;
}
