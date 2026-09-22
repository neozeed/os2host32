/* Portable M29O cbatch torture regression: no OS/2/Win32 dependencies. */
#include "cmdbatch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_CAPTURE 32

struct Host {
    struct CmdBatchState batch;
    char *capture[MAX_CAPTURE];
    int capture_count;
    int last_rc;
};

static char *dupstr(const char *s)
{
    size_t n;
    char *p;
    n = strlen(s) + 1;
    p = (char *)malloc(n);
    if (p != NULL)
        memcpy(p, s, n);
    return p;
}

static char *skipws(char *p)
{
    while (*p == ' ' || *p == '\t')
        ++p;
    return p;
}

static int ci(const char *a, const char *b)
{
    while (*a && *b) {
        int ca;
        int cb;
        ca = toupper((unsigned char)*a++);
        cb = toupper((unsigned char)*b++);
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

static int capture(struct Host *h, const char *s)
{
    if (h->capture_count >= MAX_CAPTURE)
        return 1;
    h->capture[h->capture_count] = dupstr(s);
    if (h->capture[h->capture_count] == NULL)
        return 1;
    ++h->capture_count;
    return 0;
}

static int run_line(void *ctx, char *raw, int *want_exit)
{
    struct Host *h;
    char line[CMDBATCH_LINE_MAX];
    char cmd[64];
    char file[256];
    char *tail;
    char *args;
    int rc;

    h = (struct Host *)ctx;
    if (!CmdBatchExpandLine(&h->batch, raw, line, sizeof(line)))
        return 1;
    tail = split(line, cmd, sizeof(cmd));
    rc = h->last_rc;

    if (ci(cmd, "CAPTURE") == 0) {
        rc = capture(h, tail);
    } else if (ci(cmd, "SHIFT") == 0) {
        rc = CmdBatchShift(&h->batch);
    } else if (ci(cmd, "GOTO") == 0) {
        rc = CmdBatchGoto(&h->batch, tail);
    } else if (ci(cmd, "IF") == 0) {
        rc = CmdBatchIf(&h->batch, tail, h->last_rc, want_exit);
    } else if (ci(cmd, "FOR") == 0) {
        rc = CmdBatchFor(&h->batch, tail, want_exit);
    } else if (ci(cmd, "STATUS37") == 0) {
        rc = 37;
    } else if (ci(cmd, "CALL") == 0) {
        if (*tail == ':')
            rc = CmdBatchCallLabel(&h->batch, tail, want_exit);
        else {
            args = split(tail, file, sizeof(file));
            rc = CmdBatchRunFile(&h->batch, file, args, want_exit);
        }
    } else if (ci(cmd, "CHAIN") == 0) {
        args = split(tail, file, sizeof(file));
        CmdBatchEndCurrent(&h->batch);
        rc = CmdBatchRunFile(&h->batch, file, args, want_exit);
    } else {
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
    static const char *expected[] = {
        "P0=[m29o-host-parent.cmd] P1=[first] P2=[\"two words\"] P3=[\"\"] P4=[fourth] STAR=[first \"two words\" \"\" fourth]",
        "S1=[alpha] S2=[\"beta gamma\"] S3=[\"\"] S4=[delta] STAR=[alpha \"beta gamma\" \"\" delta]",
        "SHIFT1=[\"beta gamma\"] SHIFT2=[\"\"] SHIFT3=[delta] STAR=[alpha \"beta gamma\" \"\" delta]",
        "AFTER_SUB",
        "NESTED_IF",
        "FOR=one-A",
        "FOR=one-B",
        "FOR=two-A",
        "FOR=two-B",
        "CHAIN_PARENT_BEFORE",
        "CHAIN_TARGET",
        "RETURNED_FROM_CALLED_CHAIN_PARENT",
        "GOTO_OK"
    };
    struct Host h;
    int want_exit;
    int rc;
    int i;

    memset(&h, 0, sizeof(h));
    CmdBatchInit(&h.batch, &h, run_line, exists_cb);

    if (!write_file("m29o-host-chain-target.cmd",
                    "capture CHAIN_TARGET\n") ||
        !write_file("m29o-host-chain-parent.cmd",
                    "capture CHAIN_PARENT_BEFORE\n"
                    "chain m29o-host-chain-target.cmd\n"
                    "capture CHAIN_PARENT_AFTER_BAD\n") ||
        !write_file("m29o-host-parent.cmd",
                    "capture P0=[%0] P1=[%1] P2=[%2] P3=[%3] P4=[%4] STAR=[%*]\n"
                    "call :sub alpha \"beta gamma\" \"\" delta\n"
                    "capture AFTER_SUB\n"
                    "status37\n"
                    "if errorlevel 37 if not errorlevel 38 capture NESTED_IF\n"
                    "for %%i in (one two) do for %%j in (A B) do capture FOR=%%i-%%j\n"
                    "call m29o-host-chain-parent.cmd\n"
                    "capture RETURNED_FROM_CALLED_CHAIN_PARENT\n"
                    "goto done\n"
                    "capture GOTO_BAD\n"
                    ":sub\n"
                    "capture S1=[%1] S2=[%2] S3=[%3] S4=[%4] STAR=[%*]\n"
                    "shift\n"
                    "capture SHIFT1=[%1] SHIFT2=[%2] SHIFT3=[%3] STAR=[%*]\n"
                    "goto :eof\n"
                    ":done\n"
                    "capture GOTO_OK\n")) {
        return 2;
    }

    want_exit = 0;
    rc = CmdBatchRunFile(&h.batch, "m29o-host-parent.cmd",
                         "first \"two words\" \"\" fourth", &want_exit);
    CmdBatchDone(&h.batch);

    remove("m29o-host-parent.cmd");
    remove("m29o-host-chain-parent.cmd");
    remove("m29o-host-chain-target.cmd");

    if (rc != 0 || h.capture_count != (int)(sizeof(expected) / sizeof(expected[0]))) {
        fprintf(stderr, "M29O host batch count/rc failure rc=%d count=%d\n",
                rc, h.capture_count);
        return 1;
    }
    for (i = 0; i < h.capture_count; ++i) {
        if (strcmp(h.capture[i], expected[i]) != 0) {
            fprintf(stderr, "M29O capture %d mismatch\n got=[%s]\n exp=[%s]\n",
                    i, h.capture[i], expected[i]);
            return 1;
        }
    }
    for (i = 0; i < h.capture_count; ++i)
        free(h.capture[i]);

    puts("M29O_BATCH_HOST_TORTURE_OK");
    return 0;
}
