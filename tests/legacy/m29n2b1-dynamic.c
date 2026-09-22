/*
 * M29N2b.1 dynamic module-manager regression.
 *
 * Unlike M29N2a this executable has NO static import from M29N2A.DLL.
 * It exercises the 32-bit OS/2 module APIs at DOSCALLS ordinals
 * 318/319/321/322 and calls exports returned at runtime.
 */
#include <stdio.h>
#include <string.h>

typedef unsigned long APIRET;
typedef unsigned long HMODULE;
typedef unsigned long (*VALUEFN)(void);

extern APIRET DosLoadModule(char *, unsigned long, const char *, HMODULE *);
extern APIRET DosQueryModuleHandle(const char *, HMODULE *);
extern APIRET DosQueryProcAddr(HMODULE, unsigned long, const char *, void **);
extern APIRET DosFreeModule(HMODULE);

static int fail(const char *what, APIRET rc)
{
    printf("M29N2B1_BAD %s rc=%lu\n", what, rc);
    return 1;
}

int main(void)
{
    char bad[260];
    HMODULE a1, a2, qa, qb;
    VALUEFN fn;
    void *pv;
    unsigned long v;
    APIRET rc;

    a1 = a2 = qa = qb = 0;
    bad[0] = 0;

    rc = DosQueryModuleHandle("M29N2A", &qa);
    printf("PRE_QUERY_A_RC=%lu\n", rc);
    if (rc == 0)
        return fail("pre-query unexpectedly found module", rc);

    rc = DosLoadModule(bad, sizeof(bad), "M29N2A", &a1);
    if (rc != 0)
        return fail(bad[0] ? bad : "DosLoadModule A #1", rc);
    printf("LOAD_A1_HANDLE=%08lX\n", a1);

    rc = DosLoadModule(bad, sizeof(bad), "M29N2A", &a2);
    if (rc != 0)
        return fail("DosLoadModule A #2", rc);
    printf("LOAD_A2_HANDLE=%08lX\n", a2);
    if (a1 != a2)
        return fail("repeat load returned different handle", 0);

    rc = DosQueryModuleHandle("M29N2A.DLL", &qa);
    if (rc != 0 || qa != a1)
        return fail("DosQueryModuleHandle A", rc);
    printf("QUERY_A_HANDLE=%08lX\n", qa);

    rc = DosQueryModuleHandle("M29N2B", &qb);
    if (rc != 0)
        return fail("DosQueryModuleHandle recursive B", rc);
    printf("QUERY_B_HANDLE=%08lX\n", qb);

    pv = 0;
    rc = DosQueryProcAddr(a1, 0, "M29N2AValue", &pv);
    if (rc != 0 || !pv)
        return fail("DosQueryProcAddr A by name", rc);
    fn = (VALUEFN)pv;
    v = fn();
    printf("DYNAMIC_A_VALUE=%08lX\n", v);
    if (v != 0x29A20043UL)
        return fail("dynamic A value", v);

    pv = 0;
    rc = DosQueryProcAddr(qb, 1, 0, &pv);
    if (rc != 0 || !pv)
        return fail("DosQueryProcAddr B by ordinal", rc);
    fn = (VALUEFN)pv;
    v = fn();
    printf("DYNAMIC_B_VALUE=%08lX\n", v);
    if (v != 0x29A20042UL)
        return fail("dynamic B value", v);

    rc = DosFreeModule(a1);
    if (rc != 0)
        return fail("DosFreeModule A #1", rc);
    rc = DosQueryModuleHandle("M29N2A", &qa);
    printf("AFTER_FREE1_QUERY_A_RC=%lu HANDLE=%08lX\n", rc, qa);
    if (rc != 0 || qa != a2)
        return fail("first free dropped live module", rc);

    rc = DosFreeModule(a2);
    if (rc != 0)
        return fail("DosFreeModule A #2", rc);

    qa = qb = 0;
    rc = DosQueryModuleHandle("M29N2A", &qa);
    printf("AFTER_FREE2_QUERY_A_RC=%lu\n", rc);
    if (rc == 0)
        return fail("A remained loaded after final free", rc);
    rc = DosQueryModuleHandle("M29N2B", &qb);
    printf("AFTER_FREE2_QUERY_B_RC=%lu\n", rc);
    if (rc == 0)
        return fail("recursive B remained loaded after A unload", rc);

    puts("M29N2B1_DYNAMIC_MODULE_API_OK");
    return 0;
}
