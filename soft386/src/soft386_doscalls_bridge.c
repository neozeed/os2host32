/* Native DOSCALLS.DLL marshalling bridge for Soft386.
 *
 * The guest CPU never exposes a host pointer to DOSCALLS.DLL.  Every pointer
 * argument is copied into a bounded host temporary, and every output is copied
 * back only after guest-range validation.  HFILE/HDIR values are intentionally
 * left opaque: while this bridge is enabled, the native DLL owns that handle
 * namespace and all related file calls stay on this side of the boundary.
 */
#include "soft386_doscalls_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <pthread.h>
#include <stdatomic.h>
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifndef __cdecl
#define __cdecl
#endif

#define OS2_NO_ERROR                0u
#define OS2_ERROR_INVALID_FUNCTION  1u
#define OS2_ERROR_NOT_ENOUGH_MEMORY 8u
#define OS2_ERROR_INVALID_PARAMETER 87u
#define BRIDGE_MAX_STRING           4096u
#define BRIDGE_MAX_BUFFER           0x04000000u
#define BRIDGE_MAX_ARG_BLOCK        0x00010000u
#define BRIDGE_MAX_ENV_BLOCK        0x00020000u

typedef uint32_t O2RET;
typedef uint32_t O2ULONG;
typedef int32_t  O2LONG;
typedef uint32_t O2HFILE;
struct O2DosQword { O2ULONG lo, hi; };
struct O2ResultCodes { O2ULONG codeTerminate, codeResult; };

#ifdef _WIN32
struct Win32DoscallsModule { HMODULE module; };
static void *win32_get_proc(void *opaque, uint32_t ordinal)
{
    struct Win32DoscallsModule *m = (struct Win32DoscallsModule *)opaque;
    FARPROC p;
    if (!m || !m->module || ordinal > 65535u) return NULL;
    p = GetProcAddress(m->module, (LPCSTR)(uintptr_t)(uint16_t)ordinal);
    return (void *)(uintptr_t)p;
}
static void win32_close(void *opaque)
{
    struct Win32DoscallsModule *m = (struct Win32DoscallsModule *)opaque;
    if (!m) return;
    if (m->module) FreeLibrary(m->module);
    free(m);
}
#endif

void soft386_doscalls_bridge_set_provider(struct Soft386NativeDoscalls *bridge,
                                          void *opaque,
                                          void *(*get_proc)(void *, uint32_t),
                                          void (*close_fn)(void *),
                                          int trace)
{
    if (!bridge) return;
    memset(bridge, 0, sizeof(*bridge));
    bridge->opaque = opaque;
    bridge->get_proc = get_proc;
    bridge->close = close_fn;
    bridge->trace = trace;
    bridge->loaded = get_proc != NULL;
}

int soft386_doscalls_bridge_open(struct Soft386NativeDoscalls *bridge,
                                 const char *path, int required, int trace)
{
    if (!bridge) return 0;
    memset(bridge, 0, sizeof(*bridge));
#ifdef _WIN32
    {
        struct Win32DoscallsModule *m;
        HMODULE module;
        const char *name = (path && path[0]) ? path : "DOSCALLS.dll";
        module = LoadLibraryA(name);
        if (!module) {
            if (required)
                fprintf(stderr, "soft386: cannot load native DOSCALLS bridge %s (Win32 error %lu)\n",
                        name, (unsigned long)GetLastError());
            else if (trace)
                fprintf(stderr, "soft386: native DOSCALLS bridge not loaded: %s\n", name);
            return 0;
        }
        m = (struct Win32DoscallsModule *)calloc(1, sizeof(*m));
        if (!m) { FreeLibrary(module); return 0; }
        m->module = module;
        soft386_doscalls_bridge_set_provider(bridge, m, win32_get_proc, win32_close, trace);
        if (trace)
            fprintf(stderr, "soft386: native DOSCALLS bridge loaded: %s\n", name);
        return 1;
    }
#else
    (void)path;
    if (required)
        fprintf(stderr, "soft386: native DOSCALLS.DLL bridge is available only on Win32\n");
    (void)trace;
    return 0;
#endif
}

static void close_async_jobs(struct Soft386NativeDoscalls *bridge);

void soft386_doscalls_bridge_close(struct Soft386NativeDoscalls *bridge)
{
    if (!bridge) return;
    close_async_jobs(bridge);
    if (bridge->close && bridge->opaque) bridge->close(bridge->opaque);
    memset(bridge, 0, sizeof(*bridge));
}

/* H1 ABI registry.  This table describes representation/execution only;
 * it must not grow OS/2 API semantics.  SCALAR entries use the common scalar
 * call engine.  MARSHALLED entries name only an ABI boundary that requires
 * guest-memory translation in the switch below. */
