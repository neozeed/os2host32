#ifndef OS2_NLS_API_H
#define OS2_NLS_API_H

#include "os2_personality.h"

os2_api_ret_t os2_nls_api_DosSetProcessCp(
    struct Os2PersonalityContext *context, uint32_t codepage);
os2_api_ret_t os2_nls_api_DosQueryCp(
    struct Os2PersonalityContext *context, uint32_t cb,
    os2_addr32_t codepages_address, os2_addr32_t actual_address);
os2_api_ret_t os2_nls_api_DosQueryCtryInfo(
    struct Os2PersonalityContext *context, uint32_t cb,
    os2_addr32_t countrycode_address, os2_addr32_t countryinfo_address,
    os2_addr32_t actual_address);
os2_api_ret_t os2_nls_api_DosQueryDBCSEnv(
    struct Os2PersonalityContext *context, uint32_t cb,
    os2_addr32_t countrycode_address, os2_addr32_t buffer_address);
os2_api_ret_t os2_nls_api_DosMapCase(
    struct Os2PersonalityContext *context, uint32_t cb,
    os2_addr32_t countrycode_address, os2_addr32_t buffer_address);

#endif
