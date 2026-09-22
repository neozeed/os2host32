/* M29L direct C/386 DosStartSession / SESMGR.17 regression. */
#define INCL_DOSPROCESS
#include <os2.h>
#include <stdio.h>
#include <string.h>

#include "cmdos2.h"

int main(void)
{
    unsigned long sid;
    unsigned long pid;
    unsigned long waited;
    struct CmdO2ResultCodes results;
    CmdO2Rc rc;
    FILE *f;
    char line[96];
    const char *marker;
    int result;

    if (!CmdO2Init()) {
        printf("M29L_START_INIT_FAIL %s\n", CmdO2InitError());
        return 1;
    }

    marker = "m29l-start-api.ok";
    remove(marker);
    sid = 0UL;
    pid = 0UL;
    rc = CmdO2StartSession("M29L session", "m29l-session-child.exe",
                           marker, NULL,
                           CMDO2_SSF_RELATED_CHILD,
                           CMDO2_SSF_FGBG_FORE,
                           CMDO2_SSF_TYPE_DEFAULT,
                           &sid, &pid);
    if (rc != 0UL || sid == 0UL || pid == 0UL) {
        printf("DosStartSession failed rc=%lu sid=%lu pid=%lu\n",
               rc, sid, pid);
        CmdO2Done();
        return 2;
    }
    printf("started session=%lu pid=%lu\n", sid, pid);

    /* A SESMGR session is not an ordinary DosExecPgm child. */
    results.codeTerminate = 0UL;
    results.codeResult = 0UL;
    waited = 0UL;
    rc = CmdO2WaitChild(CMDO2_WAIT_PROCESS, CMDO2_NOWAIT,
                        &results, &waited, pid);
    if (rc != CMDO2_ERROR_WAIT_NO_CHILDREN) {
        printf("session unexpectedly in DosWaitChild set rc=%lu pid=%lu\n",
               rc, waited);
        CmdO2Done();
        return 3;
    }
    puts("session is separate from DosWaitChild process set");

    DosSleep(1200UL);
    result = 0;
    f = fopen(marker, "rb");
    if (f == NULL) {
        puts("M29L_START_MARKER_MISSING");
        result = 4;
    } else {
        line[0] = '\0';
        if (fgets(line, sizeof(line), f) == NULL ||
            strncmp(line, "M29L_START_SESSION_OK", 21U) != 0) {
            puts("M29L_START_MARKER_BAD");
            result = 5;
        }
        fclose(f);
        remove(marker);
    }
    if (result == 0)
        puts("M29L_START_SESSION_API_OK");

    CmdO2Done();
    return result;
}
