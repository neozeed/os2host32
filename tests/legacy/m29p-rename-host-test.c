#include <stdio.h>
#include <string.h>

/* Pull the static wildcard transform into this translation unit.  Unused
 * command/backend sections are discarded by the host-test link. */
#include "cmdfile.c"

static int check(const char *src, const char *pat, const char *want)
{
    char out[1024];
    if (!cf_rename_wild_name(src, pat, out, sizeof(out))) {
        fprintf(stderr, "transform failed: %s -> %s\n", src, pat);
        return 0;
    }
    if (strcmp(out, want) != 0) {
        fprintf(stderr, "transform mismatch: %s + %s -> [%s], want [%s]\n",
                src, pat, out, want);
        return 0;
    }
    return 1;
}

int main(void)
{
    if (!check("alpha.txt", "*.bak", "alpha.bak")) return 1;
    if (!check("ABC.TXT", "?BC.BAK", "ABC.BAK")) return 1;
    if (!check("ABC.TXT", "X*.BAK", "XBC.BAK")) return 1;
    if (!check("long-name.dat", "*.old", "long-name.old")) return 1;
    puts("M29P_RENAME_WILDCARD_HOST_OK");
    return 0;
}
