#ifndef OS2_NLS_H
#define OS2_NLS_H

#include <stdint.h>

#define OS2_NLS_NO_ERROR                    0u
#define OS2_NLS_ERROR_INVALID_PARAMETER    87u
#define OS2_NLS_ERROR_NO_CTRY_CODE        398u
#define OS2_NLS_ERROR_TABLE_TRUNCATED     399u
#define OS2_NLS_ERROR_INVALID_CODE_PAGE   472u
#define OS2_NLS_ERROR_CPLIST_TOO_SMALL    473u
#define OS2_NLS_ERROR_CODE_PAGE_NOT_FOUND 476u

#define OS2_NLS_COUNTRYINFO_SIZE 44u
#define OS2_NLS_MAX_PREPARED_CP  4u

struct Os2NlsCodepage {
    uint32_t id;
    uint32_t is_dbcs;
    const unsigned char *lead_ranges;
    uint32_t lead_range_bytes;
    const unsigned char *upper_map;
    const unsigned char *lower_map;
};

struct Os2NlsCountryProfile {
    uint32_t country;
    uint32_t primary_codepage;
    uint32_t secondary_codepage;
    uint32_t date_format;
    unsigned char currency[5];
    unsigned char thousands_separator[2];
    unsigned char decimal_separator[2];
    unsigned char date_separator[2];
    unsigned char time_separator[2];
    uint8_t currency_format;
    uint8_t decimal_places;
    uint8_t time_format;
    unsigned char list_separator[2];
};

struct Os2NlsState {
    uint32_t country;
    uint32_t current_codepage;
    uint32_t prepared_codepages[OS2_NLS_MAX_PREPARED_CP];
    uint32_t prepared_count;
};

void os2_nls_state_init(struct Os2NlsState *state);
int os2_nls_set_country(struct Os2NlsState *state, uint32_t country);
int os2_nls_has_codepage(uint32_t codepage);
const struct Os2NlsCodepage *os2_nls_codepage(uint32_t codepage);
const struct Os2NlsCountryProfile *os2_nls_country(uint32_t country);

uint32_t os2_nls_set_process_cp(struct Os2NlsState *state,
                                uint32_t codepage);
uint32_t os2_nls_query_cp(const struct Os2NlsState *state,
                          uint32_t buffer_bytes,
                          uint32_t *codepages,
                          uint32_t *actual_bytes);
uint32_t os2_nls_query_country_info(const struct Os2NlsState *state,
                                    uint32_t requested_country,
                                    uint32_t requested_codepage,
                                    uint32_t buffer_bytes,
                                    unsigned char *buffer,
                                    uint32_t *actual_bytes);
uint32_t os2_nls_query_dbcs_env(const struct Os2NlsState *state,
                                uint32_t requested_country,
                                uint32_t requested_codepage,
                                uint32_t buffer_bytes,
                                unsigned char *buffer);
uint32_t os2_nls_map_case(const struct Os2NlsState *state,
                          uint32_t requested_country,
                          uint32_t requested_codepage,
                          unsigned char *buffer,
                          uint32_t buffer_bytes);

#endif
