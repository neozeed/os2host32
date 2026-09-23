/* Small Win32-hosted smoke test for the OS/2 personality NLS ABI. */
#include <stdio.h>
#include <string.h>

#ifndef __cdecl
#define __cdecl
#endif

typedef unsigned long ULONG;
typedef unsigned long APIRET;
typedef unsigned char UCHAR;

typedef struct COUNTRYCODE {
    ULONG country;
    ULONG codepage;
} COUNTRYCODE;

typedef struct COUNTRYINFO {
    ULONG country;
    ULONG codepage;
    ULONG fsDateFmt;
    char szCurrency[5];
    char szThousandsSeparator[2];
    char szDecimal[2];
    char szDateSeparator[2];
    char szTimeSeparator[2];
    UCHAR fsCurrencyFmt;
    UCHAR cDecimalPlace;
    UCHAR fsTimeFmt;
    unsigned short abReserved1[2];
    char szDataSeparator[2];
    unsigned short abReserved2[5];
} COUNTRYINFO;

APIRET __cdecl DosQueryCp(ULONG, ULONG *, ULONG *);
APIRET __cdecl DosSetProcessCp(ULONG);
APIRET __cdecl DosQueryCtryInfo(ULONG, const COUNTRYCODE *, COUNTRYINFO *, ULONG *);
APIRET __cdecl DosQueryDBCSEnv(ULONG, const COUNTRYCODE *, UCHAR *);
APIRET __cdecl DosMapCase(ULONG, const COUNTRYCODE *, UCHAR *);

int main(void)
{
    ULONG cp[8];
    ULONG actual;
    ULONG count;
    ULONG i;
    APIRET rc;
    COUNTRYCODE cc;
    COUNTRYINFO ci;
    UCHAR dbcs[12];
    UCHAR sample[4];

    memset(cp, 0, sizeof(cp));
    actual = 0;
    rc = DosQueryCp((ULONG)sizeof(cp), cp, &actual);
    if (rc != 0 && rc != 473) {
        printf("DosQueryCp failed rc=%lu\n", rc);
        return 1;
    }
    count = actual / (ULONG)sizeof(cp[0]);
    printf("OS/2 NLS information\n\n");
    printf("Current codepage ... %lu\n", count ? cp[0] : 0UL);
    printf("Available CPs ......");
    for (i = 0; i < count; ++i)
        printf(" %lu", cp[i]);
    printf("\n");

    cc.country = 0;
    cc.codepage = 0;
    memset(&ci, 0, sizeof(ci));
    actual = 0;
    rc = DosQueryCtryInfo((ULONG)sizeof(ci), &cc, &ci, &actual);
    if (rc != 0) {
        printf("DosQueryCtryInfo failed rc=%lu\n", rc);
        return 1;
    }
    printf("Country ............ %03lu\n", ci.country);
    printf("Character set ...... SBCS\n");
    printf("Date format ........ %lu\n", ci.fsDateFmt);
    printf("Time separator ..... %s\n", ci.szTimeSeparator);
    printf("Decimal separator .. %s\n", ci.szDecimal);
    printf("Thousands separator %s\n", ci.szThousandsSeparator);
    printf("Currency ........... %s\n", ci.szCurrency);

    memset(dbcs, 0xcc, sizeof(dbcs));
    rc = DosQueryDBCSEnv((ULONG)sizeof(dbcs), &cc, dbcs);
    printf("DBCS ranges ........ %s (rc=%lu)\n",
           dbcs[0] == 0 && dbcs[1] == 0 ? "none" : "present", rc);

    sample[0] = (UCHAR)'a';
    sample[1] = 0x82;
    sample[2] = 0x81;
    sample[3] = 0;
    printf("Map-case before .... %02X %02X %02X\n",
           sample[0], sample[1], sample[2]);
    rc = DosMapCase(3, &cc, sample);
    printf("Map-case after ..... %02X %02X %02X (rc=%lu)\n",
           sample[0], sample[1], sample[2], rc);
    return rc == 0 ? 0 : 1;
}
