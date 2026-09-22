#define INCL_DOSPROCESS
#define INCL_DOSFILEMGR
#include <os2.h>
#include <stdio.h>
#include <string.h>

static unsigned char child[] = "execchild.exe";
static unsigned char args_check[] = "execchild.exe\0check \"two words\"\0";
static unsigned char args_slow[] = "execchild.exe\0slow\0";
static unsigned char args_capture[] = "execchild.exe\0capture\0";
static unsigned char env_check[] = "WHP_EXEC_TEST=child\0";
static unsigned char capture[] = "process1-output.tmp";
#define CHECK(x) do { if (!(x)) { printf("process1 FAIL line %d\n", __LINE__); return 1; } } while (0)
int main(void)
{
    RESULTCODES result;
    PID pid, got;
    ULONG rc, action, count;
    HFILE out, saved, target;
    char object[260], text[256];
    CHECK(DosExecPgm(object, sizeof(object), EXEC_SYNC, args_check,
        env_check, &result, child) == 0);
    CHECK(result.codeTerminate == 0 && result.codeResult == 37);
    CHECK(DosExecPgm(object, sizeof(object), EXEC_ASYNCRESULT, args_slow,
        (PSZ)0, &result, child) == 0);
    pid = result.codeTerminate;
    CHECK(pid != 0);
    rc = DosWaitChild(DCWA_PROCESS, DCWW_NOWAIT, &result, &got, pid);
    if (rc == 129) {
        CHECK(DosWaitChild(DCWA_PROCESS, DCWW_WAIT, &result, &got, pid) == 0);
    } else CHECK(rc == 0);
    CHECK(got == pid && result.codeTerminate == 0 && result.codeResult == 23);
    CHECK(DosWaitChild(DCWA_PROCESS, DCWW_NOWAIT, &result, &got, pid) == 128);
    /* Exclusive creation avoids overwriting an existing file. */
    CHECK(DosOpen(capture, &out, &action, 0, 0, 0x10, 0x42, (PEAOP)0) == 0);
    saved = (HFILE)-1;
    rc = DosDupHandle(1, &saved);
    if (rc) { DosClose(out); DosDelete(capture); CHECK(rc == 0); }
    fflush(stdout);
    target = 1;
    rc = DosDupHandle(out, &target);
    if (!rc) rc = DosExecPgm(object, sizeof(object), EXEC_SYNC,
        args_capture, (PSZ)0, &result, child);
    /* Restore stdout before printing any assertion failures. */
    target = 1;
    action = DosDupHandle(saved, &target);
    DosClose(saved);
    DosSetFilePtr(out, 0, 0, &count);
    memset(text, 0, sizeof(text));
    count = 0;
    DosRead(out, text, sizeof(text)-1, &count);
    DosClose(out);
    DosDelete(capture);
    CHECK(action == 0 && rc == 0 && result.codeResult == 0);
    CHECK(strstr(text, "CHILD_STDOUT_OK") != 0);
    CHECK(strstr(text, "v2:") == 0);
    puts("process1 PASS (sync, arguments/environment, async wait, reap, stdout)");
    return 0;
}
