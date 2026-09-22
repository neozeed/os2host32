/*
 * helpmgr.c - minimal native HELPMGR compatibility personality, extended through M31G R13.
 *
 * The real OS/2 2.0 GA HELPMGR.DLL is a mixed 16/32-bit system module with
 * selector/far-pointer fixups.  V1 keeps execution native Win32, so expose
 * only the documented flat API surface actually required by guest programs.
 *
 * R13 completes the small public lifetime used by applications: create,
 * associate, and destroy an opaque help instance.  V1 deliberately does not
 * emulate the full IBM IPF help engine; it preserves the API/lifetime boundary.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

#ifndef __cdecl
#define __cdecl
#endif

typedef DWORD O2HWND;
typedef DWORD O2HAB;
typedef DWORD O2ULONG;
typedef DWORD O2HMODULE;

typedef struct O2HELPINIT {
    O2ULONG cb;
    O2ULONG ulReturnCode;
    char *pszTutorialName;
    void *phtHelpTable;
    O2HMODULE hmodHelpTableModule;
    O2HMODULE hmodAccelActionBarModule;
    O2ULONG idAccelTable;
    O2ULONG idActionBar;
    char *pszHelpWindowTitle;
    O2ULONG fShowPanelId;
    char *pszHelpLibraryName;
} O2HELPINIT;

#define PMCOMPAT_MAX_HELP_INSTANCES 32
#define PMCOMPAT_HELP_FIRST_HANDLE  0x70000000UL
struct PMCompatHelpInstance {
    O2HWND handle;
    O2HWND associated;
};
static struct PMCompatHelpInstance g_help_instances[PMCOMPAT_MAX_HELP_INSTANCES];
static O2HWND g_next_help_instance = PMCOMPAT_HELP_FIRST_HANDLE;

static int helpmgr_trace_enabled(void)
{
    char value[8];
    DWORD n;
    n = GetEnvironmentVariableA("OS2_PM_TRACE", value, sizeof(value));
    return n != 0 && n < sizeof(value) && value[0] != '0';
}

static struct PMCompatHelpInstance *helpmgr_find_instance(O2HWND hwndHelpInstance)
{
    unsigned i;
    if (hwndHelpInstance == 0)
        return NULL;
    for (i = 0; i < PMCOMPAT_MAX_HELP_INSTANCES; ++i)
        if (g_help_instances[i].handle == hwndHelpInstance)
            return &g_help_instances[i];
    return NULL;
}

static BOOL helpmgr_remove_instance(O2HWND hwndHelpInstance)
{
    struct PMCompatHelpInstance *slot;
    slot = helpmgr_find_instance(hwndHelpInstance);
    if (!slot)
        return FALSE;
    slot->handle = 0;
    slot->associated = 0;
    return TRUE;
}

/* 51 - HWND WinCreateHelpInstance(HAB hab, PHELPINIT phinit)
 *
 * V1 does not render IBM IPF help.  It does preserve the native PM lifetime
 * contract: creation yields a nonzero opaque help-instance HWND, the caller's
 * ulReturnCode is cleared on success, and that token can subsequently be
 * associated and destroyed.
 */
O2HWND __cdecl WinCreateHelpInstance(O2HAB hab, O2HELPINIT *phinit)
{
    unsigned i;
    O2HWND handle;
    (void)hab;
    if (!phinit)
        return 0;
    for (i = 0; i < PMCOMPAT_MAX_HELP_INSTANCES; ++i) {
        if (g_help_instances[i].handle == 0) {
            handle = g_next_help_instance++;
            if (handle == 0)
                handle = g_next_help_instance++;
            g_help_instances[i].handle = handle;
            g_help_instances[i].associated = 0;
            phinit->ulReturnCode = 0;
            if (helpmgr_trace_enabled()) {
                fprintf(stderr,
                        "HELPMGR: WinCreateHelpInstance hab=%08lX init=%08lX -> %08lX\n",
                        (unsigned long)hab, (unsigned long)(DWORD)phinit,
                        (unsigned long)handle);
                fflush(stderr);
            }
            return handle;
        }
    }
    phinit->ulReturnCode = 1;
    return 0;
}

/* 54 - BOOL WinAssociateHelpInstance(HWND hwndHelpInstance, HWND hwndApp) */
BOOL __cdecl WinAssociateHelpInstance(O2HWND hwndHelpInstance, O2HWND hwndApp)
{
    struct PMCompatHelpInstance *slot;
    slot = helpmgr_find_instance(hwndHelpInstance);
    if (!slot)
        return FALSE;
    slot->associated = hwndApp;
    if (helpmgr_trace_enabled()) {
        fprintf(stderr,
                "HELPMGR: WinAssociateHelpInstance help=%08lX app=%08lX -> 1\n",
                (unsigned long)hwndHelpInstance, (unsigned long)hwndApp);
        fflush(stderr);
    }
    return TRUE;
}

/* 52 - BOOL WinDestroyHelpInstance(HWND hwndHelpInstance)
 *
 * OS/2 requires a handle previously returned by WinCreateHelpInstance.
 * Unknown/system handles are therefore rejected.
 */
BOOL __cdecl WinDestroyHelpInstance(O2HWND hwndHelpInstance)
{
    BOOL ok;
    ok = helpmgr_remove_instance(hwndHelpInstance);
    if (helpmgr_trace_enabled()) {
        fprintf(stderr, "HELPMGR: WinDestroyHelpInstance hwnd=%08lX -> %u\n",
                (unsigned long)hwndHelpInstance, (unsigned)ok);
        fflush(stderr);
    }
    return ok;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)instance; (void)reason; (void)reserved;
    return TRUE;
}
