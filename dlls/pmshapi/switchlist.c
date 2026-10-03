/* Process-local PM switch records for native Win32 windows. Windows owns
 * the actual task switcher; these APIs retain the separate OS/2 title/flags.
 * A frame's first lookup supplies the entry PM normally creates for it. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include "switchlist.h"

#define MAX_SWITCHES 128
#define BAD_SWITCH 0x1202UL
#define BAD_PROCESS 0x1204UL
#define BAD_WINDOW 0x1206UL
#define BAD_PARAMETERS 0x1208UL
#define NO_SPACE 0x1201UL
#define REMOVED_SWITCH 0xffffffffUL
static const char switch_property[] = "OS2HOST32.PMSHAPI.Switch";
static CRITICAL_SECTION switch_lock;
static DWORD next_switch = 0x53000000UL;
struct SwitchEntry { DWORD handle; O2SwitchControl data; };
static struct SwitchEntry entries[MAX_SWITCHES];

void pmsh_switch_init(void) { InitializeCriticalSection(&switch_lock); }
void pmsh_switch_term(void) { DeleteCriticalSection(&switch_lock); }

static HWND own_frame(DWORD value)
{
    HWND hwnd = (HWND)(ULONG_PTR)value; DWORD pid = 0;
    if (!hwnd || !IsWindow(hwnd)) return NULL;
    GetWindowThreadProcessId(hwnd,&pid);
    if (pid != GetCurrentProcessId()) return NULL;
    return GetAncestor(hwnd,GA_ROOT);
}
/* Called under switch_lock. A window property distinguishes a destroyed
 * HWND from a later window that Windows gives the same numeric handle. */
