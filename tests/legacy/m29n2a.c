/*
 * M29N2a middle guest DLL.
 *
 * This DLL imports ordinal 1 from M29N2B.DLL, so loading the test EXE proves
 * that OS2HOST32 can recursively load a second genuine LE/LX guest module.
 */
extern unsigned long M29N2BImport(void);

unsigned long M29N2AValue(void)
{
    return M29N2BImport() + 1UL;
}
