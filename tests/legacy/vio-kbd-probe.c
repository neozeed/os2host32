/*
 * vio-kbd-probe.c - C/386 archaeology probe for the SDK/DDK VIO/KBD ABI.
 *
 * This is intentionally not part of the normal M29A build.  Build it with a
 * complete historical header/import-library set, then inspect the resulting
 * LE/LX with os2host32 --scan.  We want to learn exactly which import/fixup
 * form C/386 emits for these APIs before teaching the direct loader to thunk
 * it.
 */
#define INCL_KBD
#define INCL_VIO
#include <os2.h>

int main(void)
{
    static char before[] = "C/386 VIO/KBD probe - press a key: ";
    static char after[] = "\r\nVIO/KBD probe returned.\r\n";
    KBDKEYINFO key;
    USHORT rc;

    rc = VioWrtTTY(before, (USHORT)(sizeof(before) - 1), (HVIO)0);
    if (rc != 0)
        return 10;
    rc = KbdCharIn(&key, IO_WAIT, (HKBD)0);
    if (rc != 0)
        return 11;
    rc = VioWrtTTY(after, (USHORT)(sizeof(after) - 1), (HVIO)0);
    if (rc != 0)
        return 12;
    return 0;
}
