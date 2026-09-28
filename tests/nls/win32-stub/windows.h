#ifndef NLS_TEST_WINDOWS_H
#define NLS_TEST_WINDOWS_H

#include <stddef.h>

typedef unsigned long DWORD;
typedef unsigned int UINT;
typedef char *LPSTR;

#define LOCALE_USER_DEFAULT 0x0400UL
#define LOCALE_ICOUNTRY 0x00000005UL
#define LOCALE_RETURN_NUMBER 0x20000000UL

int GetLocaleInfoA(DWORD locale, DWORD type, LPSTR data, int cchData);
UINT GetOEMCP(void);

void nls_stub_set_country(DWORD country, int available);
void nls_stub_set_oemcp(UINT codepage);

#endif
