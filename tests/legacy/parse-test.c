/* Portable M23 parser regression harness. */
#include "cmdparse.h"
#include <stdio.h>
#include <stdlib.h>

static int one(const char *line)
{
    struct CmdParseResult r;
    printf("INPUT: %s\n", line);
    if (!CmdParser(line, &r)) {
        printf("ERROR @ %d: %s\n\n", r.error_offset, r.error);
        return 1;
    }
    CmdDumpTree(r.root, 0);
    CmdFreeTree(r.root);
    putchar('\n');
    return 0;
}

int main(void)
{
    int rc;
    rc = 0;
    rc |= one("echo one & echo two || echo three && hi | args 1 2 3");
    rc |= one("(echo grouped & hi) && args one two three");
    rc |= one("@echo %PATH% > out.txt");
    rc |= one("echo \"a&b|c\" && hi");
    rc |= one("for %i in (one two three) do echo %i");
    rc |= one("if errorlevel 7 echo seven-or-more");
    rc |= one("m29m-stderr 2> err.txt");
    rc |= one("m29m-stderr > both.txt 2>&1");
    rc |= one("m29m-stderr 2>&1 > stdout.txt");
    rc |= one("emit-test | upper-test | upper-test > final.txt 2> pipe.err");
    rc |= one("\"m29n space\\quoted args\" \"one two\" \"\" three");
    rc |= one("set M29N_OLDPATH=C:\\Program Files (x86)\\Tools;C:\\OS2");
    rc |= one("set OS2PATH=.;C:\\OS2;C:\\OS2\\APPS");
    return rc;
}
