#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "os2_nls.h"
#include "os2_nls_backend.h"
#include "os2_nls_win32.h"

static int win32_query_initial_profile(void *opaque,
                                       uint32_t *country,
                                       uint32_t *codepage)
{
    DWORD host_country;
    UINT host_codepage;
    int have_value;

    (void)opaque;
    if (country == NULL || codepage == NULL)
        return 0;

    *country = 0u;
    *codepage = 0u;
    have_value = 0;

    host_country = 0;
    if (GetLocaleInfoA(LOCALE_USER_DEFAULT,
                       LOCALE_ICOUNTRY | LOCALE_RETURN_NUMBER,
                       (LPSTR)&host_country, (int)sizeof(host_country)) != 0) {
        *country = (uint32_t)host_country;
        have_value = 1;
    }

    host_codepage = GetOEMCP();
    if (host_codepage != 0u) {
        *codepage = (uint32_t)host_codepage;
        have_value = 1;
    }

    return have_value;
}

static const struct Os2NlsBackendOps win32_nls_ops = {
    win32_query_initial_profile
};

void os2_nls_win32_init_session(struct Os2NlsState *state)
{
    os2_nls_session_init(state, NULL, &win32_nls_ops);
}
