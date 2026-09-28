#include <string.h>
#include "windows.h"

static DWORD stub_country = 1UL;
static int stub_country_available = 1;
static UINT stub_oemcp = 437U;

void nls_stub_set_country(DWORD country, int available)
{
    stub_country = country;
    stub_country_available = available;
}

void nls_stub_set_oemcp(UINT codepage)
{
    stub_oemcp = codepage;
}

int GetLocaleInfoA(DWORD locale, DWORD type, LPSTR data, int cchData)
{
    (void)locale;
    if (!stub_country_available || data == NULL ||
        cchData < (int)sizeof(stub_country) ||
        type != (LOCALE_ICOUNTRY | LOCALE_RETURN_NUMBER))
        return 0;
    memcpy(data, &stub_country, sizeof(stub_country));
    return (int)sizeof(stub_country);
}

UINT GetOEMCP(void)
{
    return stub_oemcp;
}
