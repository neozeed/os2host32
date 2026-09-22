/* Post-M31G CMD32 PS/KILL fixture.
 *
 * START this as a related SESMGR child.  It immediately updates its session
 * title through SESMGR.5, then stays alive long enough for PS/KILL testing.
 */
#define INCL_DOSPROCESS
#include <os2.h>
#include <stdio.h>

extern APIRET APIENTRY DOSSMSETTITLE(ULONG sessionId, PSZ title);

int main(void)
{
    APIRET rc;

    rc = DOSSMSETTITLE(0UL, (PSZ)"title updated by child");
    printf("PSKILL child: DosSmSetTitle rc=%lu\n", (unsigned long)rc);
    printf("PSKILL child: sleeping for 60 seconds; use PS then KILL task-id\n");
    fflush(stdout);
    DosSleep(60000UL);
    return 0;
}
