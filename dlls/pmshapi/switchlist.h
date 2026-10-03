#ifndef OS2HOST_SWITCHLIST_H
#define OS2HOST_SWITCHLIST_H
#include <stddef.h>

/* IBM's flat 32-bit pmshl.h: title at 28, program type at 92, total 96.
 * The 16-bit SWCNTRL (short PID/session and byte flags) is a different ABI. */
typedef struct O2SwitchControl {
    DWORD hwnd, hwndIcon, hprog, idProcess, idSession;
    DWORD uchVisibility, fbJump;
    char szSwtitle[64];
    DWORD bProgType;
} O2SwitchControl;
typedef char O2SwitchSizeCheck[sizeof(O2SwitchControl) == 96 ? 1 : -1];
typedef char O2SwitchTitleCheck[offsetof(O2SwitchControl,szSwtitle) == 28 ? 1 : -1];
typedef char O2SwitchTypeCheck[offsetof(O2SwitchControl,bProgType) == 92 ? 1 : -1];

void pmsh_switch_init(void);
void pmsh_switch_term(void);
DWORD __cdecl WinAddSwitchEntry(const O2SwitchControl *control);
DWORD __cdecl WinChangeSwitchEntry(DWORD handle, const O2SwitchControl *control);
DWORD __cdecl WinQuerySwitchEntry(DWORD handle, O2SwitchControl *control);
DWORD __cdecl WinQuerySwitchHandle(DWORD hwnd, DWORD pid);
DWORD __cdecl WinRemoveSwitchEntry(DWORD handle);
#endif