static void reap(void)
{
    unsigned i;
    for (i = 0; i < MAX_SWITCHES; ++i) {
        if (entries[i].handle && (DWORD)(ULONG_PTR)GetPropA(
            (HWND)(ULONG_PTR)entries[i].data.hwnd,switch_property) != entries[i].handle)
            memset(&entries[i],0,sizeof(entries[i]));
    }
}
static struct SwitchEntry *find_switch(DWORD handle)
{
    unsigned i;
    reap();
    for (i = 0; i < MAX_SWITCHES; ++i)
        if (handle && entries[i].handle == handle) return &entries[i];
    return NULL;
}
static DWORD prepare(const O2SwitchControl *source, O2SwitchControl *data)
{
    HWND hwnd;
    if (!source) return BAD_PARAMETERS;
    *data = *source;
    if (data->idProcess && data->idProcess != GetCurrentProcessId()) return BAD_PROCESS;
    hwnd = own_frame(data->hwnd); if (!hwnd) return BAD_WINDOW;
    data->hwnd = (DWORD)(ULONG_PTR)hwnd;
    data->idProcess = GetCurrentProcessId();
    if (!data->idSession) ProcessIdToSessionId(data->idProcess,&data->idSession);
    /* IBM permits up to 60 title characters; the last three bytes are ABI
     * padding, not additional title capacity. Never read past the struct. */
    memset(data->szSwtitle+60,0,4);
    return 0;
}
static DWORD add_locked(const O2SwitchControl *data)
{
    struct SwitchEntry *entry; unsigned i; DWORD handle;
    HWND hwnd = (HWND)(ULONG_PTR)data->hwnd;
    entry = find_switch((DWORD)(ULONG_PTR)GetPropA(hwnd,switch_property));
    if (entry) { entry->data = *data; return entry->handle; }
    for (i = 0; i < MAX_SWITCHES; ++i) if (!entries[i].handle) break;
    if (i == MAX_SWITCHES) return 0;
    do { handle = ++next_switch; } while (!handle || handle == REMOVED_SWITCH || find_switch(handle));
    if (!SetPropA(hwnd,switch_property,(HANDLE)(ULONG_PTR)handle)) return 0;
    entries[i].handle = handle; entries[i].data = *data;
    return handle;
}
DWORD __cdecl WinAddSwitchEntry(const O2SwitchControl *source)
{
    O2SwitchControl data; DWORD handle;
    if (prepare(source,&data)) return 0;
    if (!data.szSwtitle[0]) GetWindowTextA((HWND)(ULONG_PTR)data.hwnd,data.szSwtitle,61);
    EnterCriticalSection(&switch_lock); handle = add_locked(&data);
    LeaveCriticalSection(&switch_lock); return handle;
}
DWORD __cdecl WinChangeSwitchEntry(DWORD handle, const O2SwitchControl *source)
{
    O2SwitchControl data; struct SwitchEntry *entry; DWORD rc;
    HWND hwnd, old;
    rc = prepare(source,&data); if (rc) return rc;
    EnterCriticalSection(&switch_lock); entry = find_switch(handle);
    if (!entry) rc = BAD_SWITCH;
    else {
        hwnd = (HWND)(ULONG_PTR)data.hwnd; old = (HWND)(ULONG_PTR)entry->data.hwnd;
        if (hwnd != old && find_switch((DWORD)(ULONG_PTR)GetPropA(hwnd,switch_property)))
            rc = BAD_WINDOW;
        else if (!SetPropA(hwnd,switch_property,(HANDLE)(ULONG_PTR)handle)) rc = NO_SPACE;
        else {
            if (hwnd != old) SetPropA(old,switch_property,(HANDLE)(ULONG_PTR)REMOVED_SWITCH);
            entry->data = data;
        }
    }
    LeaveCriticalSection(&switch_lock); return rc;
}
DWORD __cdecl WinQuerySwitchEntry(DWORD handle, O2SwitchControl *data)
{
    struct SwitchEntry *entry; DWORD rc = BAD_SWITCH;
    if (!data) return BAD_PARAMETERS;
    EnterCriticalSection(&switch_lock); entry = find_switch(handle);
    if (entry) { *data = entry->data; rc = 0; }
    LeaveCriticalSection(&switch_lock); return rc;
}
static BOOL CALLBACK first_frame(HWND hwnd, LPARAM context)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd,&pid);
    if (pid == GetCurrentProcessId() && GetPropA(hwnd,switch_property) != (HANDLE)(ULONG_PTR)REMOVED_SWITCH) {
        *(HWND *)context = hwnd; return FALSE;
    }
    return TRUE;
}
DWORD __cdecl WinQuerySwitchHandle(DWORD window, DWORD pid)
{
    HWND hwnd = NULL; DWORD handle = 0; unsigned i; O2SwitchControl data;
    if (pid && pid != GetCurrentProcessId()) return 0;
    if (window) hwnd = own_frame(window);
    else {
        EnterCriticalSection(&switch_lock); reap();
        for (i = 0; i < MAX_SWITCHES; ++i) if (entries[i].handle) { handle = entries[i].handle; break; }
        LeaveCriticalSection(&switch_lock);
        if (handle) return handle;
        EnumWindows(first_frame,(LPARAM)&hwnd);
    }
    if (!hwnd) return 0;
    memset(&data,0,sizeof(data)); data.hwnd = (DWORD)(ULONG_PTR)hwnd;
    data.idProcess = GetCurrentProcessId(); ProcessIdToSessionId(data.idProcess,&data.idSession);
    data.uchVisibility = 4; data.fbJump = 2; data.bProgType = 3;
    GetWindowTextA(hwnd,data.szSwtitle,61);
    EnterCriticalSection(&switch_lock);
    handle = (DWORD)(ULONG_PTR)GetPropA(hwnd,switch_property);
    if (handle == REMOVED_SWITCH) handle = 0;
    else if (!find_switch(handle)) handle = add_locked(&data);
    LeaveCriticalSection(&switch_lock); return handle;
}
DWORD __cdecl WinRemoveSwitchEntry(DWORD handle)
{
    struct SwitchEntry *entry; DWORD rc = BAD_SWITCH;
    EnterCriticalSection(&switch_lock); entry = find_switch(handle);
    if (entry) {
        /* Keep a tombstone on this window, so a query cannot undo removal.
         * Windows drops it automatically when the window is destroyed. */
        if (!SetPropA((HWND)(ULONG_PTR)entry->data.hwnd,switch_property,(HANDLE)(ULONG_PTR)REMOVED_SWITCH)) rc = NO_SPACE;
        else { memset(entry,0,sizeof(*entry)); rc = 0; }
    }
    LeaveCriticalSection(&switch_lock); return rc;
}
