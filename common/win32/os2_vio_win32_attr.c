#include "os2_vio_win32.h"

unsigned short os2_vio_win32_attribute(unsigned char os2_attribute)
{
    /* IBM text attributes and Win32 console attributes share the low seven
     * bits: foreground BGR + intensity and background BGR.  OS/2 bit 7 is
     * blink in the traditional mode, while Win32 uses it as background
     * intensity, so do not silently convert blink into a bright background. */
    return (unsigned short)(os2_attribute & 0x7fU);
}