struct Soft386DosAbiDesc { uint32_t ordinal; unsigned nargs, flags; const char *name; };
static const struct Soft386DosAbiDesc dos_abi[] = {
    { 110u, 1u, SOFT386_DOS_ABI_MARSHALLED, "DosForceDelete" },
    { 209u, 1u, SOFT386_DOS_ABI_SCALAR, "DosSetMaxFH" },
    { 218u, 4u, SOFT386_DOS_ABI_MARSHALLED, "DosSetFileInfo" },
    { 232u, 0u, SOFT386_DOS_ABI_SCALAR, "DosEnterCritSec" },
    { 219u, 5u, SOFT386_DOS_ABI_MARSHALLED, "DosSetPathInfo" },
    { 220u, 1u, SOFT386_DOS_ABI_SCALAR, "DosSetDefaultDisk" },
    { 221u, 2u, SOFT386_DOS_ABI_SCALAR, "DosSetFHState" },
    { 223u, 4u, SOFT386_DOS_ABI_MARSHALLED, "DosQueryPathInfo" },
    { 224u, 3u, SOFT386_DOS_ABI_MARSHALLED, "DosQueryHType" },
    { 226u, 1u, SOFT386_DOS_ABI_MARSHALLED, "DosDeleteDir" },
    { 239u, 3u, SOFT386_DOS_ABI_MARSHALLED, "DosCreatePipe" },
    { 254u, 1u, SOFT386_DOS_ABI_SCALAR, "DosResetBuffer" },
    { 255u, 1u, SOFT386_DOS_ABI_MARSHALLED, "DosSetCurrentDir" },
    { 256u, 4u, SOFT386_DOS_ABI_MARSHALLED, "DosSetFilePtr" },
    { 257u, 1u, SOFT386_DOS_ABI_SCALAR, "DosClose" },
    { 258u, 3u, SOFT386_DOS_ABI_MARSHALLED, "DosCopy" },
    { 259u, 2u, SOFT386_DOS_ABI_MARSHALLED, "DosDelete" },
    { 260u, 2u, SOFT386_DOS_ABI_MARSHALLED, "DosDupHandle" },
    { 263u, 1u, SOFT386_DOS_ABI_SCALAR, "DosFindClose" },
    { 264u, 7u, SOFT386_DOS_ABI_MARSHALLED, "DosFindFirst" },
    { 265u, 4u, SOFT386_DOS_ABI_MARSHALLED, "DosFindNext" },
    { 270u, 3u, SOFT386_DOS_ABI_MARSHALLED, "DosCreateDir" },
    { 271u, 2u, SOFT386_DOS_ABI_MARSHALLED, "DosMove" },
    { 272u, 2u, SOFT386_DOS_ABI_SCALAR, "DosSetFileSize" },
    { 273u, 8u, SOFT386_DOS_ABI_MARSHALLED, "DosOpen" },
    { 274u, 3u, SOFT386_DOS_ABI_MARSHALLED, "DosQueryCurrentDir" },
    { 275u, 2u, SOFT386_DOS_ABI_MARSHALLED, "DosQueryCurrentDisk" },
    { 276u, 2u, SOFT386_DOS_ABI_MARSHALLED, "DosQueryFHState" },
    { 278u, 4u, SOFT386_DOS_ABI_MARSHALLED, "DosQueryFSInfo" },
    { 279u, 4u, SOFT386_DOS_ABI_MARSHALLED, "DosQueryFileInfo" },
    { 280u, 5u, SOFT386_DOS_ABI_MARSHALLED, "DosWaitChild" },
    { 281u, 4u, SOFT386_DOS_ABI_MARSHALLED, "DosRead" },
    { 282u, 4u, SOFT386_DOS_ABI_MARSHALLED, "DosWrite" },
    { 283u, 7u, SOFT386_DOS_ABI_MARSHALLED, "DosExecPgm" },
    { 286u, 2u, SOFT386_DOS_ABI_SCALAR | SOFT386_DOS_MAY_BLOCK, "DosBeep" },
    { 323u, 2u, SOFT386_DOS_ABI_MARSHALLED, "DosQueryAppType" },
    { 362u, 1u, SOFT386_DOS_ABI_MARSHALLED, "DosTmrQueryFreq" },
    { 363u, 1u, SOFT386_DOS_ABI_MARSHALLED, "DosTmrQueryTime" },
    { 382u, 2u, SOFT386_DOS_ABI_MARSHALLED, "DosSetRelMaxFH" }
};
static const struct Soft386DosAbiDesc *abi_desc(uint32_t ordinal)
{
    unsigned i;
    for (i = 0; i < sizeof(dos_abi)/sizeof(dos_abi[0]); ++i)
        if (dos_abi[i].ordinal == ordinal) return &dos_abi[i];
    return NULL;
}
unsigned soft386_doscalls_bridge_abi(uint32_t ordinal, unsigned *nargs)
{
    const struct Soft386DosAbiDesc *d = abi_desc(ordinal);
    if (nargs) *nargs = d ? d->nargs : 0u;
    return d ? d->flags : 0u;
}

/* Bridged service admission is now descriptor-driven.  Guest-local APIs are
 * absent from this registry and therefore fall through to the jar handlers. */
int soft386_doscalls_bridge_ordinal(uint32_t ordinal)
{
    return abi_desc(ordinal) != NULL;
}

/* Service availability belongs to the provider, not to the ABI table. */
int soft386_doscalls_bridge_export(struct Soft386NativeDoscalls *bridge, uint32_t ordinal)
{
    if (!bridge || !bridge->loaded || !bridge->get_proc) return 0;
    return bridge->get_proc(bridge->opaque, ordinal) != NULL;
}

