/* M29N2b.2 leaf DLL: visible INIT/TERM plus an exported state probe. */
typedef unsigned long ULONG;
extern ULONG DosWrite(ULONG, const void *, ULONG, ULONG *);

static ULONG initialized;

static void say(const char *s)
{
    ULONG n, wrote;
    n = 0;
    while (s[n]) ++n;
    wrote = 0;
    DosWrite(1UL, s, n, &wrote);
}

ULONG M29N2DValue(void)
{
    return initialized ? 0x29B20042UL : 0xDEAD0002UL;
}

ULONG M29N2DInitTerm(ULONG hmod, ULONG flag)
{
    (void)hmod;
    if (flag == 0UL) {
        initialized = 1UL;
        say("M29N2B2_INIT_D\r\n");
        return 1UL;
    }
    if (flag == 1UL) {
        say("M29N2B2_TERM_D\r\n");
        initialized = 0UL;
        return 1UL;
    }
    return 0UL;
}
