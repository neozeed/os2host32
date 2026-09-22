/*
 * M29N2a leaf guest DLL.
 *
 * Keep this deliberately CRT-free: the point of the first user-DLL loader
 * regression is mapping/export/import recursion, not DLL runtime startup.
 */
unsigned long M29N2BValue(void)
{
    return 0x29A20042UL;
}
