#include <stdio.h>

#include "windows.h"
#include "os2_nls.h"
#include "os2_nls_win32.h"

static int failures;

static void check(int ok, const char *message)
{
    if (!ok) {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

int main(void)
{
    struct Os2NlsState first;
    struct Os2NlsState second;

    nls_stub_set_country(44UL, 1);
    nls_stub_set_oemcp(850U);
    os2_nls_win32_init_session(&first);
    check(first.country == 44U, "Win32 country bootstrap accepted");
    check(first.current_codepage == 850U,
          "Win32 OEM codepage bootstrap accepted");

    nls_stub_set_country(999UL, 1);
    nls_stub_set_oemcp(932U);
    os2_nls_win32_init_session(&second);
    check(second.country == 1U,
          "unsupported host country does not leak into OS/2 state");
    check(second.current_codepage == 437U,
          "unsupported host codepage does not leak into OS/2 state");

    nls_stub_set_country(44UL, 0);
    nls_stub_set_oemcp(0U);
    os2_nls_win32_init_session(&second);
    check(second.country == 1U && second.current_codepage == 437U,
          "missing host profile leaves deterministic OS/2 defaults");

    check(os2_nls_set_process_cp(&first, 437U) == 0U &&
          first.current_codepage == 437U &&
          second.current_codepage == 437U,
          "sessions remain independently mutable after bootstrap");

    if (failures != 0) {
        fprintf(stderr, "nls-win32-shim-check: %d failure(s)\n", failures);
        return 1;
    }
    puts("nls-win32-shim-check: PASS");
    return 0;
}
