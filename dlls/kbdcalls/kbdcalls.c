/*
 * KBDCALLS.dll - thin exported OS/2 keyboard ABI veneer.
 *
 * KBDCALLS R2 keeps the existing six names/ordinals/signatures here while
 * moving OS/2 keyboard status and line-input semantics into common/kbd and
 * Win32 console/input mechanics into common/win32/os2_kbd_win32.c.
 */

#include "os2_kbd.h"
#include "os2_kbd_win32.h"

#ifndef __cdecl
#define __cdecl
#endif

static struct Os2KbdSession *kbd_session(void)
{
    return os2_kbd_win32_session();
}

unsigned short __cdecl KbdCharIn(struct Os2KbdKeyInfo *info,
                                  unsigned short wait,
                                  unsigned short hkbd)
{
    return os2_kbd_KbdCharIn(kbd_session(), info, wait, hkbd);
}

unsigned short __cdecl KbdStringIn(char *buffer,
                                    struct Os2KbdStringInBuf *length,
                                    unsigned short wait,
                                    unsigned short hkbd)
{
    return os2_kbd_KbdStringIn(kbd_session(), buffer, length, wait, hkbd);
}

unsigned short __cdecl KbdGetStatus(struct Os2KbdInfo *info,
                                     unsigned short hkbd)
{
    return os2_kbd_KbdGetStatus(kbd_session(), info, hkbd);
}

unsigned short __cdecl KbdSetStatus(const struct Os2KbdInfo *info,
                                     unsigned short hkbd)
{
    return os2_kbd_KbdSetStatus(kbd_session(), info, hkbd);
}

unsigned short __cdecl KbdFlushBuffer(unsigned short hkbd)
{
    return os2_kbd_KbdFlushBuffer(kbd_session(), hkbd);
}

unsigned short __cdecl KbdPeek(struct Os2KbdKeyInfo *info,
                                unsigned short hkbd)
{
    return os2_kbd_KbdPeek(kbd_session(), info, hkbd);
}