static uint32_t arg32(const struct Soft386GuestMemoryOps *m, uint32_t esp, unsigned n)
{
    return m->read_u32(m->opaque, esp + 4u + 4u * (uint32_t)n);
}
static unsigned bridge_nargs(uint32_t ordinal)
{
    const struct Soft386DosAbiDesc *d = abi_desc(ordinal);
    return d ? d->nargs : 0u;
}

static int need(const struct Soft386GuestMemoryOps *m, uint32_t p, uint32_t cb, int wr)
{
    if (!p && cb) return 0;
    return m->valid(m->opaque, p, cb, wr);
}
static int get_cstr(const struct Soft386GuestMemoryOps *m, uint32_t p, char *s, uint32_t cap)
{
    if (!p || !s || cap == 0) return 0;
    return m->read_cstr(m->opaque, p, s, cap);
}
static void *temp_alloc(uint32_t cb)
{
    if (cb > BRIDGE_MAX_BUFFER) return NULL;
    if (cb == 0) cb = 1;
    return calloc(1, cb);
}

/* Copy a guest NUL-list (argv block or environment block) into host memory.
 * Both OS/2 forms terminate with an empty string, i.e. a double NUL. */
static char *copy_nul_list(const struct Soft386GuestMemoryOps *m,
                           uint32_t address, uint32_t limit, int *ok)
{
    char *p;
    uint32_t i;
    unsigned zero_run;
    unsigned char ch;

    if (ok) *ok = 0;
    if (!address) {
        if (ok) *ok = 1;
        return NULL;
    }
    p = (char *)malloc(limit ? limit : 1u);
    if (!p) return NULL;
    zero_run = 0;
    for (i = 0; i < limit; ++i) {
        if (!m->read(m->opaque, address + i, &ch, 1u)) {
            free(p);
            return NULL;
        }
        p[i] = (char)ch;
        if (ch == 0) {
            ++zero_run;
            if (zero_run == 2u) {
                if (ok) *ok = 1;
                return p;
            }
        } else {
            zero_run = 0;
        }
    }
    free(p);
    return NULL;
}
static void *proc(struct Soft386NativeDoscalls *b, uint32_t ord)
{
    return (b && b->loaded && b->get_proc) ? b->get_proc(b->opaque, ord) : NULL;
}
static void trace_call(struct Soft386NativeDoscalls *b, uint32_t ord, const char *name)
{
    if (b && b->trace) fprintf(stderr, "soft386: native DOSCALLS.%u %s [marshalled]\n", ord, name);
}
static void trace_return(struct Soft386NativeDoscalls *b, uint32_t ord, uint32_t rc)
{
    if (b && b->trace) fprintf(stderr, "soft386: native DOSCALLS.%u returned rc=%u\n", ord, rc);
}
static uint32_t missing(struct Soft386NativeDoscalls *b, uint32_t ord)
{
    if (b && b->trace) fprintf(stderr, "soft386: native DOSCALLS.%u export missing\n", ord);
    return OS2_ERROR_INVALID_FUNCTION;
}

/* Cast through uintptr_t so GCC accepts FARPROC/void* providers cleanly on the
 * intended 32-bit Win32 build. */
#define PCAST(type, p) ((type)(uintptr_t)(p))

static uint32_t call_scalar(void *vp, unsigned nargs, const uint32_t *a)
{
    switch (nargs) {
    case 0: return PCAST(O2RET (__cdecl *)(void),vp)();
    case 1: return PCAST(O2RET (__cdecl *)(O2ULONG),vp)(a[0]);
    case 2: return PCAST(O2RET (__cdecl *)(O2ULONG,O2ULONG),vp)(a[0],a[1]);
    case 3: return PCAST(O2RET (__cdecl *)(O2ULONG,O2ULONG,O2ULONG),vp)(a[0],a[1],a[2]);
    case 4: return PCAST(O2RET (__cdecl *)(O2ULONG,O2ULONG,O2ULONG,O2ULONG),vp)(a[0],a[1],a[2],a[3]);
    case 5: return PCAST(O2RET (__cdecl *)(O2ULONG,O2ULONG,O2ULONG,O2ULONG,O2ULONG),vp)(a[0],a[1],a[2],a[3],a[4]);
    case 6: return PCAST(O2RET (__cdecl *)(O2ULONG,O2ULONG,O2ULONG,O2ULONG,O2ULONG,O2ULONG),vp)(a[0],a[1],a[2],a[3],a[4],a[5]);
    default: return OS2_ERROR_INVALID_PARAMETER;
    }
}

