/* M29L1 full STARTDATA / SESMGR.17 regression. */
#define INCL_DOSPROCESS
#include <os2.h>
#include <stdio.h>
#include <string.h>

#include "cmdos2.h"

int main(void)
{
    struct CmdO2StartOptions o;
    unsigned long sid;
    unsigned long pid;
    CmdO2Rc rc;
    FILE *f;
    char line[128];
    char object[128];
    const char *marker;
    int result;

    if (!CmdO2Init()) {
        printf("M29L1_STARTDATA_INIT_FAIL %s\n", CmdO2InitError());
        return 1;
    }

    marker = "m29l1-startdata-api.ok";
    remove(marker);
    memset(&o, 0, sizeof(o));
    memset(object, 0, sizeof(object));
    o.title = "M29L1 STARTDATA regression";
    o.program = "m29l-session-child.exe";
    o.inputs = marker;
    o.environment = NULL;
    o.related = CMDO2_SSF_RELATED_INDEPENDENT;
    o.fgbg = CMDO2_SSF_FGBG_BACK;
    o.traceOpt = CMDO2_SSF_TRACEOPT_NONE;
    o.inheritOpt = CMDO2_SSF_INHERTOPT_PARENT;
    o.sessionType = CMDO2_SSF_TYPE_WINDOWABLEVIO;
    o.pgmControl = CMDO2_SSF_CONTROL_MINIMIZE |
                   CMDO2_SSF_CONTROL_SETPOS;
    o.initXPos = 64UL;
    o.initYPos = 48UL;
    o.initXSize = 640UL;
    o.initYSize = 400UL;
    o.objectBuffer = object;
    o.objectBufferLen = (unsigned long)sizeof(object);

    sid = 0UL;
    pid = 0UL;
    rc = CmdO2StartSessionEx(&o, &sid, &pid);
    if (rc != 0UL || sid == 0UL || pid == 0UL) {
        printf("DosStartSessionEx failed rc=%lu sid=%lu pid=%lu object=[%s]\n",
               rc, sid, pid, object);
        CmdO2Done();
        return 2;
    }
    printf("startdata session=%lu pid=%lu\n", sid, pid);

    DosSleep(1200UL);
    result = 0;
    f = fopen(marker, "rb");
    if (f == NULL) {
        puts("M29L1_STARTDATA_MARKER_MISSING");
        result = 3;
    } else {
        line[0] = '\0';
        if (fgets(line, sizeof(line), f) == NULL ||
            strncmp(line, "M29L_START_SESSION_OK", 21U) != 0) {
            puts("M29L1_STARTDATA_MARKER_BAD");
            result = 4;
        }
        fclose(f);
        remove(marker);
    }
    if (result == 0)
        puts("M29L1_STARTDATA_API_OK");

    CmdO2Done();
    return result;
}
