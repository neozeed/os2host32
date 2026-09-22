/* M29N2b.2 negative fixture: an INIT routine which explicitly fails. */
typedef unsigned long ULONG;
extern ULONG DosWrite(ULONG, const void *, ULONG, ULONG *);

static void say(const char *s)
{
    ULONG n, wrote;
    n = 0;
    while (s[n]) ++n;
    wrote = 0;
    DosWrite(1UL, s, n, &wrote);
}

ULONG M29N2FValue(void)
{
    return 0xBADF00DUL;
}

ULONG M29N2FInitTerm(ULONG hmod, ULONG flag)
{
    (void)hmod;
    if (flag == 0UL) {
        say("M29N2B2_FAIL_INIT_CALLED\r\n");
        return 0UL;
    }
    say("M29N2B2_FAIL_TERM_SHOULD_NOT_RUN\r\n");
    return 1UL;
}
