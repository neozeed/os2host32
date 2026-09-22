/*
 * cmdos2_os2_wait_test.c - M29K explicit child lifecycle regression.
 *
 * Starts a real C/386 child with EXEC_ASYNCRESULT, verifies NOWAIT reports
 * ERROR_CHILD_NOT_COMPLETE, then waits and checks the child's result code.
 */
#include <stdio.h>
#include <string.h>
#include "cmdos2.h"

int main(void)
{
    static char args[] = "m29k-wait-child.exe\0";
    char fail[260];
    struct CmdO2ResultCodes results;
    unsigned long pid;
    unsigned long waited;
    CmdO2Rc rc;

    if (!CmdO2Init()) {
        printf("M29K_WAIT_INIT_FAIL %s\n", CmdO2InitError());
        return 1;
    }

    fail[0] = '\0';
    results.codeTerminate = 0UL;
    results.codeResult = 0UL;
    rc = CmdO2ExecPgm(fail, (long)sizeof(fail),
                      CMDO2_EXEC_ASYNCRESULT,
                      args, (const char *)0, &results,
                      "m29k-wait-child.exe");
    if (rc != 0UL || results.codeTerminate == 0UL) {
        printf("M29K_WAIT_EXEC_FAIL rc=%lu pid=%lu object=[%s]\n",
               rc, results.codeTerminate, fail);
        CmdO2Done();
        return 2;
    }
    pid = results.codeTerminate;
    printf("async pid=%lu\n", pid);

    results.codeTerminate = 0UL;
    results.codeResult = 0UL;
    waited = 0UL;
    rc = CmdO2WaitChild(CMDO2_WAIT_PROCESS, CMDO2_NOWAIT,
                        &results, &waited, pid);
    if (rc != CMDO2_ERROR_CHILD_NOT_COMPLETE) {
        printf("M29K_WAIT_NOWAIT_FAIL rc=%lu waited=%lu result=%lu\n",
               rc, waited, results.codeResult);
        CmdO2Done();
        return 3;
    }
    puts("nowait: child still running");

    results.codeTerminate = 0UL;
    results.codeResult = 0UL;
    waited = 0UL;
    rc = CmdO2WaitChild(CMDO2_WAIT_PROCESS, CMDO2_WAIT,
                        &results, &waited, pid);
    if (rc != 0UL || waited != pid || results.codeResult != 23UL) {
        printf("M29K_WAIT_FINAL_FAIL rc=%lu waited=%lu result=%lu\n",
               rc, waited, results.codeResult);
        CmdO2Done();
        return 4;
    }

    printf("waited pid=%lu result=%lu\n", waited, results.codeResult);
    puts("M29K_CHILD_WAIT_OK");
    CmdO2Done();
    return 0;
}
