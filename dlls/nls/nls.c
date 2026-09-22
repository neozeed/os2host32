/*
 * nls.c - minimal Win32 personality for OS/2 32-bit NLS.DLL.
 *
 * Milestone 30G implements the two NLS entry points imported by the EMX
 * runtime under test. M31G R17 adds the 32-bit country-format query reached
 * by the OS/2 2.0 E editor:
 *
 *      5  DosQueryCtryInfo
 *      6  DosQueryDBCSEnv
 *      7  DosMapCase
 *
 * The current host personality models a western SBCS process.  Therefore
 * DosQueryDBCSEnv returns an empty (zero-terminated) DBCS lead-byte vector.
 * DosMapCase performs the ASCII subset of OS/2 case mapping.  This is enough
 * for the EMX startup path and deliberately avoids pretending that the host
 * Windows ANSI code page is byte-compatible with an OS/2 OEM code page.
 *
 * ANSI C89 / old Win32 SDK friendly by design.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef __cdecl
#define __cdecl
#endif

typedef unsigned long O2ULONG;
typedef unsigned long O2APIRET;
typedef unsigned short O2USHORT;
typedef unsigned char O2UCHAR;

typedef struct O2COUNTRYCODE {
    O2ULONG country;
    O2ULONG codepage;
} O2COUNTRYCODE;

#define O2_NO_ERROR                     0UL
#define O2_ERROR_INVALID_PARAMETER     87UL
#define O2_ERROR_NO_COUNTRY_OR_CODEPAGE 398UL
#define O2_ERROR_NLS_TABLE_TRUNCATED   399UL

static int nls_trace_enabled(void);

static int nls_trace_enabled(void)
{
    return getenv("OS2_TRACE_NLS") != NULL ||
           getenv("OS2_TRACE_MODULES") != NULL;
}

typedef struct O2COUNTRYINFO {
    O2ULONG country;
    O2ULONG codepage;
    O2ULONG fsDateFmt;
    char szCurrency[5];
    char szThousandsSeparator[2];
    char szDecimal[2];
    char szDateSeparator[2];
    char szTimeSeparator[2];
    O2UCHAR fsCurrencyFmt;
    O2UCHAR cDecimalPlace;
    O2UCHAR fsTimeFmt;
    O2USHORT abReserved1[2];
    char szDataSeparator[2];
    O2USHORT abReserved2[5];
} O2COUNTRYINFO;

static O2ULONG nls_locale_number(LCTYPE type, O2ULONG fallback)
{
    DWORD value;
    int n;
    value = (DWORD)fallback;
    n = GetLocaleInfoA(LOCALE_USER_DEFAULT, type | LOCALE_RETURN_NUMBER,
                       (LPSTR)&value, (int)sizeof(value));
    return n != 0 ? (O2ULONG)value : fallback;
}

static void nls_locale_string(LCTYPE type, char *out, O2ULONG cap,
                              const char *fallback)
{
    char tmp[64];
    int n;
    O2ULONG copy;
    if (!out || cap == 0UL)
        return;
    tmp[0] = 0;
    n = GetLocaleInfoA(LOCALE_USER_DEFAULT, type, tmp, (int)sizeof(tmp));
    if (n == 0 || tmp[0] == 0) {
        strncpy(out, fallback, (size_t)(cap - 1UL));
        out[cap - 1UL] = 0;
        return;
    }
    copy = (O2ULONG)strlen(tmp);
    if (copy >= cap)
        copy = cap - 1UL;
    memcpy(out, tmp, (size_t)copy);
    out[copy] = 0;
}

/* NLS.5 / OS/2 2.x DosQueryCtryInfo.
 *
 * The 32-bit API uses ULONG country/codepage/date-format fields and reports a
 * 44-byte COUNTRYINFO structure.  Country/codepage zero request the process
 * defaults.  V1 maps those defaults to the host user's locale plus the host
 * OEM code page; explicit foreign country/codepage requests are rejected
 * rather than returning formatting data for the wrong locale.
 */
