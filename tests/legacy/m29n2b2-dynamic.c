/* M29N2b.2 dynamic DLL lifecycle regression. */
#include <stdio.h>

typedef unsigned long APIRET;
typedef unsigned long HMODULE;
typedef unsigned long (*VALUEFN)(void);

extern APIRET DosLoadModule(char *, unsigned long, const char *, HMODULE *);
extern APIRET DosQueryModuleHandle(const char *, HMODULE *);
extern APIRET DosQueryProcAddr(HMODULE, unsigned long, const char *, void **);
extern APIRET DosFreeModule(HMODULE);

static int bad(const char *what, APIRET rc)
{
    printf("M29N2B2_BAD %s rc=%lu\n", what, rc);
    return 1;
}

int main(void)
{
    char failname[260];
    HMODULE c1, c2, q;
    APIRET rc;
    void *pv;
    VALUEFN fn;
    unsigned long v;

    c1 = c2 = q = 0;
    failname[0] = 0;

    rc = DosLoadModule(failname, sizeof(failname), "M29N2C", &c1);
    if (rc != 0)
        return bad(failname[0] ? failname : "load C #1", rc);
    printf("LIFECYCLE_C1_HANDLE=%08lX\n", c1);

    /* A repeated load must acquire the existing instance without INIT again. */
    rc = DosLoadModule(failname, sizeof(failname), "M29N2C", &c2);
    if (rc != 0 || c2 != c1)
        return bad("load C #2", rc);
    printf("LIFECYCLE_C2_HANDLE=%08lX\n", c2);

    pv = 0;
    rc = DosQueryProcAddr(c1, 0, "M29N2CValue", &pv);
    if (rc != 0 || !pv)
        return bad("query C value", rc);
    fn = (VALUEFN)pv;
    v = fn();
    printf("LIFECYCLE_C_VALUE=%08lX\n", v);
    if (v != 0x29B20043UL)
        return bad("C did not observe initialized D", v);

    rc = DosFreeModule(c1);
    if (rc != 0)
        return bad("free C #1", rc);
    rc = DosQueryModuleHandle("M29N2C", &q);
    printf("AFTER_LIFECYCLE_FREE1_RC=%lu HANDLE=%08lX\n", rc, q);
    if (rc != 0 || q != c2)
        return bad("C terminated before final reference", rc);

    rc = DosFreeModule(c2);
    if (rc != 0)
        return bad("free C #2", rc);
    rc = DosQueryModuleHandle("M29N2C", &q);
    printf("AFTER_LIFECYCLE_FREE2_C_RC=%lu\n", rc);
    if (rc == 0)
        return bad("C remains after final free", rc);
    rc = DosQueryModuleHandle("M29N2D", &q);
    printf("AFTER_LIFECYCLE_FREE2_D_RC=%lu\n", rc);
    if (rc == 0)
        return bad("D remains after parent final free", rc);

    /* Initialization failure must be reported, and the failed module must not
     * remain visible in the module table. */
    q = 0;
    failname[0] = 0;
    rc = DosLoadModule(failname, sizeof(failname), "M29N2F", &q);
    printf("FAILED_INIT_RC=%lu\n", rc);
    if (rc != 295UL)
        return bad("expected ERROR_INIT_ROUTINE_FAILED", rc);
    rc = DosQueryModuleHandle("M29N2F", &q);
    printf("FAILED_INIT_QUERY_RC=%lu\n", rc);
    if (rc == 0)
        return bad("failed-init module remained registered", rc);

    puts("M29N2B2_DYNAMIC_LIFECYCLE_OK");
    return 0;
}