uint32_t soft386_doscalls_bridge_dispatch(
    struct Soft386NativeDoscalls *b,
    const struct Soft386GuestMemoryOps *m,
    uint32_t ordinal, uint32_t esp, int *handled)
{
    void *vp;
    uint32_t a[9], rc, v1, v2;
    unsigned i, nargs;
    char s1[BRIDGE_MAX_STRING], s2[BRIDGE_MAX_STRING];
    void *buf;

    if (handled) *handled = soft386_doscalls_bridge_ordinal(ordinal);
    if (!soft386_doscalls_bridge_ordinal(ordinal)) return OS2_ERROR_INVALID_FUNCTION;
    if (!b || !b->loaded) return OS2_ERROR_INVALID_FUNCTION;
    if (!m || !m->valid || !m->read || !m->write || !m->read_cstr || !m->read_u32)
        return OS2_ERROR_INVALID_PARAMETER;
    nargs = bridge_nargs(ordinal);
    if (nargs && !m->valid(m->opaque, esp + 4u, nargs * 4u, 0))
        return OS2_ERROR_INVALID_PARAMETER;
    memset(a, 0, sizeof(a));
    for (i = 0; i < nargs; ++i) a[i] = arg32(m, esp, i);
    vp = proc(b, ordinal);
    if (!vp) return missing(b, ordinal);

    {
        const struct Soft386DosAbiDesc *d = abi_desc(ordinal);
        if (d && (d->flags & SOFT386_DOS_ABI_SCALAR)) {
            trace_call(b, ordinal, d->name);
            rc = call_scalar(vp, d->nargs, a);
            trace_return(b, ordinal, rc);
            return rc;
        }
    }

    switch (ordinal) {
    case 110: { /* DosForceDelete(path) */
        typedef O2RET (__cdecl *Fn)(const char *);
        trace_call(b,ordinal,"DosForceDelete");
        if (!get_cstr(m,a[0],s1,sizeof(s1))) return OS2_ERROR_INVALID_PARAMETER;
        return PCAST(Fn,vp)(s1);
    }
    case 258: { /* DosCopy(from,to,flags) */
        typedef O2RET (__cdecl *Fn)(const char *,const char *,O2ULONG);
        trace_call(b,ordinal,"DosCopy");
        if (!get_cstr(m,a[0],s1,sizeof(s1)) || !get_cstr(m,a[1],s2,sizeof(s2))) return OS2_ERROR_INVALID_PARAMETER;
        return PCAST(Fn,vp)(s1,s2,a[2]);
    }
    case 276: { /* DosQueryFHState(h,&mode) */
        typedef O2RET (__cdecl *Fn)(O2HFILE,O2ULONG *);
        trace_call(b,ordinal,"DosQueryFHState"); if(!need(m,a[1],4,1))return OS2_ERROR_INVALID_PARAMETER;
        v1=0; rc=PCAST(Fn,vp)(a[0],&v1); if(!rc&&!m->write(m->opaque,a[1],&v1,4))return OS2_ERROR_INVALID_PARAMETER; return rc;
    }
    case 278: { /* DosQueryFSInfo(disk,level,buf,cb) */
        typedef O2RET (__cdecl *Fn)(O2ULONG,O2ULONG,void *,O2ULONG);
        trace_call(b,ordinal,"DosQueryFSInfo"); if(a[3]&&!need(m,a[2],a[3],1))return OS2_ERROR_INVALID_PARAMETER;
        buf=temp_alloc(a[3]); if(!buf)return OS2_ERROR_NOT_ENOUGH_MEMORY; rc=PCAST(Fn,vp)(a[0],a[1],buf,a[3]);
        if (!rc && a[3] && !m->write(m->opaque, a[2], buf, a[3]))
            rc = OS2_ERROR_INVALID_PARAMETER;
        free(buf);
        return rc;
    }
    case 362: { typedef O2RET (__cdecl *Fn)(O2ULONG *); trace_call(b,ordinal,"DosTmrQueryFreq"); if(!need(m,a[0],4,1))return OS2_ERROR_INVALID_PARAMETER; v1=0;rc=PCAST(Fn,vp)(&v1);if(!rc&&!m->write(m->opaque,a[0],&v1,4))return OS2_ERROR_INVALID_PARAMETER;return rc; }
    case 363: { typedef O2RET (__cdecl *Fn)(struct O2DosQword *); struct O2DosQword q; trace_call(b,ordinal,"DosTmrQueryTime"); if(!need(m,a[0],8,1))return OS2_ERROR_INVALID_PARAMETER; memset(&q,0,sizeof(q));rc=PCAST(Fn,vp)(&q);if(!rc&&!m->write(m->opaque,a[0],&q,8))return OS2_ERROR_INVALID_PARAMETER;return rc; }
    case 382: { typedef O2RET (__cdecl *Fn)(O2LONG *,O2ULONG *); O2LONG req; O2ULONG cur; trace_call(b,ordinal,"DosSetRelMaxFH"); if(!need(m,a[0],4,1)||!need(m,a[1],4,1))return OS2_ERROR_INVALID_PARAMETER; req=(O2LONG)m->read_u32(m->opaque,a[0]);cur=m->read_u32(m->opaque,a[1]);rc=PCAST(Fn,vp)(&req,&cur);if(!rc){if(!m->write(m->opaque,a[0],&req,4)||!m->write(m->opaque,a[1],&cur,4))return OS2_ERROR_INVALID_PARAMETER;}return rc; }
    case 223: { /* path,level,buffer,cb */
        typedef O2RET (__cdecl *Fn)(const char *,O2ULONG,void *,O2ULONG);
        trace_call(b,ordinal,"DosQueryPathInfo"); if(!get_cstr(m,a[0],s1,sizeof(s1))||(a[3]&&!need(m,a[2],a[3],1)))return OS2_ERROR_INVALID_PARAMETER;
        buf=temp_alloc(a[3]);if(!buf)return OS2_ERROR_NOT_ENOUGH_MEMORY;rc=PCAST(Fn,vp)(s1,a[1],buf,a[3]);if(!rc&&a[3]&&!m->write(m->opaque,a[2],buf,a[3]))rc=OS2_ERROR_INVALID_PARAMETER;free(buf);return rc;
    }
    case 224: { typedef O2RET (__cdecl *Fn)(O2HFILE,O2ULONG *,O2ULONG *); trace_call(b,ordinal,"DosQueryHType"); if(!need(m,a[1],4,1)||!need(m,a[2],4,1))return OS2_ERROR_INVALID_PARAMETER;v1=v2=0;rc=PCAST(Fn,vp)(a[0],&v1,&v2);trace_return(b,ordinal,rc);if(!rc){if(!m->write(m->opaque,a[1],&v1,4)||!m->write(m->opaque,a[2],&v2,4))return OS2_ERROR_INVALID_PARAMETER;}return rc; }
    case 226: { typedef O2RET (__cdecl *Fn)(const char *); trace_call(b,ordinal,"DosDeleteDir"); if(!get_cstr(m,a[0],s1,sizeof(s1)))return OS2_ERROR_INVALID_PARAMETER;return PCAST(Fn,vp)(s1); }
    case 239: { typedef O2RET (__cdecl *Fn)(O2HFILE *,O2HFILE *,O2ULONG); trace_call(b,ordinal,"DosCreatePipe"); if(!need(m,a[0],4,1)||!need(m,a[1],4,1))return OS2_ERROR_INVALID_PARAMETER;v1=v2=0;rc=PCAST(Fn,vp)(&v1,&v2,a[2]);if(!rc){if(!m->write(m->opaque,a[0],&v1,4)||!m->write(m->opaque,a[1],&v2,4))return OS2_ERROR_INVALID_PARAMETER;}return rc; }
    case 255: { typedef O2RET (__cdecl *Fn)(const char *); trace_call(b,ordinal,"DosSetCurrentDir"); if(!get_cstr(m,a[0],s1,sizeof(s1)))return OS2_ERROR_INVALID_PARAMETER;return PCAST(Fn,vp)(s1); }
    case 256: { typedef O2RET (__cdecl *Fn)(O2HFILE,O2LONG,O2ULONG,O2ULONG *); trace_call(b,ordinal,"DosSetFilePtr"); if(!need(m,a[3],4,1))return OS2_ERROR_INVALID_PARAMETER;v1=0;rc=PCAST(Fn,vp)(a[0],(O2LONG)a[1],a[2],&v1);if(!rc&&!m->write(m->opaque,a[3],&v1,4))return OS2_ERROR_INVALID_PARAMETER;return rc; }
    case 259: { typedef O2RET (__cdecl *Fn)(const char *,O2ULONG); trace_call(b,ordinal,"DosDelete"); if(!get_cstr(m,a[0],s1,sizeof(s1)))return OS2_ERROR_INVALID_PARAMETER;return PCAST(Fn,vp)(s1,a[1]); }
    case 260: { typedef O2RET (__cdecl *Fn)(O2HFILE,O2HFILE *); trace_call(b,ordinal,"DosDupHandle"); if(!need(m,a[1],4,1))return OS2_ERROR_INVALID_PARAMETER;v1=m->read_u32(m->opaque,a[1]);rc=PCAST(Fn,vp)(a[0],&v1);if(!rc&&!m->write(m->opaque,a[1],&v1,4))return OS2_ERROR_INVALID_PARAMETER;return rc; }
    case 264: { /* filespec,phdir,attrs,findbuf,cb,pcount,level */
        typedef O2RET (__cdecl *Fn)(const char *,O2ULONG *,O2ULONG,void *,O2ULONG,O2ULONG *,O2ULONG);
        O2ULONG hdir,count; uint32_t level=arg32(m,esp,6);
        trace_call(b,ordinal,"DosFindFirst");
        if(!get_cstr(m,a[0],s1,sizeof(s1))||!need(m,a[1],4,1)||!need(m,a[5],4,1)||(a[4]&&!need(m,a[3],a[4],1)))return OS2_ERROR_INVALID_PARAMETER;
        hdir=m->read_u32(m->opaque,a[1]);count=m->read_u32(m->opaque,a[5]);buf=temp_alloc(a[4]);if(!buf)return OS2_ERROR_NOT_ENOUGH_MEMORY;
        rc=PCAST(Fn,vp)(s1,&hdir,a[2],buf,a[4],&count,level);
        if(!m->write(m->opaque,a[1],&hdir,4)||!m->write(m->opaque,a[5],&count,4)||(a[4]&&!m->write(m->opaque,a[3],buf,a[4])))rc=OS2_ERROR_INVALID_PARAMETER;
        free(buf);return rc;
    }
    case 265: { typedef O2RET (__cdecl *Fn)(O2ULONG,void *,O2ULONG,O2ULONG *); O2ULONG count; trace_call(b,ordinal,"DosFindNext"); if(!need(m,a[3],4,1)||(a[2]&&!need(m,a[1],a[2],1)))return OS2_ERROR_INVALID_PARAMETER;count=m->read_u32(m->opaque,a[3]);buf=temp_alloc(a[2]);if(!buf)return OS2_ERROR_NOT_ENOUGH_MEMORY;rc=PCAST(Fn,vp)(a[0],buf,a[2],&count);if(!m->write(m->opaque,a[3],&count,4)||(a[2]&&!m->write(m->opaque,a[1],buf,a[2])))rc=OS2_ERROR_INVALID_PARAMETER;free(buf);return rc; }
    case 270: { typedef O2RET (__cdecl *Fn)(const char *,void *,O2ULONG); trace_call(b,ordinal,"DosCreateDir"); if(!get_cstr(m,a[0],s1,sizeof(s1)))return OS2_ERROR_INVALID_PARAMETER; if(a[1])return OS2_ERROR_INVALID_PARAMETER; return PCAST(Fn,vp)(s1,NULL,a[2]); }
    case 271: { typedef O2RET (__cdecl *Fn)(const char *,const char *); trace_call(b,ordinal,"DosMove");if(!get_cstr(m,a[0],s1,sizeof(s1))||!get_cstr(m,a[1],s2,sizeof(s2)))return OS2_ERROR_INVALID_PARAMETER;return PCAST(Fn,vp)(s1,s2); }
    case 273: { /* ABI8 guest: path,phfile,paction,cb,attr,flags,mode,pEA */
        typedef O2RET (__cdecl *Fn)(const char *,O2HFILE *,O2ULONG *,O2ULONG,O2ULONG,O2ULONG,O2ULONG,void *,O2ULONG);
        O2HFILE hf; O2ULONG action; uint32_t ea=arg32(m,esp,7);
        trace_call(b,ordinal,"DosOpen"); if(!get_cstr(m,a[0],s1,sizeof(s1))||!need(m,a[1],4,1)||!need(m,a[2],4,1)||ea)return OS2_ERROR_INVALID_PARAMETER;
        hf=m->read_u32(m->opaque,a[1]);action=m->read_u32(m->opaque,a[2]);rc=PCAST(Fn,vp)(s1,&hf,&action,a[3],a[4],a[5],arg32(m,esp,6),NULL,0);
        if(!rc){if(!m->write(m->opaque,a[1],&hf,4)||!m->write(m->opaque,a[2],&action,4))return OS2_ERROR_INVALID_PARAMETER;}return rc;
    }
    case 274: { typedef O2RET (__cdecl *Fn)(O2ULONG,char *,O2ULONG *); O2ULONG cb; trace_call(b,ordinal,"DosQueryCurrentDir"); if(!need(m,a[2],4,1))return OS2_ERROR_INVALID_PARAMETER;cb=m->read_u32(m->opaque,a[2]);if(cb&&!need(m,a[1],cb,1))return OS2_ERROR_INVALID_PARAMETER;buf=temp_alloc(cb);if(!buf)return OS2_ERROR_NOT_ENOUGH_MEMORY;rc=PCAST(Fn,vp)(a[0],(char*)buf,&cb);if(!m->write(m->opaque,a[2],&cb,4)||(!rc&&cb&& !m->write(m->opaque,a[1],buf,cb)))rc=OS2_ERROR_INVALID_PARAMETER;free(buf);return rc; }
    case 275: { typedef O2RET (__cdecl *Fn)(O2ULONG *,O2ULONG *);trace_call(b,ordinal,"DosQueryCurrentDisk");if(!need(m,a[0],4,1)||!need(m,a[1],4,1))return OS2_ERROR_INVALID_PARAMETER;v1=v2=0;rc=PCAST(Fn,vp)(&v1,&v2);if(!rc){if(!m->write(m->opaque,a[0],&v1,4)||!m->write(m->opaque,a[1],&v2,4))return OS2_ERROR_INVALID_PARAMETER;}return rc; }
    case 279: { typedef O2RET (__cdecl *Fn)(O2HFILE,O2ULONG,void *,O2ULONG);trace_call(b,ordinal,"DosQueryFileInfo");if(a[3]&&!need(m,a[2],a[3],1))return OS2_ERROR_INVALID_PARAMETER;buf=temp_alloc(a[3]);if(!buf)return OS2_ERROR_NOT_ENOUGH_MEMORY;rc=PCAST(Fn,vp)(a[0],a[1],buf,a[3]);if(!rc&&a[3]&&!m->write(m->opaque,a[2],buf,a[3]))rc=OS2_ERROR_INVALID_PARAMETER;free(buf);return rc; }
    case 280: { /* DosWaitChild(action,option,results,ppid,pid) */
        typedef O2RET (__cdecl *Fn)(O2ULONG,O2ULONG,struct O2ResultCodes *,O2ULONG *,O2ULONG);
        struct O2ResultCodes results; O2ULONG pid=0;
        trace_call(b,ordinal,"DosWaitChild");
        if(!need(m,a[2],8,1)||!need(m,a[3],4,1))return OS2_ERROR_INVALID_PARAMETER;
        memset(&results,0,sizeof(results));
        rc=PCAST(Fn,vp)(a[0],a[1],&results,&pid,a[4]);
        trace_return(b,ordinal,rc);
        if(!m->write(m->opaque,a[2],&results,8)||!m->write(m->opaque,a[3],&pid,4))
            return OS2_ERROR_INVALID_PARAMETER;
        return rc;
    }
    case 281: { typedef O2RET (__cdecl *Fn)(O2HFILE,void *,O2ULONG,O2ULONG *);O2ULONG actual=0;trace_call(b,ordinal,"DosRead");if(!need(m,a[3],4,1)||(a[2]&&!need(m,a[1],a[2],1)))return OS2_ERROR_INVALID_PARAMETER;buf=temp_alloc(a[2]);if(!buf)return OS2_ERROR_NOT_ENOUGH_MEMORY;rc=PCAST(Fn,vp)(a[0],buf,a[2],&actual);if(actual>a[2])actual=a[2];if((!rc&&actual&&!m->write(m->opaque,a[1],buf,actual))||!m->write(m->opaque,a[3],&actual,4))rc=OS2_ERROR_INVALID_PARAMETER;free(buf);return rc; }
    case 282: { typedef O2RET (__cdecl *Fn)(O2HFILE,const void *,O2ULONG,O2ULONG *);O2ULONG actual=0;trace_call(b,ordinal,"DosWrite");if(!need(m,a[3],4,1)||(a[2]&&!need(m,a[1],a[2],0)))return OS2_ERROR_INVALID_PARAMETER;buf=temp_alloc(a[2]);if(!buf)return OS2_ERROR_NOT_ENOUGH_MEMORY;if(a[2]&&!m->read(m->opaque,a[1],buf,a[2])){free(buf);return OS2_ERROR_INVALID_PARAMETER;}rc=PCAST(Fn,vp)(a[0],buf,a[2],&actual);if(!m->write(m->opaque,a[3],&actual,4))rc=OS2_ERROR_INVALID_PARAMETER;free(buf);return rc; }
    case 283: { /* DosExecPgm(object,cb,flag,args,env,results,program) */
        typedef O2RET (__cdecl *Fn)(char *,O2LONG,O2ULONG,const char *,const char *,struct O2ResultCodes *,const char *);
        struct O2ResultCodes results;
        char *object=NULL,*args=NULL,*env=NULL;
        int ok_args=0,ok_env=0;
        uint32_t cb;

        trace_call(b,ordinal,"DosExecPgm");
        cb=(uint32_t)a[1];
        if((O2LONG)a[1]<0 || cb>BRIDGE_MAX_STRING || !need(m,a[5],8,1) ||
           !get_cstr(m,a[6],s1,sizeof(s1))) return OS2_ERROR_INVALID_PARAMETER;
        if(cb){
            if(!need(m,a[0],cb,1))return OS2_ERROR_INVALID_PARAMETER;
            object=(char*)calloc(1,cb);
            if(!object)return OS2_ERROR_NOT_ENOUGH_MEMORY;
        }
        args=copy_nul_list(m,a[3],BRIDGE_MAX_ARG_BLOCK,&ok_args);
        if(a[3] && !ok_args){free(object);return OS2_ERROR_INVALID_PARAMETER;}
        env=copy_nul_list(m,a[4],BRIDGE_MAX_ENV_BLOCK,&ok_env);
        if(a[4] && !ok_env){free(args);free(object);return OS2_ERROR_INVALID_PARAMETER;}
        memset(&results,0,sizeof(results));
        rc=PCAST(Fn,vp)(object,(O2LONG)a[1],a[2],args,env,&results,s1);
        trace_return(b,ordinal,rc);
        if(!m->write(m->opaque,a[5],&results,8))rc=OS2_ERROR_INVALID_PARAMETER;
        if(object && cb && !m->write(m->opaque,a[0],object,cb))rc=OS2_ERROR_INVALID_PARAMETER;
        free(env);free(args);free(object);
        return rc;
    }
    case 218: { typedef O2RET (__cdecl *Fn)(O2HFILE,O2ULONG,const void *,O2ULONG);trace_call(b,ordinal,"DosSetFileInfo");if(a[3]&&!need(m,a[2],a[3],0))return OS2_ERROR_INVALID_PARAMETER;buf=temp_alloc(a[3]);if(!buf)return OS2_ERROR_NOT_ENOUGH_MEMORY;if(a[3]&&!m->read(m->opaque,a[2],buf,a[3])){free(buf);return OS2_ERROR_INVALID_PARAMETER;}rc=PCAST(Fn,vp)(a[0],a[1],buf,a[3]);free(buf);return rc; }
    case 219: { typedef O2RET (__cdecl *Fn)(const char *,O2ULONG,const void *,O2ULONG,O2ULONG);uint32_t opt=arg32(m,esp,4);trace_call(b,ordinal,"DosSetPathInfo");if(!get_cstr(m,a[0],s1,sizeof(s1))||(a[3]&&!need(m,a[2],a[3],0)))return OS2_ERROR_INVALID_PARAMETER;buf=temp_alloc(a[3]);if(!buf)return OS2_ERROR_NOT_ENOUGH_MEMORY;if(a[3]&&!m->read(m->opaque,a[2],buf,a[3])){free(buf);return OS2_ERROR_INVALID_PARAMETER;}rc=PCAST(Fn,vp)(s1,a[1],buf,a[3],opt);free(buf);return rc; }
    case 323: { typedef O2RET (__cdecl *Fn)(const char *,O2ULONG *);trace_call(b,ordinal,"DosQueryAppType");if(!get_cstr(m,a[0],s1,sizeof(s1))||!need(m,a[1],4,1))return OS2_ERROR_INVALID_PARAMETER;v1=0;rc=PCAST(Fn,vp)(s1,&v1);if(!rc&&!m->write(m->opaque,a[1],&v1,4))return OS2_ERROR_INVALID_PARAMETER;return rc; }
    default: break;
    }
    return OS2_ERROR_INVALID_FUNCTION;
}


