/*
 * cmdos2_os2_smoke.c - M28D direct OS/2 env/current-dir/handle/process probe.
 */
#include <stdio.h>
#include "cmdos2.h"

static char env_snapshot[32768];

static unsigned smoke_strlen(const char *s)
{
    const char *p;
    p = s;
    while (*p != '\0')
        ++p;
    return (unsigned)(p - s);
}

static int smoke_same(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (*a != *b)
            return 0;
        ++a;
        ++b;
    }
    return *a == *b;
}

static int smoke_ends_with(const char *s, const char *tail)
{
    unsigned ns;
    unsigned nt;
    ns = smoke_strlen(s);
    nt = smoke_strlen(tail);
    if (nt > ns)
        return 0;
    s += ns - nt;
    return smoke_same(s, tail);
}

int main(void)
{
    char cwd[1024];
    char value[256];
    struct CmdO2FindData fd;
    struct CmdO2ResultCodes results;
    CmdO2FindHandle dh;
    CmdO2Handle h;
    CmdO2Handle pipe_read;
    CmdO2Handle pipe_write;
    struct CmdO2StdToken stdout_token;
    unsigned long actual;
    unsigned long envbytes;
    unsigned long async_pid;
    unsigned long waited_pid;
    CmdO2Rc rc;
    static const char msg[] = "M28C direct OS/2 backend works.\r\n";
    static const char pipe_msg[] = "M28C_PIPE_CHILD_OK\r\n";
    static const char append_msg[] = "M28C_APPEND_OK\r\n";
    static const char redir_expected[] =
        "M28C_PIPE_CHILD_OK\r\nM28C_APPEND_OK\r\n";
    static char child_args[] = "env-child.exe\0\0";
    static char handle_child_args[] = "handle-child.exe\0\0";
    static char async_child_args[] = "async-child.exe\0\0";
    char pipe_buffer[64];
    char fail[260];

    if (!CmdO2Init()) {
        printf("CmdO2Init failed: %s\n", CmdO2InitError());
        return 1;
    }

    rc = CmdO2QueryCurrentDir(cwd, sizeof(cwd));
    if (rc != 0) {
        printf("DosQueryCurrentDir path failed: %lu\n", rc);
        return 2;
    }
    printf("cwd=[%s]\n", cwd);

    /* Exercise the default-disk/current-directory API on the genuine LX path.
     * Selecting the already-current disk is harmless but proves ordinal 220. */
    if (cwd[0] >= 'A' && cwd[0] <= 'Z' && cwd[1] == ':') {
        rc = CmdO2SetDefaultDisk((unsigned long)(cwd[0] - 'A' + 1));
        if (rc != 0) {
            printf("DosSetDefaultDisk rc=%lu\n", rc);
            return 20;
        }
    }
    (void)CmdO2DeleteDir("m28b-cd-probe");
    rc = CmdO2CreateDir("m28b-cd-probe");
    if (rc != 0) {
        printf("DosCreateDir(cwd probe) rc=%lu\n", rc);
        return 21;
    }
    rc = CmdO2SetCurrentDir("m28b-cd-probe");
    if (rc != 0) {
        printf("DosSetCurrentDir(cwd probe) rc=%lu\n", rc);
        return 22;
    }
    rc = CmdO2QueryCurrentDir(cwd, sizeof(cwd));
    if (rc != 0 || !smoke_ends_with(cwd, "\\m28b-cd-probe")) {
        printf("cwd probe failed rc=%lu cwd=[%s]\n", rc, cwd);
        return 23;
    }
    printf("cwd probe=[%s]\n", cwd);
    rc = CmdO2SetCurrentDir("..");
    if (rc != 0)
        return 24;
    rc = CmdO2DeleteDir("m28b-cd-probe");
    if (rc != 0)
        return 25;

    dh = CMDO2_FIND_CREATE;
    rc = CmdO2FindFirst("*.txt", &dh, &fd);
    if (rc == 0) {
        printf("first *.txt=[%s] size=%lu\n", fd.name, fd.size);
        CmdO2FindClose(dh);
    } else {
        printf("DosFindFirst(*.txt) rc=%lu\n", rc);
    }

    rc = CmdO2EnvSet("M28BTEST", "outer");
    if (rc != 0)
        return 10;
    rc = CmdO2EnvGet("M28BTEST", value, sizeof(value));
    if (rc != 0 || !smoke_same(value, "outer")) {
        printf("environment set/get failed rc=%lu value=[%s]\n", rc, value);
        return 11;
    }

    envbytes = 0;
    rc = CmdO2EnvExport(env_snapshot, sizeof(env_snapshot), &envbytes);
    if (rc != 0) {
        printf("environment snapshot failed rc=%lu size=%lu\n", rc, envbytes);
        return 12;
    }
    (void)CmdO2EnvSet("M28BTEST", "inner");
    rc = CmdO2EnvImport(env_snapshot);
    if (rc != 0)
        return 13;
    rc = CmdO2EnvGet("M28BTEST", value, sizeof(value));
    if (rc != 0 || !smoke_same(value, "outer")) {
        printf("environment restore failed rc=%lu value=[%s]\n", rc, value);
        return 14;
    }
    printf("environment snapshot/restore=[%s] block=%lu bytes\n", value, envbytes);

    /* Prove that a SET-like change is explicitly inherited by another genuine
     * LX process.  env-child.exe checks it with the real DosScanEnv API. */
    rc = CmdO2EnvSet("M28BTEST", "from-parent");
    if (rc != 0)
        return 15;
    fail[0] = '\0';
    results.codeTerminate = 0;
    results.codeResult = 0;
    rc = CmdO2ExecPgm(fail, sizeof(fail), 0UL, child_args,
                      (const char *)0, &results, "env-child.exe");
    if (rc != 0 || results.codeResult != 0) {
        printf("environment child inheritance failed rc=%lu child=%lu fail=[%s]\n",
               rc, results.codeResult, fail);
        return 16;
    }

    /* M28D: execute a genuine LX child asynchronously and collect its
     * result through the real DosWaitChild ABI. */
    fail[0] = '\0';
    results.codeTerminate = 0;
    results.codeResult = 0;
    rc = CmdO2ExecPgm(fail, sizeof(fail), CMDO2_EXEC_ASYNCRESULT,
                      async_child_args, (const char *)0, &results,
                      "async-child.exe");
    if (rc != 0 || results.codeTerminate == 0UL) {
        printf("async child start failed rc=%lu pid=%lu fail=[%s]\n",
               rc, results.codeTerminate, fail);
        return 17;
    }
    async_pid = results.codeTerminate;
    waited_pid = 0UL;
    results.codeTerminate = 0;
    results.codeResult = 0;
    rc = CmdO2WaitChild(CMDO2_WAIT_PROCESS, CMDO2_WAIT,
                        &results, &waited_pid, async_pid);
    if (rc != 0 || waited_pid != async_pid || results.codeResult != 7UL) {
        printf("async wait failed rc=%lu requested=%lu waited=%lu term=%lu result=%lu\n",
               rc, async_pid, waited_pid, results.codeTerminate,
               results.codeResult);
        return 18;
    }
    printf("async exec/wait=OK pid=%lu result=%lu\n",
           waited_pid, results.codeResult);

    /* M28C: prove genuine OS/2 HFILE pipe + DosDupHandle semantics across
     * an LX child.  The child writes directly to HFILE 1 with DosWrite. */
    pipe_read = 0;
    pipe_write = 0;
    rc = CmdO2CreatePipe(&pipe_read, &pipe_write, 4096UL);
    if (rc != 0) {
        printf("DosCreatePipe rc=%lu\n", rc);
        return 30;
    }
    stdout_token.value = CMDO2_HANDLE_ALLOCATE;
    stdout_token.os2_value = CMDO2_HANDLE_ALLOCATE;
    rc = CmdO2StdSave(1UL, &stdout_token);
    if (rc != 0) {
        printf("save stdout rc=%lu\n", rc);
        return 31;
    }
    rc = CmdO2StdRedirectHandle(1UL, pipe_write);
    if (rc != 0) {
        printf("redirect stdout to pipe rc=%lu\n", rc);
        return 32;
    }
    CmdO2Close(pipe_write);

    fail[0] = '\0';
    results.codeTerminate = 0;
    results.codeResult = 0;
    rc = CmdO2ExecPgm(fail, sizeof(fail), 0UL, handle_child_args,
                      (const char *)0, &results, "handle-child.exe");
    (void)CmdO2StdRestore(1UL, &stdout_token);
    if (rc != 0 || results.codeResult != 0) {
        printf("pipe child failed rc=%lu child=%lu fail=[%s]\n",
               rc, results.codeResult, fail);
        return 33;
    }

    actual = 0;
    rc = CmdO2Read(pipe_read, pipe_buffer,
                   (unsigned long)smoke_strlen(pipe_msg), &actual);
    CmdO2Close(pipe_read);
    if (rc != 0 || actual != (unsigned long)smoke_strlen(pipe_msg)) {
        printf("pipe read failed rc=%lu actual=%lu\n", rc, actual);
        return 34;
    }
    pipe_buffer[actual] = '\0';
    if (!smoke_same(pipe_buffer, pipe_msg)) {
        printf("pipe payload mismatch=[%s]\n", pipe_buffer);
        return 35;
    }
    printf("pipe child exact payload=OK (%lu bytes)\n", actual);

    /* M28C: prove the same standard-handle abstraction redirects HFILE 1
     * to a filesystem object on the direct OS/2 backend. */
    stdout_token.value = CMDO2_HANDLE_ALLOCATE;
    stdout_token.os2_value = CMDO2_HANDLE_ALLOCATE;
    rc = CmdO2StdSave(1UL, &stdout_token);
    if (rc != 0)
        return 36;
    rc = CmdO2StdRedirectPath(1UL, "m28c-redirect.tmp", CMDO2_REDIR_OUTPUT);
    if (rc != 0)
        return 37;
    actual = 0;
    rc = CmdO2Write((CmdO2Handle)1UL, pipe_msg,
                    (unsigned long)smoke_strlen(pipe_msg), &actual);
    (void)CmdO2StdRestore(1UL, &stdout_token);
    if (rc != 0 || actual != (unsigned long)smoke_strlen(pipe_msg))
        return 38;

    stdout_token.value = CMDO2_HANDLE_ALLOCATE;
    stdout_token.os2_value = CMDO2_HANDLE_ALLOCATE;
    rc = CmdO2StdSave(1UL, &stdout_token);
    if (rc != 0)
        return 39;
    rc = CmdO2StdRedirectPath(1UL, "m28c-redirect.tmp", CMDO2_REDIR_APPEND);
    if (rc != 0)
        return 40;
    actual = 0;
    rc = CmdO2Write((CmdO2Handle)1UL, append_msg,
                    (unsigned long)smoke_strlen(append_msg), &actual);
    (void)CmdO2StdRestore(1UL, &stdout_token);
    if (rc != 0 || actual != (unsigned long)smoke_strlen(append_msg))
        return 41;

    rc = CmdO2OpenRead("m28c-redirect.tmp", &h);
    if (rc != 0)
        return 42;
    actual = 0;
    rc = CmdO2Read(h, pipe_buffer,
                   (unsigned long)smoke_strlen(redir_expected), &actual);
    CmdO2Close(h);
    (void)CmdO2Delete("m28c-redirect.tmp");
    if (rc != 0 || actual != (unsigned long)smoke_strlen(redir_expected))
        return 43;
    pipe_buffer[actual] = '\0';
    if (!smoke_same(pipe_buffer, redir_expected))
        return 44;
    printf("stdout redirection/append through DosDupHandle=OK\n");

    rc = CmdO2OpenWriteReplace("m28c-smoke.tmp", &h);
    if (rc != 0) {
        printf("DosOpen(write) rc=%lu\n", rc);
        return 3;
    }
    actual = 0;
    rc = CmdO2Write(h, msg, (unsigned long)smoke_strlen(msg), &actual);
    CmdO2Close(h);
    if (rc != 0) {
        printf("DosWrite rc=%lu\n", rc);
        return 4;
    }
    printf("wrote %lu bytes\n", actual);

    CmdO2Delete("m28c-smoke2.tmp");
    rc = CmdO2Move("m28c-smoke.tmp", "m28c-smoke2.tmp");
    if (rc != 0) {
        printf("DosMove rc=%lu\n", rc);
        return 5;
    }
    rc = CmdO2Delete("m28c-smoke2.tmp");
    if (rc != 0) {
        printf("DosDelete rc=%lu\n", rc);
        return 6;
    }

    printf("M28D_OS2_BACKEND_OK\n");
    return 0;
}