O2APIRET __cdecl DosQueryCtryInfo(O2ULONG cb, const O2COUNTRYCODE *pcc,
                                  O2COUNTRYINFO *pci, O2ULONG *pcbActual)
{
    O2COUNTRYINFO ci;
    O2ULONG host_country;
    O2ULONG host_cp;
    O2ULONG wanted_country;
    O2ULONG wanted_cp;
    O2ULONG copy;
    O2ULONG currency_fmt;

    if (!pci || !pcbActual)
        return O2_ERROR_INVALID_PARAMETER;

    host_country = nls_locale_number(LOCALE_ICOUNTRY, 1UL);
    host_cp = (O2ULONG)GetOEMCP();
    if (host_cp == 0UL)
        host_cp = 850UL;

    wanted_country = pcc ? pcc->country : 0UL;
    wanted_cp = pcc ? pcc->codepage : 0UL;
    if (wanted_country != 0UL && wanted_country != host_country)
        return O2_ERROR_NO_COUNTRY_OR_CODEPAGE;
    if (wanted_cp != 0UL && wanted_cp != host_cp)
        return O2_ERROR_NO_COUNTRY_OR_CODEPAGE;

    memset(&ci, 0, sizeof(ci));
    ci.country = wanted_country ? wanted_country : host_country;
    ci.codepage = wanted_cp ? wanted_cp : host_cp;
    ci.fsDateFmt = nls_locale_number(LOCALE_IDATE, 0UL);
    nls_locale_string(LOCALE_SCURRENCY, ci.szCurrency,
                      (O2ULONG)sizeof(ci.szCurrency), "$");
    nls_locale_string(LOCALE_STHOUSAND, ci.szThousandsSeparator,
                      (O2ULONG)sizeof(ci.szThousandsSeparator), ",");
    nls_locale_string(LOCALE_SDECIMAL, ci.szDecimal,
                      (O2ULONG)sizeof(ci.szDecimal), ".");
    nls_locale_string(LOCALE_SDATE, ci.szDateSeparator,
                      (O2ULONG)sizeof(ci.szDateSeparator), "/");
    nls_locale_string(LOCALE_STIME, ci.szTimeSeparator,
                      (O2ULONG)sizeof(ci.szTimeSeparator), ":");
    currency_fmt = nls_locale_number(LOCALE_ICURRENCY, 0UL);
    ci.fsCurrencyFmt = (O2UCHAR)(currency_fmt & 3UL);
    ci.cDecimalPlace = (O2UCHAR)nls_locale_number(LOCALE_ICURRDIGITS, 2UL);
    ci.fsTimeFmt = (O2UCHAR)(nls_locale_number(LOCALE_ITIME, 0UL) ? 1 : 0);
    nls_locale_string(LOCALE_SLIST, ci.szDataSeparator,
                      (O2ULONG)sizeof(ci.szDataSeparator), ",");

    *pcbActual = (O2ULONG)sizeof(ci);
    if (cb != 0UL)
        memset(pci, 0, (size_t)cb);
    copy = cb < (O2ULONG)sizeof(ci) ? cb : (O2ULONG)sizeof(ci);
    if (copy != 0UL)
        memcpy(pci, &ci, (size_t)copy);

    if (nls_trace_enabled()) {
        printf("M31G R17 NLS: DosQueryCtryInfo cb=%lu country=%lu cp=%lu -> host country=%lu cp=%lu datefmt=%lu size=%lu%s\n",
               cb, wanted_country, wanted_cp, ci.country, ci.codepage,
               ci.fsDateFmt, (O2ULONG)sizeof(ci),
               cb < (O2ULONG)sizeof(ci) ? " TRUNCATED" : "");
        fflush(stdout);
    }
    return cb < (O2ULONG)sizeof(ci) ? O2_ERROR_NLS_TABLE_TRUNCATED : O2_NO_ERROR;
}

/* NLS.6 / OS/2 2.x DosQueryDBCSEnv.
 *
 * The result is a sequence of inclusive lead-byte pairs, terminated by
 * 00,00.  A western SBCS process has no lead-byte ranges, so a zero-filled
 * buffer is the correct answer for country/codepage 0 (current defaults).
 */
O2APIRET __cdecl DosQueryDBCSEnv(O2ULONG cb, const O2COUNTRYCODE *pcc,
                                 O2UCHAR *buf)
{
    if (buf == NULL || cb == 0UL)
        return O2_ERROR_INVALID_PARAMETER;

    memset(buf, 0, (size_t)cb);

    if (nls_trace_enabled()) {
        O2ULONG country;
        O2ULONG codepage;
        country = pcc ? pcc->country : 0UL;
        codepage = pcc ? pcc->codepage : 0UL;
        printf("M30G NLS: DosQueryDBCSEnv cb=%lu country=%lu cp=%lu -> SBCS empty vector\n",
               cb, country, codepage);
        fflush(stdout);
    }
    return O2_NO_ERROR;
}

/* NLS.7 / OS/2 2.x DosMapCase.
 *
 * Preserve bytes >= 0x80 until we add explicit OS/2 OEM-code-page tables;
 * mapping them through the Windows ACP would be subtly wrong.  EMX startup
 * only needs the ordinary ASCII behavior here.
 */
O2APIRET __cdecl DosMapCase(O2ULONG cb, const O2COUNTRYCODE *pcc,
                            O2UCHAR *s)
{
    O2ULONG i;
    if (s == NULL && cb != 0UL)
        return O2_ERROR_INVALID_PARAMETER;

    for (i = 0UL; i < cb; ++i) {
        if (s[i] >= (O2UCHAR)'a' && s[i] <= (O2UCHAR)'z')
            s[i] = (O2UCHAR)(s[i] - (O2UCHAR)'a' + (O2UCHAR)'A');
    }

    if (nls_trace_enabled()) {
        O2ULONG country;
        O2ULONG codepage;
        country = pcc ? pcc->country : 0UL;
        codepage = pcc ? pcc->codepage : 0UL;
        printf("M30G NLS: DosMapCase cb=%lu country=%lu cp=%lu -> rc=0\n",
               cb, country, codepage);
        fflush(stdout);
    }
    return O2_NO_ERROR;
}