/* Generic asynchronous scalar invocation.  This is scheduler plumbing, not
 * DosBeep semantics: any future scalar descriptor may opt into MAY_BLOCK. */
struct Soft386DosAsyncJob {
    uint32_t ordinal, esp, tid, args[6], rc;
    unsigned nargs;
    void *fn;
#ifdef _WIN32
    HANDLE thread;
#else
    pthread_t thread;
    atomic_int done;
#endif
};
static void async_execute(struct Soft386DosAsyncJob *j)
{
    j->rc = call_scalar(j->fn, j->nargs, j->args);
}
#ifdef _WIN32
static DWORD WINAPI dos_async_worker(void *p) { async_execute((struct Soft386DosAsyncJob *)p); return 0; }
static int dos_async_start(struct Soft386DosAsyncJob *j) { j->thread=CreateThread(NULL,0,dos_async_worker,j,0,NULL); return j->thread!=NULL; }
static int dos_async_done(struct Soft386DosAsyncJob *j) { return WaitForSingleObject(j->thread,0)==WAIT_OBJECT_0; }
static void dos_async_join(struct Soft386DosAsyncJob *j) { CloseHandle(j->thread); }
#else
static void *dos_async_worker(void *p) { struct Soft386DosAsyncJob *j=(struct Soft386DosAsyncJob *)p; async_execute(j); atomic_store_explicit(&j->done,1,memory_order_release); return NULL; }
static int dos_async_start(struct Soft386DosAsyncJob *j) { atomic_init(&j->done,0); return pthread_create(&j->thread,NULL,dos_async_worker,j)==0; }
static int dos_async_done(struct Soft386DosAsyncJob *j) { return atomic_load_explicit(&j->done,memory_order_acquire)!=0; }
static void dos_async_join(struct Soft386DosAsyncJob *j) { (void)pthread_join(j->thread,NULL); }
#endif

