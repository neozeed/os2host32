/*
 * M29K4 direct C/386 EXEC_BACKGROUND regression.
 *
 * The background child must be created successfully, must not be waitable
 * through DosWaitChild, and must continue long enough to leave its marker.
 */
#define INCL_DOSPROCESS
#include <os2.h>
#include <stdio.h>
#include <string.h>

#include "cmdos2.h"

int main(void)
{
    char argblock[256];
    char objectName[260];
    struct CmdO2ResultCodes results;
    struct CmdO2ResultCodes waitedResults;
    unsigned long waited;
    unsigned long pid;
    CmdO2Rc rc;
    FILE *f;
    char line[128];
    const char *program;
    const char *marker;
    size_t pn;
    size_t mn;
    int result;

    if (!CmdO2Init()) {
        printf("M29K4_DETACH_INIT_FAIL %s\n", CmdO2InitError());
        return 1;
    }

    result = 0;
    program = "m29k4-detach-child.exe";
    marker = "m29k4-detach-api.ok";
    remove(marker);

    pn = strlen(program);
    mn = strlen(marker);
    if (pn + mn + 3U > sizeof(argblock)) {
        puts("M29K4_DETACH_API_BAD_ARGBLOCK");
        result = 2;
        goto done;
    }
    memcpy(argblock, program, pn + 1U);
    memcpy(argblock + pn + 1U, marker, mn + 1U);
    argblock[pn + 1U + mn + 1U] = '\0';

    objectName[0] = '\0';
    results.codeTerminate = 0UL;
    results.codeResult = 0UL;
    rc = CmdO2ExecPgm(objectName, (long)sizeof(objectName),
                      CMDO2_EXEC_BACKGROUND, argblock, NULL,
                      &results, program);
    if (rc != 0UL || results.codeTerminate == 0UL) {
        printf("background launch failed rc=%lu pid=%lu object=[%s]\n",
               rc, results.codeTerminate, objectName);
        result = 3;
        goto done;
    }
    pid = results.codeTerminate;
    printf("background pid=%lu\n", pid);

    waitedResults.codeTerminate = 0UL;
    waitedResults.codeResult = 0UL;
    waited = 0UL;
    rc = CmdO2WaitChild(CMDO2_WAIT_PROCESS, CMDO2_NOWAIT,
                        &waitedResults, &waited, pid);
    if (rc != CMDO2_ERROR_WAIT_NO_CHILDREN) {
        printf("background unexpectedly waitable rc=%lu waited=%lu\n",
               rc, waited);
        result = 4;
        goto done;
    }
    puts("background is not in DosWaitChild result set");

    DosSleep(1200UL);
    f = fopen(marker, "rb");
    if (f == NULL) {
        puts("background marker missing");
        result = 5;
        goto done;
    }
    line[0] = '\0';
    if (fgets(line, sizeof(line), f) == NULL) {
        fclose(f);
        puts("background marker unreadable");
        result = 6;
        goto done;
    }
    fclose(f);
    remove(marker);
    if (strncmp(line, "M29K4_DETACH_MARKER_OK", 22U) != 0) {
        printf("unexpected marker [%s]\n", line);
        result = 7;
        goto done;
    }

    puts("M29K4_DETACH_API_OK");

done:
    CmdO2Done();
    return result;
}
