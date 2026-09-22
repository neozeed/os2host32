/* Portable regression for the M28B CMD-owned environment block. */
#include <stdio.h>
#include "cmdos2.h"
#include "cmdos2_env.h"

static int same(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (*a != *b)
            return 0;
        ++a;
        ++b;
    }
    return *a == *b;
}

static int has_entry(const char *block, const char *want)
{
    const char *p;
    const char *a;
    const char *b;
    p = block;
    while (*p != '\0') {
        a = p;
        b = want;
        while (*a != '\0' && *b != '\0' && *a == *b) {
            ++a;
            ++b;
        }
        if (*a == '\0' && *b == '\0')
            return 1;
        while (*p != '\0')
            ++p;
        ++p;
    }
    return 0;
}

int main(void)
{
    static const char initial[] =
        "=C:=C:\\M28B\0Path=C:\\OS2;C:\\TOOLS\0Foo=outer\0\0";
    char value[128];
    char snapshot[1024];
    unsigned long actual;

    if (!CmdEnvStoreInit(initial))
        return 1;
    if (!CmdEnvStoreApplyOs2Path())
        return 11;
    if (CmdO2EnvGet("PATH", value, sizeof(value)) != 0 ||
        !same(value, "."))
        return 12;
    if (CmdO2EnvGet("OS2PATH", value, sizeof(value)) != 0 ||
        !same(value, "."))
        return 13;
    if (CmdO2EnvGet("foo", value, sizeof(value)) != 0 ||
        !same(value, "outer"))
        return 2;
    if (CmdO2EnvSet("FOO", "inner") != 0)
        return 3;
    if (CmdO2EnvGet("foo", value, sizeof(value)) != 0 ||
        !same(value, "inner"))
        return 4;
    actual = 0;
    if (CmdO2EnvExport(snapshot, sizeof(snapshot), &actual) != 0)
        return 5;
    if (!has_entry(snapshot, "=C:=C:\\M28B"))
        return 6;
    if (CmdO2EnvSet("FOO", (const char *)0) != 0)
        return 7;
    if (CmdO2EnvGet("foo", value, sizeof(value)) !=
        CMDO2_ERROR_ENVVAR_NOT_FOUND)
        return 8;
    if (CmdO2EnvImport(snapshot) != 0)
        return 9;
    if (CmdO2EnvGet("FOO", value, sizeof(value)) != 0 ||
        !same(value, "inner"))
        return 10;
    puts("M28B_ENV_STORE_OK\nM29N1_OS2PATH_NAMESPACE_OK");
    return 0;
}
