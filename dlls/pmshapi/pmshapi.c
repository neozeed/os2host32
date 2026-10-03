/* PMSHAPI profile personality. Switch-list support is in switchlist.c. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

#ifndef __cdecl
#define __cdecl
#endif

#include "switchlist.h"

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
    if (hini != 0 && hini != 0xffffffffUL && hini != 0xfffffffeUL)
        return NULL;
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
    if (strlen(fileName) >= MAX_PATH)
        return 0;
    for (i = 0; i < PMCOMPAT_MAX_PROFILES; ++i) {
        if (g_profiles[i].handle == 0) {
            h = g_next_profile_handle++;
            if (h == 0 || h == 0xffffffffUL || h == 0xfffffffeUL)
                h = g_next_profile_handle++;
            g_profiles[i].handle = h;
            /* Resolve now: later working-directory changes must not redirect
               an open profile, nor should Win32 search the Windows directory. */
            n = GetFullPathNameA(fileName, MAX_PATH, g_profiles[i].path, NULL);
            if (!n || n >= MAX_PATH) {
                memset(&g_profiles[i],0,sizeof(g_profiles[i]));
                return 0;
            }
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

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)instance; (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) pmsh_switch_init();
    if (reason == DLL_PROCESS_DETACH) pmsh_switch_term();
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
    if (!path) return FALSE;
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
    if (!path) return FALSE;
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
    if (!path) return (SHORT)defval;
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
    if (!path) return FALSE;
    return WritePrivateProfileStringA(app, key, value, path);
}

/* 103. Win32 profile operations own no persistent open file handle. Flush
   its cache, then retire our HINI; stale handles must never fall back to USER. */
BOOL __cdecl PrfCloseProfile(DWORD hini)
{
    unsigned i;
    for (i=0; i<PMCOMPAT_MAX_PROFILES; ++i) {
        if (hini && g_profiles[i].handle==hini) {
            /* The documented cache-flush form can return zero even when no
               data was pending. Its return does not describe HINI validity. */
            WritePrivateProfileStringA(NULL,NULL,NULL,g_profiles[i].path);
            memset(&g_profiles[i],0,sizeof(g_profiles[i]));
            return TRUE;
        }
    }
    return FALSE;
}

/* 101. Report decoded byte size for our existing @HEX: binary encoding;
   plain strings include their NUL, name lists include the final extra NUL. */
BOOL __cdecl PrfQueryProfileSize(DWORD hini,const char *app,const char *key,DWORD *size)
{
    char fallback[MAX_PATH], *buffer;
    const char *path;
    DWORD n,i,cap;
    BOOL ok;
    if (!size || (!app && key)) return FALSE;
    *size=0;
    path=pmsh_profile_path(hini,fallback,sizeof(fallback));
    if (!path) return FALSE;
    cap=65536UL;
    buffer=(char *)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,cap);
    if (!buffer) return FALSE;
    /* A nonempty sentinel distinguishes a missing key from an empty value. */
    n=GetPrivateProfileStringA(app,key,"\001",buffer,cap,path);
    ok=TRUE;
    if (n>=cap-2 || (app && key && n==1 && buffer[0]=='\001')) ok=FALSE;
    else if (!app || !key) *size=n ? n+1 : 0;
    else if (n>=5 && memcmp(buffer,"@HEX:",5)==0) {
        if ((n-5)&1) ok=FALSE;
        for(i=5;ok && i<n;++i) if(pmsh_hex_value((unsigned char)buffer[i])<0) ok=FALSE;
        if(ok) *size=(n-5)/2;
    } else *size=n+1;
    HeapFree(GetProcessHeap(),0,buffer);
    return ok;
}
