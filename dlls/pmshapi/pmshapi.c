/*
 * pmshapi.c - first tiny PMSHAPI personality for Milestone 31A.
 *
 * WinAddSwitchEntry is advisory in real OS/2 PM.  A native Win32 top-level
 * window already participates in the Windows task switcher, so accepting the
 * registration is sufficient for the WMCHAR sample while preserving the API
 * boundary for later shell-list work.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

#ifndef __cdecl
#define __cdecl
#endif

typedef DWORD O2HSWITCH;

#define PMCOMPAT_MAX_PROFILES 32
struct PMCompatProfile {
    DWORD handle;
    char path[MAX_PATH];
};
static struct PMCompatProfile g_profiles[PMCOMPAT_MAX_PROFILES];
static DWORD g_next_profile_handle = 0x10000UL;

static void pmsh_default_profile_path(char *path, DWORD size)
{
    char *p;
    if (!path || size == 0) return;
    path[0] = 0;
    GetModuleFileNameA(NULL, path, size);
    path[size - 1] = 0;
    p = strrchr(path, '\\');
    if (p) strcpy(p + 1, "os2user.ini");
    else strcpy(path, "os2user.ini");
}

static const char *pmsh_profile_path(DWORD hini, char *fallback, DWORD size)
{
    unsigned i;
    for (i = 0; i < PMCOMPAT_MAX_PROFILES; ++i)
        if (g_profiles[i].handle == hini && g_profiles[i].path[0])
            return g_profiles[i].path;
    pmsh_default_profile_path(fallback, size);
    return fallback;
}

/* 102 - open/create a private PM profile.  Win32's profile APIs open files
   per operation, so the compatibility HINI only needs to retain the path. */
DWORD __cdecl PrfOpenProfile(DWORD hab, const char *fileName)
{
    unsigned i;
    DWORD h;
    char value[8];
    DWORD n;
    (void)hab;
    if (!fileName || !*fileName)
        return 0;
    for (i = 0; i < PMCOMPAT_MAX_PROFILES; ++i) {
        if (g_profiles[i].handle == 0) {
            h = g_next_profile_handle++;
            if (h == 0 || h == 0xffffffffUL || h == 0xfffffffeUL)
                h = g_next_profile_handle++;
            g_profiles[i].handle = h;
            strncpy(g_profiles[i].path, fileName, sizeof(g_profiles[i].path)-1);
            g_profiles[i].path[sizeof(g_profiles[i].path)-1] = 0;
            n = GetEnvironmentVariableA("OS2_PM_TRACE", value, sizeof(value));
            if (n != 0 && n < sizeof(value) && value[0] != '0') {
                fprintf(stderr, "PMSHAPI: PrfOpenProfile hab=%08lX hini=%08lX file=%s\n",
                        (unsigned long)hab, (unsigned long)h,
                        g_profiles[i].path);
                fflush(stderr);
            }
            return h;
        }
    }
    return 0;
}

/* 120 */
O2HSWITCH __cdecl WinAddSwitchEntry(void *switchControl)
{
    char value[8];
    DWORD n;
    n = GetEnvironmentVariableA("OS2_PM_TRACE", value, sizeof(value));
    if (n != 0 && n < sizeof(value) && value[0] != '0') {
        fprintf(stderr, "PMSHAPI: WinAddSwitchEntry swctl=%08lX\n",
                (unsigned long)(DWORD)switchControl);
        fflush(stderr);
    }
    return 1;
}

/* 123 - WinChangeSwitchEntry.  Win32 already owns the task-switch entry;
   accept title/icon/state updates from PM applications. */
WORD __cdecl WinChangeSwitchEntry(O2HSWITCH hswitch, void *switchControl)
{
    char value[8];
    DWORD n;
    (void)hswitch; (void)switchControl;
    n = GetEnvironmentVariableA("OS2_PM_TRACE", value, sizeof(value));
    if (n != 0 && n < sizeof(value) && value[0] != '0') {
        fprintf(stderr, "PMSHAPI: WinChangeSwitchEntry hsw=%08lX swctl=%08lX\n",
                (unsigned long)hswitch, (unsigned long)(DWORD)switchControl);
        fflush(stderr);
    }
    return 0;
}

/* 129 - WinRemoveSwitchEntry.  The host top-level window is already owned by
   Win32's task switcher; the HSWITCH returned by WinAddSwitchEntry is a
   compatibility token, so removal is an advisory successful operation. */
