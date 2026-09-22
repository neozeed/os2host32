/* Portable M24 cbatch.c regression harness. */
#include "cmdbatch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct TestHost {
    struct CmdBatchState batch;
    int last_rc;
};

static char *skipws(char *p)
{
    while (*p == ' ' || *p == '\t')
        ++p;
    return p;
}

static int ci(const char *a, const char *b)
{
    while (*a && *b) {
        int ca = toupper((unsigned char)*a++);
        int cb = toupper((unsigned char)*b++);
        if (ca != cb)
            return ca - cb;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static char *split(char *line, char *cmd, size_t cap)
{
    char *p;
    size_t n;
    p = skipws(line);
    n = 0;
    while (*p && *p != ' ' && *p != '\t') {
        if (n + 1 < cap)
            cmd[n++] = *p;
        ++p;
    }
    cmd[n] = '\0';
    return skipws(p);
}

static int exists_cb(void *ctx, const char *path)
{
    FILE *fp;
    (void)ctx;
    fp = fopen(path, "rb");
    if (fp == NULL)
        return 0;
    fclose(fp);
    return 1;
}

static int test_run_line(void *ctx, char *raw, int *want_exit)
{
    struct TestHost *h;
    char line[CMDBATCH_LINE_MAX];
    char cmd[64];
    char *tail;
    int rc;

    h = (struct TestHost *)ctx;
    if (!CmdBatchExpandLine(&h->batch, raw, line, sizeof(line)))
        return 1;
    tail = split(line, cmd, sizeof(cmd));
    rc = h->last_rc;

    if (ci(cmd, "ECHO") == 0) {
        puts(tail);
        rc = 0;
    } else if (ci(cmd, "GOTO") == 0) {
        rc = CmdBatchGoto(&h->batch, tail);
    } else if (ci(cmd, "SHIFT") == 0) {
        rc = CmdBatchShift(&h->batch);
    } else if (ci(cmd, "IF") == 0) {
        rc = CmdBatchIf(&h->batch, tail, h->last_rc, want_exit);
    } else if (ci(cmd, "FOR") == 0) {
        rc = CmdBatchFor(&h->batch, tail, want_exit);
    } else if (ci(cmd, "CALL") == 0) {
        if (*tail == ':') {
            rc = CmdBatchCallLabel(&h->batch, tail, want_exit);
        } else {
            char file[256];
            char *args;
            args = split(tail, file, sizeof(file));
            rc = CmdBatchRunFile(&h->batch, file, args, want_exit);
        }
    } else if (ci(cmd, "STATUS0") == 0) {
        rc = 0;
    } else if (ci(cmd, "STATUS7") == 0) {
        rc = 7;
    } else if (ci(cmd, "EXIT") == 0) {
        *want_exit = 1;
        rc = 0;
    } else {
        printf("EXEC[%s]\n", line);
        rc = 0;
    }
    h->last_rc = rc;
    return rc;
}

static int write_file(const char *path, const char *text)
{
    FILE *fp;
    fp = fopen(path, "wb");
    if (fp == NULL)
        return 0;
    if (fwrite(text, 1, strlen(text), fp) != strlen(text)) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    return 1;
}

int main(void)
{
    struct TestHost h;
    int want_exit;
    int rc;

    memset(&h, 0, sizeof(h));
    CmdBatchInit(&h.batch, &h, test_run_line, exists_cb);

    if (!write_file("m24-child.cmd",
                    "echo child0=%0 child1=%1 child2=%2\n"
                    "shift\n"
                    "echo shifted1=%1 shifted2=%2\n") ||
        !write_file("m24-parent.cmd",
                    "echo arg0=%0 arg1=%1 arg2=%2\n"
                    "status7\n"
                    "if errorlevel 7 echo errorlevel-ok\n"
                    "if not exist definitely-missing.file echo not-exist-ok\n"
                    "if \"a\"==\"a\" echo strcmp-ok\n"
                    "for %%i in (one two three) do echo for=%%i\n"
                    "call m24-child.cmd alpha beta\n"
                    "call :sub q r\n"
                    "goto done\n"
                    "echo SHOULD-NOT-PRINT\n"
                    ":sub\n"
                    "echo sub1=%1 sub2=%2\n"
                    "goto :eof\n"
                    ":done\n"
                    "echo done\n")) {
        return 2;
    }

    want_exit = 0;
    rc = CmdBatchRunFile(&h.batch, "m24-parent.cmd", "first second",
                         &want_exit);
    CmdBatchDone(&h.batch);
    remove("m24-parent.cmd");
    remove("m24-child.cmd");
    return rc;
}