static void close_async_jobs(struct Soft386NativeDoscalls *b)
{
    unsigned i;
    if(!b)return;
    for(i=0;i<SOFT386_DOS_ASYNC_SLOTS;++i){
        struct Soft386DosAsyncJob *j=(struct Soft386DosAsyncJob *)b->async_jobs[i];
        if(!j)continue;
        dos_async_join(j);
        free(j);
        b->async_jobs[i]=NULL;
    }
}

uint32_t soft386_doscalls_bridge_dispatch_async_scalar(
    struct Soft386NativeDoscalls *b, const struct Soft386GuestMemoryOps *m,
    uint32_t ordinal, uint32_t esp, unsigned slot, uint32_t tid,
    int *waiting, int *handled)
{
    const struct Soft386DosAbiDesc *d=abi_desc(ordinal);
    struct Soft386DosAsyncJob *j;
    unsigned i;
    void *vp;
    if (waiting) *waiting=0;
    if (handled) *handled=d && (d->flags & (SOFT386_DOS_ABI_SCALAR|SOFT386_DOS_MAY_BLOCK)) == (SOFT386_DOS_ABI_SCALAR|SOFT386_DOS_MAY_BLOCK);
    if (!d || !(d->flags & SOFT386_DOS_ABI_SCALAR) || !(d->flags & SOFT386_DOS_MAY_BLOCK)) return OS2_ERROR_INVALID_FUNCTION;
    if (!b || !b->loaded || !m || !m->valid || !m->read_u32 || slot>=SOFT386_DOS_ASYNC_SLOTS) return OS2_ERROR_INVALID_PARAMETER;
    j=(struct Soft386DosAsyncJob *)b->async_jobs[slot];
    if (j) {
        if (j->ordinal!=ordinal || j->esp!=esp || j->tid!=tid) return OS2_ERROR_INVALID_PARAMETER;
        if (!dos_async_done(j)) { if(waiting)*waiting=1; return OS2_NO_ERROR; }
        dos_async_join(j);
        i=j->rc;
        if (b->trace) fprintf(stderr,"soft386: native DOSCALLS.%u async returned rc=%u\n",ordinal,i);
        free(j); b->async_jobs[slot]=NULL; return i;
    }
    if (d->nargs>6u || !m->valid(m->opaque,esp+4u,d->nargs*4u,0)) return OS2_ERROR_INVALID_PARAMETER;
    vp=proc(b,ordinal); if(!vp)return missing(b,ordinal);
    j=(struct Soft386DosAsyncJob *)calloc(1,sizeof(*j)); if(!j)return OS2_ERROR_NOT_ENOUGH_MEMORY;
    j->ordinal=ordinal;j->esp=esp;j->tid=tid;j->nargs=d->nargs;j->fn=vp;
    for(i=0;i<d->nargs;++i)j->args[i]=m->read_u32(m->opaque,esp+4u+4u*i);
    if(!dos_async_start(j)){free(j);return OS2_ERROR_NOT_ENOUGH_MEMORY;}
    b->async_jobs[slot]=j;b->async_tids[slot]=tid;
    if(waiting)*waiting=1;
    if(b->trace)fprintf(stderr,"soft386: native DOSCALLS.%u %s [generic async scalar] owner TID=%u\n",ordinal,d->name,tid);
    return OS2_NO_ERROR;
}