DWORD __cdecl WinRemoveSwitchEntry(O2HSWITCH hswitch)
{
    char value[8];
    DWORD n;
    n = GetEnvironmentVariableA("OS2_PM_TRACE", value, sizeof(value));
    if (n != 0 && n < sizeof(value) && value[0] != '0') {
        fprintf(stderr, "PMSHAPI: WinRemoveSwitchEntry hsw=%08lX\n",
                (unsigned long)hswitch);
        fflush(stderr);
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)instance; (void)reason; (void)reserved;
    return TRUE;
}

static int pmsh_hex_value(int ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

/* 117 - binary profile values.  The compatibility profile is a normal host
   INI file, so preserve arbitrary OS/2 bytes as an explicit hexadecimal
   payload.  This keeps private HINI values persistent across runs without
   confusing them with PrfQueryProfileString data. */
BOOL __cdecl PrfQueryProfileData(DWORD hini, const char *app,
                                 const char *key, void *buffer, DWORD *pcb)
{
    char fallback[MAX_PATH];
    const char *path;
    char *encoded;
    DWORD cap, n, need, i;
    unsigned char *dst;
    int hi, lo;
    if (!app || !key || !pcb)
        return FALSE;
    path = pmsh_profile_path(hini, fallback, sizeof(fallback));
    cap = 65536UL;
    encoded = (char *)HeapAlloc(GetProcessHeap(), 0, cap);
    if (!encoded)
        return FALSE;
    encoded[0] = 0;
    n = GetPrivateProfileStringA(app, key, "", encoded, cap, path);
    if (n < 5UL || memcmp(encoded, "@HEX:", 5) != 0 || ((n - 5UL) & 1UL)) {
        HeapFree(GetProcessHeap(), 0, encoded);
        return FALSE;
    }
    need = (n - 5UL) / 2UL;
    if (!buffer || *pcb < need) {
        *pcb = need;
        HeapFree(GetProcessHeap(), 0, encoded);
        return FALSE;
    }
    dst = (unsigned char *)buffer;
    for (i = 0; i < need; ++i) {
        hi = pmsh_hex_value((unsigned char)encoded[5UL + i * 2UL]);
        lo = pmsh_hex_value((unsigned char)encoded[6UL + i * 2UL]);
        if (hi < 0 || lo < 0) {
            HeapFree(GetProcessHeap(), 0, encoded);
            return FALSE;
        }
        dst[i] = (unsigned char)((hi << 4) | lo);
    }
    *pcb = need;
    HeapFree(GetProcessHeap(), 0, encoded);
    return TRUE;
}

/* 118 */
BOOL __cdecl PrfWriteProfileData(DWORD hini, const char *app,
                                 const char *key, const void *data, DWORD cb)
{
    static const char hex[] = "0123456789ABCDEF";
    char fallback[MAX_PATH];
    const char *path;
    const unsigned char *src;
    char *encoded;
    DWORD i, chars;
    BOOL ok;
    if (!app || !key)
        return FALSE;
    path = pmsh_profile_path(hini, fallback, sizeof(fallback));
    if (!data && cb == 0)
        return WritePrivateProfileStringA(app, key, NULL, path);
    if (!data || cb > 32760UL)
        return FALSE;
    chars = 5UL + cb * 2UL;
    encoded = (char *)HeapAlloc(GetProcessHeap(), 0, chars + 1UL);
    if (!encoded)
        return FALSE;
    memcpy(encoded, "@HEX:", 5);
    src = (const unsigned char *)data;
    for (i = 0; i < cb; ++i) {
        encoded[5UL + i * 2UL] = hex[(src[i] >> 4) & 15U];
        encoded[6UL + i * 2UL] = hex[src[i] & 15U];
    }
    encoded[chars] = 0;
    ok = WritePrivateProfileStringA(app, key, encoded, path);
    HeapFree(GetProcessHeap(), 0, encoded);
    return ok;
}

/* 114 - persist HINI_USERPROFILE values in a small host-side INI file. */
SHORT __cdecl PrfQueryProfileInt(DWORD hini, const char *app,
                                const char *key, LONG defval)
{
    char fallback[MAX_PATH];
    const char *path;
    if (!app || !key)
        return defval;
    path = pmsh_profile_path(hini, fallback, sizeof(fallback));
    return (SHORT)GetPrivateProfileIntA(app, key, (INT)defval, path);
}

/* 116 */
BOOL __cdecl PrfWriteProfileString(DWORD hini, const char *app,
                                   const char *key, const char *value)
{
    char fallback[MAX_PATH];
    const char *path;
    if (!app || !key)
        return FALSE;
    path = pmsh_profile_path(hini, fallback, sizeof(fallback));
    return WritePrivateProfileStringA(app, key, value, path);
}
