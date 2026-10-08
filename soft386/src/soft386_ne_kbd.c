/* NE KBDCALLS keyboard host adapter.
 *
 * Win32 uses the same backend-neutral KBD R2 common semantics and Win32
 * backend as the real KBDCALLS.DLL.  No guest pointer or selector crosses
 * into this host helper.  For non-Windows diagnostic builds, a terminal
 * input queue can be flushed with tcflush; a redirected stream is not
 * consumed or disturbed.  KBD R2 currently ignores hkbd (default device).
 */
#include "soft386_ne_kbd.h"

#ifdef _WIN32
#include "os2_kbd.h"
#include "os2_kbd_win32.h"
uint32_t soft386_ne_kbd_flush(uint16_t hkbd)
{
    return os2_kbd_KbdFlushBuffer(os2_kbd_win32_session(),hkbd);
}
#else
#include <unistd.h>
#include <termios.h>
#include <errno.h>
uint32_t soft386_ne_kbd_flush(uint16_t hkbd)
{
    (void)hkbd;
    if(isatty(STDIN_FILENO) && tcflush(STDIN_FILENO,TCIFLUSH)!=0)
        return errno==EBADF?6u:1u;
    return 0u;
}
#endif
