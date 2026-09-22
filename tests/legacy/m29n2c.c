/* M29N2b.2 parent DLL.  Its TERM routine deliberately calls the child DLL. */
typedef unsigned long ULONG;
extern ULONG DosWrite(ULONG, const void *, ULONG, ULONG *);
extern ULONG M29N2DValue(void);

static ULONG initialized;
static ULONG init_saw_child;

static void say(const char *s)
{
    ULONG n, wrote;
    n = 0;
    while (s[n]) ++n;
    wrote = 0;
    DosWrite(1UL, s, n, &wrote);
}

ULONG M29N2CValue(void)
{
    if (!initialized || !init_saw_child)
        return 0xDEAD0003UL;
    return M29N2DValue() + 1UL;
}

ULONG M29N2CInitTerm(ULONG hmod, ULONG flag)
{
    ULONG child;
    (void)hmod;
    if (flag == 0UL) {
        child = M29N2DValue();
        if (child == 0x29B20042UL) {
            init_saw_child = 1UL;
            say("M29N2B2_INIT_C_AFTER_D_OK\r\n");
        } else {
            say("M29N2B2_INIT_C_AFTER_D_FAIL\r\n");
            return 0UL;
        }
        initialized = 1UL;
        return 1UL;
    }
    if (flag == 1UL) {
        child = M29N2DValue();
        if (child == 0x29B20042UL)
            say("M29N2B2_TERM_C_SEES_D_OK\r\n");
        else
            say("M29N2B2_TERM_C_SEES_D_FAIL\r\n");
        initialized = 0UL;
        return 1UL;
    }
    return 0UL;
}
