/* M28B child-inheritance probe: genuine C/386 OS/2 LX program. */
#define INCL_DOS
#include <os2.h>
#include <stdio.h>

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

int main(void)
{
    PSZ value;
    APIRET rc;

    value = (PSZ)0;
    rc = DosScanEnv("M28BTEST", &value);
    if (rc != 0 || value == (PSZ)0) {
        printf("env-child: DosScanEnv rc=%lu\n", (unsigned long)rc);
        return 7;
    }
    printf("env-child: M28BTEST=[%s]\n", value);
    if (!same(value, "from-parent"))
        return 8;
    return 0;
}
