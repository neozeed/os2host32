/* Portable M29O1 parser regression for nested FOR variable preservation. */
#include "cmdparse.h"

#include <stdio.h>
#include <string.h>

static int expect_simple(const char *line, const char *expected)
{
    struct CmdParseResult r;
    int ok;

    if (!CmdParser(line, &r)) {
        fprintf(stderr, "M29O1 parser rejected [%s]: %s\n", line, r.error);
        return 0;
    }
    ok = r.root != NULL && r.root->type == CMD_NODE_SIMPLE &&
         r.root->text != NULL && strcmp(r.root->text, expected) == 0;
    if (!ok) {
        fprintf(stderr, "M29O1 parser mismatch\n input=[%s]\n", line);
        if (r.root != NULL && r.root->text != NULL)
            fprintf(stderr, " got=[%s]\n", r.root->text);
        fprintf(stderr, " exp=[%s]\n", expected);
    }
    CmdFreeTree(r.root);
    return ok;
}

int main(void)
{
    static const char nested[] =
        "for %i in (one two) do for %j in (A B) do echo M29O_NESTED_FOR=%i-%j";
    static const char punctuation[] =
        "for %x in (a b) do echo [%x:%x/%x-%x]";

    if (!expect_simple(nested, nested))
        return 1;
    if (!expect_simple(punctuation, punctuation))
        return 1;

    puts("M29O1_NESTED_FOR_PARSER_OK");
    return 0;
}
