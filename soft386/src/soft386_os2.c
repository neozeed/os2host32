/*
 * soft386_os2.c - Tiny386-backed 32-bit OS/2 LE/LX process vessel.
 *
 * R0 deliberately follows the proven WHP V2 guest ABI: untouched 32-bit
 * LE/LX code, synthetic ordinal veneers, OUT 0xF0,EAX host calls, and one
 * virtual CPU with multiple saved guest thread contexts. Mixed-mode support
 * is restricted to reviewed adaptations; general 16-bit apps remain deferred.
 */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "i386.h"
#include "os2_personality.h"
#include "os2_doscalls_core.h"
#include "soft386_doscalls_bridge.h"
#include "soft386_system_bridge.h"
#include "soft386_pm_bridge.h"
#include "soft386_queue_bridge.h"
#include "soft386_net_bridge.h"

#if defined(_WIN64)
#error "Soft386 native personality bridges require a 32-bit i386/i686 Windows build; use RosBE/i386 or i686-w64-mingw32-gcc"
#endif
#include "os2_nls.h"
#include "os2_nls_api.h"
#ifdef _WIN32
#include "os2_nls_win32.h"
#endif

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#define HOST_WRITE _write
#define HOST_LSEEK _lseeki64
#define HOST_ENVIRON _environ
#else
#include <strings.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#define _stricmp strcasecmp
#define HOST_WRITE write
#define HOST_LSEEK lseek
#define HOST_ENVIRON environ
#endif

#define RAM_SIZE            0x04000000u
#define GUEST_GDT           0x00007000u
#define MAX_THREADS         32u
#define MAX_EVENT_SEMS      64u
#define MAX_MUTEXES         64u
#define EVENT_NAME_MAX      127u
#define SYNC_GENERATION_MAX 0x003fffffu
#define MUTEX_TAG           0x40000000u
#define SEM_INDEFINITE_WAIT 0xffffffffu
#define GUEST_GDT_BYTES     ((3u + MAX_THREADS) * 8u)
#define GUEST_STARTUP       0x00E00000u
#define GUEST_STARTUP_LIMIT 0x00E20000u
#define GUEST_INFO          0x00E20000u
#define GUEST_INFO_LIMIT    0x00E23000u
#define GUEST_STUB_BASE     0x000F0000u
#define GUEST_STUB_LIMIT    0x000F8000u
#define GUEST_ALLOC_BASE    0x01000000u
#define GUEST_ALLOC_LIMIT   0x03000000u
#define GUEST_MODULE_BASE   0x03000000u
#define GUEST_MODULE_LIMIT  0x03F00000u
#define HOSTCALL_PORT       0x00F0u
#define HC_DOSCALLS         0x01000000u
#define HC_RUNTIME          0x02000000u
#define HC_VIOCALLS         0x03000000u
#define HC_KBDCALLS         0x04000000u
#define HC_SESMGR           0x05000000u
#define HC_NLS              0x06000000u
#define HC_C386_CONSOLE     0x07000000u
#define HC_PMWIN            0x08000000u
#define HC_PMGPI            0x09000000u
#define HC_PMCTLS           0x0A000000u
#define HC_MSG              0x0B000000u
#define HC_PMSHAPI          0x0C000000u
#define HC_HELPMGR          0x0D000000u
#define HC_PMWP             0x0E000000u
#define HC_QUECALLS         0x0F000000u
#define HC_PM_OLDPROC       0x10000000u
#define HC_SO32DLL          0x11000000u
#define HC_TCP32DLL         0x12000000u
#define HC_RUNTIME_THREAD_RETURN 1u
#define HC_RUNTIME_CALLBACK_RETURN 2u
#define GUEST_THREAD_EXIT_STUB 0x000F7FF0u
#define GUEST_CALLBACK_RETURN_STUB 0x000F7FE0u

#define MAX_OBJECTS         32u
#define MAX_PAGES           65535u
#define MAX_IMPORTS         512u
#define MAX_NAME_IMPORTS    512u
#define MAX_MODULES         32u
#define MAX_ALLOCS          256u

#define SRC_MASK            0x0f
#define SRC_SEL16           2u
#define SRC_PTR16           3u
#define SRC_PTR32           6u
#define SRC_OFF32           0x07
#define SRC_REL32           0x08
#define SRC_LIST            0x20
#define TGT_MASK            0x03
#define TGT_INTERNAL        0x00
#define TGT_EXT_ORD         0x01
#define TGT_EXT_NAME        0x02
#define TGT_INT_ENTRY       0x03
#define TGT_ADDITIVE        0x04
#define TGT_CHAIN           0x08
#define TGT_OFF32           0x10
#define TGT_ADD32           0x20
#define TGT_OBJ16           0x40
#define TGT_ORD8            0x80
#define OBJ_READ            0x0001u
#define OBJ_WRITE           0x0002u
#define OBJ_EXEC            0x0004u
#define OBJ_BIG             0x2000u
#define MOD_TYPE_MASK       0x00038000u
#define MOD_TYPE_DLL        0x00008000u

#define OS2_NO_ERROR                    0u
#define OS2_ERROR_INVALID_FUNCTION      1u
#define OS2_ERROR_INVALID_HANDLE        6u
#define OS2_ERROR_NOT_ENOUGH_MEMORY     8u
#define OS2_ERROR_INVALID_PARAMETER     87u
#define OS2_ERROR_TOO_MANY_SEM_REQUESTS 103u
#define OS2_ERROR_SEM_TIMEOUT           121u
#define OS2_ERROR_SEM_OWNER_DIED        105u
#define OS2_ERROR_SEM_NOT_FOUND         187u
#define OS2_ERROR_NOT_OWNER             288u
#define OS2_ERROR_DUPLICATE_NAME        285u
#define OS2_ERROR_TOO_MANY_OPENS        291u
#define OS2_ERROR_ALREADY_POSTED        299u
#define OS2_ERROR_ALREADY_RESET         300u
#define OS2_ERROR_SEM_BUSY              301u
#define OS2_ERROR_THREAD_NOT_TERMINATED 294u
#define OS2_ERROR_INVALID_THREADID      309u
#define EXIT_THREAD  0u
#define EXIT_PROCESS 1u
#define DCWW_WAIT   0u
#define DCWW_NOWAIT 1u
#define PAG_READ    0x00000001u
#define PAG_WRITE   0x00000002u
#define PAG_EXECUTE 0x00000004u
#define PAG_COMMIT  0x00000010u
#define PAG_FREE    0x00004000u
#define PAG_BASE    0x00010000u

struct LeObject {
    uint32_t size;
    uint32_t addr;          /* preferred address from LE object table */
    uint32_t mapped_addr;   /* actual guest linear address for this process */
    uint32_t flags;
    uint32_t mapidx;
    uint32_t mapsize;
    uint32_t reserved;
};

struct LePage {
    uint32_t physical;
    uint16_t flags;
    uint32_t data_offset;
    uint32_t data_size;
};

struct PhysOwner {
    int valid;
    uint32_t object;
    uint32_t object_page;
};

struct ImportModule {
    char name[32];
};

struct ImportOrd {
    uint32_t module;
    uint32_t ordinal;
    uint32_t address;
    uint32_t sites;
};

struct ImportName {
    uint32_t module;
    uint32_t name_offset;
    uint32_t address;
    uint32_t sites;
};

struct BridgeFix { uint32_t obj,off,first,target,type,kind,used; };
struct LeImage {
    struct BridgeFix bridge_fix[256];
    uint32_t nbridge_fix;
    int telnet_profile;
    uint8_t *file;
    uint32_t file_size;
    uint32_t le;
    int is_lx;
    uint32_t module_flags;
    uint32_t num_pages;
    uint32_t num_map_pages;
    uint32_t entry_object;
    uint32_t entry_offset;
    uint32_t stack_object;
    uint32_t stack_offset;
    uint32_t page_size;
    uint32_t last_page_size;
    uint32_t object_table_off;
    uint32_t num_objects;
    uint32_t object_map_off;
    uint32_t resident_name_off;
    uint32_t entry_table_off;
    uint32_t fixpage_off;
    uint32_t fixrec_off;
    uint32_t impmod_off;
    uint32_t num_impmods;
    uint32_t impproc_off;
    uint32_t data_pages_off;
    uint32_t iterated_pages_off;
    uint32_t nonresident_name_off;
    uint32_t nonresident_name_len;
    struct LeObject objects[MAX_OBJECTS];
    struct LePage *pages;
    struct PhysOwner *owner;
    struct ImportModule modules[MAX_MODULES];
    struct ImportOrd imports[MAX_IMPORTS];
    uint32_t nimports;
    struct ImportName name_imports[MAX_NAME_IMPORTS];
    uint32_t nname_imports;
    uint32_t internal_records;
    uint32_t internal_sites;
    uint32_t external_sites;
};


struct GuestAlloc { uint32_t base, size, flags; int used; };
struct GuestModule {
    struct LeImage image;
    char name[256], path[4096];
    uint32_t handle, refs, init_order;
    int state, pinned, term_called;
};
struct GuestCallback {
    struct GuestCallback *previous;
    uint32_t tid, entry_sp, argc, result;
    int done;
    unsigned cpu_trace_id, cpu_trace_steps, cpu_trace_limit;
};
enum GuestThreadState {
    THREAD_FREE=0, THREAD_RUNNABLE, THREAD_RUNNING, THREAD_WAIT_THREAD,
    THREAD_SLEEP, THREAD_WAIT_EVENT, THREAD_WAIT_MUTEX, THREAD_SUSPENDED, THREAD_DEAD,
    THREAD_HOSTCALL, THREAD_WAIT_PM, THREAD_WAIT_QUEUE
};
struct GuestThread {
    uint32_t tid; enum GuestThreadState state;
    uint32_t stack_base, stack_size, wait_tid, wait_ptid;
    uint32_t wait_event;
    uint64_t wait_deadline_ms, wait_order; int owns_stack;
    CPUI386_State regs; int regs_valid;
    uint32_t callback_depth;
    uint32_t priority_class; int32_t priority_delta;
};
struct GuestEventSem {
    int used; uint32_t generation, refs, post_count;
    char name[EVENT_NAME_MAX + 1];
};
struct GuestMutex {
    uint32_t generation, refs, owner, count;
    int used, abandoned;
    char name[EVENT_NAME_MAX + 1];
};
struct HostStub { uint32_t ordinal, address; };
struct Runtime {
    uint8_t *ram; CPUI386 *cpu; CPU_CB *cb;
    struct GuestAlloc allocs[MAX_ALLOCS]; uint32_t alloc_next;
    struct GuestThread threads[MAX_THREADS];
    struct GuestEventSem events[MAX_EVENT_SEMS];
    struct GuestMutex mutexes[MAX_MUTEXES];
    uint64_t wait_serial;
    struct HostStub stubs[MAX_IMPORTS];
    uint32_t module_next, stub_next, next_tid;
    int current_thread, switch_requested, current_thread_exited;
    int modal_pump_active, modal_owner; unsigned modal_budget;
    int process_exited, process_forced; uint32_t process_rc; const char *process_reason;
    int pending_hostcall; uint32_t pending_api;
    long max_cycles; int trace_hc, trace_sched, quiet;
    struct Soft386NativeDoscalls native_dos;
    struct Soft386SystemBridge native_sys;
    struct Soft386PmBridge pm;
    struct Soft386QueueBridge queue;
    struct Soft386NetBridge net;
    uint32_t net_arenas[MAX_THREADS][3], signal_focus[MAX_THREADS];
    struct {uint32_t entry,priority,serial;} exitlist[64];
    uint32_t exit_serial;int exit_processing;struct GuestCallback *exit_frame;
    struct GuestModule *modules[MAX_MODULES];
    struct LeImage *main_image;
    char main_path[4096];
    uint32_t init_serial;
    struct GuestCallback *callback_top;
    unsigned timer_trace_count;
    uint32_t watch_addr; unsigned watch_len;
    int retry_hostcall;
    struct Os2NlsState nls_state;
    int native_dos_required;
    int native_dos_disabled;
    const char *native_dos_path;
    const char *native_vio_path, *native_kbd_path, *native_ses_path;
    int native_vio_required, native_kbd_required, native_ses_required;
    int native_sys_disabled;
    const char *guest_argv0;
};

static int invoke_guest(void *, uint32_t, uint32_t, const uint32_t *, uint32_t, uint32_t *);
static int run_guest_until(struct Runtime *,struct GuestCallback *);
static int pm_modal_scheduler_yield(void *);
static int pm_thread_hostcall(void *,uint32_t);
static int initialize_modules(struct Runtime *);
static const char *loader_path;

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noreturn))
#endif
static void fatal(const char *what)
{
    if (loader_path) fprintf(stderr, "soft386: loading %s: %s\n", loader_path, what);
    else fprintf(stderr, "soft386: %s\n", what);
    exit(1);
}

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static int16_t rds16(const uint8_t *p)
{
    return (int16_t)rd16(p);
}

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xffu);
    p[1] = (uint8_t)((v >> 8) & 0xffu);
    p[2] = (uint8_t)((v >> 16) & 0xffu);
    p[3] = (uint8_t)((v >> 24) & 0xffu);
}

static uint32_t align_up(uint32_t v, uint32_t a)
{
    return (v + a - 1u) & ~(a - 1u);
}

static int file_range(const struct LeImage *x, uint32_t off, uint32_t cb)
{
    if (off > x->file_size)
        return 0;
    return cb <= x->file_size - off;
}

static uint8_t *load_file(const char *name, uint32_t *size_out)
{
    FILE *f;
    long n;
    uint8_t *p;

    f = fopen(name, "rb");
    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    n = ftell(f);
    if (n < 0 || (unsigned long)n > 0xffffffffUL) {
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    p = (uint8_t *)malloc((size_t)n);
    if (!p) {
        fclose(f);
        return NULL;
    }
    if (n != 0 && fread(p, 1, (size_t)n, f) != (size_t)n) {
        free(p);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *size_out = (uint32_t)n;
    return p;
}

static void parse_header(struct LeImage *x)
{
    uint32_t h;

    if (x->file_size < 0x40u)
        fatal("file is too small for MZ header");
    if (x->file[0] != 'M' || x->file[1] != 'Z')
        fatal("input is not MZ");

    x->le = rd32(x->file + 0x3c);
    if (!file_range(x, x->le, 0xc4u))
        fatal("LE header is outside file");
    if (x->file[x->le] != 'L' ||
        (x->file[x->le + 1] != 'E' && x->file[x->le + 1] != 'X')) {
        fprintf(stderr, "soft386: image size=%u header=%08X signature=%02X %02X %02X %02X\n",
                x->file_size, x->le, x->file[x->le], x->file[x->le+1], x->file[x->le+2], x->file[x->le+3]);
        if (!memcmp(x->file+x->le,"PE\0\0",4))
            fatal("native PE image cannot be loaded as a guest DLL/executable; a system service needs an explicit Soft386 bridge");
        fatal("input is not an LE/LX executable");
    }
    x->is_lx = x->file[x->le + 1] == 'X';
    if (x->file[x->le + 2] != 0 || x->file[x->le + 3] != 0)
        fatal("big-endian LE is unsupported");
    if (rd16(x->file + x->le + 8) != 2)
        fatal("only i386 LE images are supported");
    if (rd16(x->file + x->le + 0x0a) != 1)
        fatal("only OS/2 LE images are supported");

    h = x->le;
    x->module_flags     = rd32(x->file + h + 0x10);
    x->num_pages        = rd32(x->file + h + 0x14);
    x->entry_object     = rd32(x->file + h + 0x18);
    x->entry_offset     = rd32(x->file + h + 0x1c);
    x->stack_object     = rd32(x->file + h + 0x20);
    x->stack_offset     = rd32(x->file + h + 0x24);
    x->page_size        = rd32(x->file + h + 0x28);
    x->last_page_size   = rd32(x->file + h + 0x2c);
    if (x->is_lx && x->last_page_size > 31u)
        fatal("invalid LX page offset shift");
    x->object_table_off = h + rd32(x->file + h + 0x40);
    x->num_objects      = rd32(x->file + h + 0x44);
    x->object_map_off   = h + rd32(x->file + h + 0x48);
    x->resident_name_off= h + rd32(x->file + h + 0x58);
    x->entry_table_off  = h + rd32(x->file + h + 0x5c);
    x->fixpage_off      = h + rd32(x->file + h + 0x68);
    x->fixrec_off       = h + rd32(x->file + h + 0x6c);
    x->impmod_off       = h + rd32(x->file + h + 0x70);
    x->num_impmods      = rd32(x->file + h + 0x74);
    x->impproc_off      = h + rd32(x->file + h + 0x78);
    x->data_pages_off   = rd32(x->file + h + 0x80);
    x->iterated_pages_off = rd32(x->file + h + 0x4c);
    x->nonresident_name_off = rd32(x->file + h + 0x88);
    x->nonresident_name_len = rd32(x->file + h + 0x8c);

    if (x->num_objects == 0 || x->num_objects > MAX_OBJECTS)
        fatal("unsupported object count");
    if (x->num_pages == 0 || x->num_pages > MAX_PAGES)
        fatal("unsupported physical page count");
    if (x->page_size == 0 || (x->page_size & (x->page_size - 1u)) != 0)
        fatal("invalid LE page size");
    if (!file_range(x, x->object_table_off, x->num_objects * 24u))
        fatal("bad LE object table");
    if (!file_range(x, x->fixpage_off, (x->num_pages + 1u) * 4u))
        fatal("bad LE fixup page table");
    if ((x->module_flags & MOD_TYPE_MASK) != MOD_TYPE_DLL) {
        if (x->entry_object == 0 || x->entry_object > x->num_objects)
            fatal("bad LE entry object");
        if (x->stack_object == 0 || x->stack_object > x->num_objects)
            fatal("bad LE stack object");
    } else {
        if (x->entry_object > x->num_objects)
            fatal("bad DLL LE entry object");
        if (x->stack_object > x->num_objects)
            fatal("bad DLL LE stack object");
    }
}

/* Same bounded LX record format as the native loader. The section offset is
 * file-relative, and each map offset is shifted by the LX page shift. */
static void expand_iterated_page(struct LeImage *x, uint8_t *dst, uint32_t room,
                                 uint32_t src, uint32_t encoded)
{
    uint32_t in=0,out=0,repeats,length,i;
    if (!file_range(x,src,encoded)) fatal("LX iterated data extends past EOF");
    while (out<room) {
        if (encoded-in<4) fatal("truncated LX iterated record");
        repeats=rd16(x->file+src+in);length=rd16(x->file+src+in+2);in+=4;
        if (!repeats||!length||length>encoded-in||repeats>(room-out)/length)
            fatal("invalid LX iterated expansion");
        for(i=0;i<repeats;i++){memcpy(dst+out,x->file+src+in,length);out+=length;}
        in+=length;
    }
    if(in!=encoded)fatal("trailing LX iterated data");
}

 #include "soft386_telnet_match.h"

static void parse_objects(struct LeImage *x, struct Runtime *rt, int is_main)
{
    uint32_t i, j, off, phys, src, room, actual, last_map;
    uint32_t min_addr, max_end, span, module_base, resource_next;
    uint8_t *m;
    uint16_t pflags;

    x->telnet_profile = is_main && tp_match(x);
    last_map = 0;
    min_addr = 0xffffffffu;
    max_end = 0;
    for (i = 0; i < x->num_objects; ++i) {
        struct LeObject *o;
        o = &x->objects[i];
        off = x->object_table_off + i * 24u;
        o->size     = rd32(x->file + off + 0);
        o->addr     = rd32(x->file + off + 4);
        o->mapped_addr = 0;
        o->flags    = rd32(x->file + off + 8);
        o->mapidx   = rd32(x->file + off + 12);
        o->mapsize  = rd32(x->file + off + 16);
        o->reserved = rd32(x->file + off + 20);

        if (!x->telnet_profile && !(o->flags & (OBJ_BIG | 8u)) && (!(o->flags & OBJ_EXEC) || o->size > 65536u))
            fatal("only bounded C/386 migration code objects are supported");
        if (o->size != 0) {
            if (o->addr > 0xffffffffu - o->size)
                fatal("LE object preferred range overflows");
            if (o->addr < min_addr)
                min_addr = o->addr;
            if (o->addr + o->size > max_end)
                max_end = o->addr + o->size;
        }
        if (o->mapsize != 0) {
            if (o->mapidx == 0 || o->mapidx > MAX_PAGES ||
                o->mapsize > MAX_PAGES - (o->mapidx - 1u) ||
                o->mapsize > (o->size / x->page_size + (o->size % x->page_size != 0)))
                fatal("object has invalid page-map index");
            if (o->mapidx - 1u + o->mapsize > last_map)
                last_map = o->mapidx - 1u + o->mapsize;
        }
    }

    if (is_main) {
        if (!(x->objects[x->entry_object-1].flags & OBJ_BIG) ||
            !(x->objects[x->stack_object-1].flags & OBJ_BIG))
            fatal("16-bit entry/stack unsupported");
        if (x->entry_offset >= x->objects[x->entry_object - 1u].size ||
            x->stack_offset < 20u ||
            x->stack_offset > x->objects[x->stack_object - 1u].size)
            fatal("entry/stack offset lies outside its object");
    }

    if (min_addr == 0xffffffffu)
        min_addr = 0;

    if (is_main) {
        for (i = 0; i < x->num_objects; ++i) {
            struct LeObject *o = &x->objects[i];
            o->mapped_addr = o->addr;
            /* Resource objects can have zero/overlapping preferred addresses.
             * Their immutable host copies are registered separately. */
            if (o->flags & 8u) {
                uint32_t bytes=align_up(o->size,0x1000u);
                if (bytes<o->size || rt->module_next>GUEST_MODULE_LIMIT || bytes>GUEST_MODULE_LIMIT-rt->module_next)
                    fatal("resource arena exhausted");
                o->mapped_addr=rt->module_next; rt->module_next+=bytes;
            }
            if (o->size != 0 &&
                (o->mapped_addr >= RAM_SIZE || o->size > RAM_SIZE - o->mapped_addr))
                fatal("main LE/LX object lies outside guest RAM");
            if (o->size && !(o->flags & 8u) &&
                ((o->addr < GUEST_GDT + GUEST_GDT_BYTES && o->addr + o->size > GUEST_GDT) ||
                 (o->addr < GUEST_INFO_LIMIT && o->addr + o->size > GUEST_STARTUP) ||
                 (o->addr < GUEST_STUB_LIMIT && o->addr + o->size > GUEST_STUB_BASE) ||
                 o->addr + o->size > GUEST_ALLOC_BASE))
                fatal("main object overlaps reserved V2 runtime memory");
            for (j = 0; j < i; ++j) {
                struct LeObject *prev = &x->objects[j];
                if (o->size && prev->size && o->mapped_addr < prev->mapped_addr + prev->size &&
                    prev->mapped_addr < o->mapped_addr + o->size)
                    fatal("overlapping main objects");
            }
        }
    } else {
        if (max_end-min_addr>GUEST_MODULE_LIMIT-GUEST_MODULE_BASE) fatal("DLL object span too large");
        span = align_up(max_end - min_addr, 0x10000u);
        module_base = align_up(rt->module_next, 0x10000u);
        if (span == 0)
            span = 0x10000u;
        if (module_base >= GUEST_MODULE_LIMIT || span > GUEST_MODULE_LIMIT - module_base)
            fatal("guest DLL arena exhausted");
        resource_next=module_base+span;
        for (i = 0; i < x->num_objects; ++i) {
            struct LeObject *o = &x->objects[i];
            o->mapped_addr = module_base + (o->addr - min_addr);
            /* Resource-only DLLs commonly give every object preferred VA 0.
             * Each still needs its own guest bytes and resource offsets. */
            if(o->flags&8u){uint32_t bytes=align_up(o->size,0x1000u);
                if(bytes<o->size||resource_next>GUEST_MODULE_LIMIT||bytes>GUEST_MODULE_LIMIT-resource_next)
                    fatal("DLL resource arena exhausted");
                o->mapped_addr=resource_next;resource_next+=bytes;
            }
            if (o->size != 0 &&
                (o->mapped_addr >= RAM_SIZE || o->size > RAM_SIZE - o->mapped_addr))
                fatal("guest DLL object lies outside guest RAM");
            for(j=0;j<i;j++){struct LeObject *p=&x->objects[j];
                if(o->size&&p->size&&o->mapped_addr<p->mapped_addr+p->size&&p->mapped_addr<o->mapped_addr+o->size)
                    fatal("overlapping DLL objects");
            }
        }
        rt->module_next = resource_next;
    }

    if (last_map == 0 || last_map > MAX_PAGES)
        fatal("unsupported logical page-map size");
    if (x->is_lx && last_map != x->num_pages)
        fatal("LX object map does not cover module pages");
    x->num_map_pages = last_map;
    if (!file_range(x, x->object_map_off, x->num_map_pages * (x->is_lx ? 8u : 4u)))
        fatal("bad LE object page map");

    x->pages = (struct LePage *)calloc((size_t)x->num_map_pages, sizeof(*x->pages));
    x->owner = (struct PhysOwner *)calloc((size_t)x->num_pages + 1u, sizeof(*x->owner));
    if (!x->pages || !x->owner)
        fatal("out of memory parsing LE pages");

    for (i = 0; i < x->num_map_pages; ++i) {
        m = x->file + x->object_map_off + i * (x->is_lx ? 8u : 4u);
        if (x->is_lx) {
            uint64_t offset;
            phys = i + 1u; /* LX fixups index logical module pages. */
            pflags = rd16(m + 6);
            x->pages[i].data_size = rd16(m + 4);
            if (pflags==0 && x->pages[i].data_size > x->page_size)
                fatal("LX page data exceeds page size");
            if ((pflags == 0 || pflags == 1) && x->pages[i].data_size != 0) {
                if(pflags==1&&!x->iterated_pages_off)fatal("LX iterated page has no data section");
                offset = (uint64_t)(pflags==1?x->iterated_pages_off:x->data_pages_off) +
                         ((uint64_t)rd32(m) << x->last_page_size);
                if (offset > 0xffffffffu ||
                    !file_range(x, (uint32_t)offset, x->pages[i].data_size))
                    fatal("LX page data extends past EOF");
                x->pages[i].data_offset = (uint32_t)offset;
            }
        } else {
            phys = ((uint32_t)m[0] << 16) | ((uint32_t)m[1] << 8) | (uint32_t)m[2];
            pflags = m[3];
        }
        x->pages[i].physical = phys;
        x->pages[i].flags = pflags;
        if (pflags == 0) {
            if (phys == 0 || phys > x->num_pages)
                fatal("bad LE/LX page number");
        } else if (pflags != 3 && !(x->is_lx&&pflags==1)) {
            fprintf(stderr,"soft386: %s page %u has unsupported encoding %u\n",x->is_lx?"LX":"LE",i+1,pflags);
            fatal("unsupported page encoding (normal, zero and LX iterated pages are supported)");
        }
    }

    for (i = 0; i < x->num_objects; ++i) {
        struct LeObject *o;
        o = &x->objects[i];
        if (o->size)
            memset(rt->ram + o->mapped_addr, 0, o->size);

        for (j = 0; j < o->mapsize; ++j) {
            struct LePage *lp;
            lp = &x->pages[o->mapidx - 1u + j];
            if (!x->is_lx && lp->flags == 3)
                continue;

            phys = lp->physical;
            if (x->owner[phys].valid)
                fatal("physical LE page has multiple owners");
            x->owner[phys].valid = 1;
            x->owner[phys].object = i;
            x->owner[phys].object_page = j;

            if (lp->flags == 3)
                continue; /* LX zero pages still own fixup records. */
            src = x->is_lx ? lp->data_offset :
                  x->data_pages_off + (phys - 1u) * x->page_size;
            room = x->page_size;
            if (j * x->page_size >= o->size)
                room = 0;
            else if (room > o->size - j * x->page_size)
                room = o->size - j * x->page_size;
            if(x->is_lx&&lp->flags==1){
                expand_iterated_page(x,rt->ram+o->mapped_addr+j*x->page_size,room,lp->data_offset,lp->data_size);
                continue;
            }
            actual = room;
            if (x->is_lx && actual > lp->data_size)
                actual = lp->data_size;
            if (!x->is_lx && phys == x->num_pages && actual > x->last_page_size)
                actual = x->last_page_size;
            if (!file_range(x, src, actual))
                fatal("LE page data extends past EOF");
            if (actual)
                memcpy(rt->ram + o->mapped_addr + j * x->page_size,
                       x->file + src, actual);
        }
    }
}

static void parse_import_modules(struct LeImage *x)
{
    uint32_t i, p;
    uint8_t n;

    if (x->num_impmods > MAX_MODULES)
        fatal("unsupported import-module count");
    if (x->num_impmods == 0)
        return;
    p = x->impmod_off;
    for (i = 0; i < x->num_impmods; ++i) {
        if (!file_range(x, p, 1))
            fatal("bad import module table");
        n = x->file[p++];
        if (n == 0 || n >= sizeof(x->modules[i].name) || !file_range(x, p, n))
            fatal("bad import module name");
        memcpy(x->modules[i].name, x->file + p, n);
        x->modules[i].name[n] = 0;
        p += n;
    }
}

static uint32_t skip_objmod(const struct LeImage *x, uint32_t *pos,
                            uint8_t flags, uint32_t end)
{
    uint32_t v;
    if (flags & TGT_OBJ16) {
        if (*pos > end || end - *pos < 2)
            fatal("truncated LE target object/module");
        v = rd16(x->file + *pos);
        *pos += 2;
    } else {
        if (*pos >= end)
            fatal("truncated LE target object/module");
        v = x->file[(*pos)++];
    }
    return v;
}

static uint32_t source_object_offset(struct LeImage *x, uint32_t physical,
                                     int16_t source, uint32_t *obj_out)
{
    struct PhysOwner *po;
    struct LeObject *o;
    int32_t v;

    if (physical == 0 || physical > x->num_pages)
        fatal("fixup references invalid physical page");
    po = &x->owner[physical];
    if (!po->valid)
        fatal("fixup physical page has no object owner");
    o = &x->objects[po->object];
    v = (int32_t)(po->object_page * x->page_size) + (int32_t)source;
    if (v < 0 || (uint32_t)v + 4u > o->size)
        fatal("fixup source lies outside object");
    *obj_out = po->object;
    return (uint32_t)v;
}

static uint32_t find_or_add_import(struct LeImage *x, uint32_t module,
                                   uint32_t ordinal)
{
    uint32_t i;
    if (module >= x->num_impmods)
        fatal("external fixup references invalid module");
    for (i = 0; i < x->nimports; ++i) {
        if (x->imports[i].module == module && x->imports[i].ordinal == ordinal)
            return i;
    }
    if (x->nimports >= MAX_IMPORTS)
        fatal("too many imported ordinals");
    x->imports[x->nimports].module = module;
    x->imports[x->nimports].ordinal = ordinal;
    x->imports[x->nimports].address = 0;
    x->imports[x->nimports].sites = 0;
    return x->nimports++;
}

static uint32_t import_index(struct LeImage *x, uint32_t module, uint32_t ordinal)
{
    uint32_t i;
    for (i = 0; i < x->nimports; ++i) {
        if (x->imports[i].module == module && x->imports[i].ordinal == ordinal)
            return i;
    }
    fatal("internal error: missing import");
}

static uint32_t find_or_add_name_import(struct LeImage *x, uint32_t module,
                                        uint32_t name_offset)
{
    uint32_t i;
    if (module >= x->num_impmods)
        fatal("external named fixup references invalid module");
    for (i = 0; i < x->nname_imports; ++i) {
        if (x->name_imports[i].module == module &&
            x->name_imports[i].name_offset == name_offset)
            return i;
    }
    if (x->nname_imports >= MAX_NAME_IMPORTS)
        fatal("too many named imports");
    x->name_imports[x->nname_imports].module = module;
    x->name_imports[x->nname_imports].name_offset = name_offset;
    x->name_imports[x->nname_imports].address = 0;
    x->name_imports[x->nname_imports].sites = 0;
    return x->nname_imports++;
}

static uint32_t name_import_index(struct LeImage *x, uint32_t module,
                                  uint32_t name_offset)
{
    uint32_t i;
    for (i = 0; i < x->nname_imports; ++i) {
        if (x->name_imports[i].module == module &&
            x->name_imports[i].name_offset == name_offset)
            return i;
    }
    fatal("internal error: missing named import");
}

static void scan_fixups(struct LeImage *x)
{
    uint32_t physical, start, endoff, p, end, q, first, target;
    uint32_t count, i, idx, dummy_obj;
    uint8_t type, flags, kind, st;

    for (physical = 1; physical <= x->num_pages; ++physical) {
        start = rd32(x->file + x->fixpage_off + (physical - 1u) * 4u);
        endoff = rd32(x->file + x->fixpage_off + physical * 4u);
        p = x->fixrec_off + start;
        end = x->fixrec_off + endoff;
        if (p > end || end > x->impmod_off)
            fatal("bad LE fixup-record range");

        while (p < end) {
            if (end - p < 2)
                fatal("truncated LE fixup record");
            type = x->file[p++];
            flags = x->file[p++];
            st = type & SRC_MASK;
            kind = flags & TGT_MASK;
            if (st != SRC_OFF32 && st != SRC_REL32 && st != SRC_SEL16 && st != SRC_PTR16 && st != SRC_PTR32)
                fatal("V2 DLL milestone supports OFF32/REL32 fixups only");
            if ((type&0x10) && st!=SRC_SEL16 && st!=SRC_PTR16) fatal("unsupported alias fixup");
            if (flags & TGT_CHAIN)
                fatal("chained LE fixups are unsupported");

            if (type & SRC_LIST) {
                if (p >= end)
                    fatal("truncated LE source-list count");
                count = x->file[p++];
            } else {
                count = 1;
                if (end - p < 2)
                    fatal("truncated LE fixup source");
                (void)source_object_offset(x, physical, rds16(x->file + p), &dummy_obj);
                p += 2;
            }

            q = p;
            first = skip_objmod(x, &q, flags, end);
            target = 0;

            if (kind == TGT_INTERNAL) {
                if (st != SRC_OFF32 && st != SRC_REL32 && st != SRC_SEL16 && st != SRC_PTR32)
                    fatal("unsupported internal fixup kind");
                if (first == 0 || first > x->num_objects)
                    fatal("internal fixup has invalid object");
                if (st == SRC_SEL16) {
                    target = 0;
                } else if (flags & TGT_OFF32) {
                    if (end - q < 4) fatal("truncated internal target");
                    target = rd32(x->file+q); q += 4;
                } else {
                    if (end - q < 2) fatal("truncated internal target");
                    target = rd16(x->file+q); q += 2;
                }
                if (flags & TGT_ADDITIVE)
                    fatal("additive internal fixup unsupported");
                ++x->internal_records;
                x->internal_sites += count;
            } else if (kind == TGT_EXT_ORD) {
                if (st != SRC_REL32 && st != SRC_OFF32 && st != SRC_PTR16)
                    fatal("unsupported ordinal fixup kind");
                if (st == SRC_PTR16 && (flags & TGT_ADDITIVE)) fatal("additive far16 import unsupported");
                if (first == 0 || first > x->num_impmods)
                    fatal("external fixup has invalid module");
                if (flags & TGT_ORD8) {
                    if (q >= end) fatal("truncated ordinal");
                    target = x->file[q++];
                } else if (flags & TGT_OFF32) {
                    if (end - q < 4) fatal("truncated ordinal");
                    target = rd32(x->file + q); q += 4;
                } else {
                    if (end - q < 2) fatal("truncated ordinal");
                    target = rd16(x->file + q); q += 2;
                }
                if (flags & TGT_ADDITIVE) {
                    uint32_t n;
                    n = (flags & TGT_ADD32) ? 4u : 2u;
                    if (end - q < n) fatal("truncated additive field");
                    q += n;
                }
                idx = find_or_add_import(x, first - 1u, target);
                x->imports[idx].sites += count;
                x->external_sites += count;
            } else if (kind == TGT_EXT_NAME) {
                if (st != SRC_REL32 && st != SRC_OFF32)
                    fatal("external named fixup is not OFF32/REL32");
                if (first == 0 || first > x->num_impmods)
                    fatal("external named fixup has invalid module");
                if (flags & TGT_OFF32) {
                    if (end - q < 4) fatal("truncated name offset");
                    target = rd32(x->file + q); q += 4;
                } else {
                    if (end - q < 2) fatal("truncated name offset");
                    target = rd16(x->file + q); q += 2;
                }
                if (flags & TGT_ADDITIVE) {
                    uint32_t n;
                    n = (flags & TGT_ADD32) ? 4u : 2u;
                    if (end - q < n) fatal("truncated additive field");
                    q += n;
                }
                idx = find_or_add_name_import(x, first - 1u, target);
                x->name_imports[idx].sites += count;
                x->external_sites += count;
            } else if (kind == TGT_INT_ENTRY) {
                fatal("entry-table fixup is unsupported in V2 DLL milestone");
            } else {
                fatal("unknown LE target kind");
            }

            if (st == SRC_SEL16 || st == SRC_PTR16 || st == SRC_PTR32) {
                uint32_t j,at;
                int16_t source;
                for(j=0;j<count;j++) {
                    struct BridgeFix *b;
                    if(x->nbridge_fix==256)fatal("too many migration fixups");
                    at=(type&SRC_LIST)?q+j*2u:start; /* non-list source is immediately after header */
                    if(type&SRC_LIST) { if(at+2u>end)fatal("truncated migration list"); }
                    source=rds16(x->file+((type&SRC_LIST)?at:p-2u));
                    b=&x->bridge_fix[x->nbridge_fix++];
                    b->off=source_object_offset(x,physical,source,&b->obj);
                    if(st==SRC_PTR32 && (x->objects[b->obj].size<6 || b->off>x->objects[b->obj].size-6))fatal("far32 source overflow");
                    b->first=first;b->target=target;b->type=type;b->kind=kind;b->used=0;
                }
            }
            p = q;
            if (type & SRC_LIST) {
                if (p > end || count * 2u > end - p)
                    fatal("fixup source list extends past record area");
                for (i = 0; i < count; ++i) {
                    (void)source_object_offset(x, physical, rds16(x->file + p), &dummy_obj);
                    p += 2;
                }
            }
            if (p > end)
                fatal("fixup record extends past record area");
        }
        if (p != end)
            fatal("fixup page did not end on record boundary");
    }
}

static void apply_fixups(struct LeImage *x, uint8_t *ram)
{
    uint32_t physical, start, endoff, p, end, q, first, target, target_off;
    uint32_t count, i, src_obj_index, source_off, src_va, dst_va, idx, addend;
    uint8_t type, flags, kind;
    int16_t source;

    for (physical = 1; physical <= x->num_pages; ++physical) {
        start = rd32(x->file + x->fixpage_off + (physical - 1u) * 4u);
        endoff = rd32(x->file + x->fixpage_off + physical * 4u);
        p = x->fixrec_off + start;
        end = x->fixrec_off + endoff;

        while (p < end) {
            type = x->file[p++];
            flags = x->file[p++];
            if (type & SRC_LIST) {
                count = x->file[p++];
                source = 0;
            } else {
                count = 1;
                source = rds16(x->file + p);
                p += 2;
            }

            q = p;
            first = skip_objmod(x, &q, flags, end);
            kind = flags & TGT_MASK;
            target = 0;
            target_off = 0; addend = 0;

            if (kind == TGT_INTERNAL) {
                target = first;
                if ((type&SRC_MASK)==SRC_SEL16) {
                    target_off = 0;
                } else if (flags & TGT_OFF32) {
                    target_off = rd32(x->file + q); q += 4;
                } else {
                    target_off = rd16(x->file + q); q += 2;
                }
            } else if (kind == TGT_EXT_ORD) {
                if (flags & TGT_ORD8) {
                    target = x->file[q++];
                } else if (flags & TGT_OFF32) {
                    target = rd32(x->file + q); q += 4;
                } else {
                    target = rd16(x->file + q); q += 2;
                }
                if (flags & TGT_ADDITIVE) {
                    addend=(flags&TGT_ADD32)?rd32(x->file+q):rd16(x->file+q);
                    q += (flags & TGT_ADD32) ? 4u : 2u;
                }
            } else if (kind == TGT_EXT_NAME) {
                if (flags & TGT_OFF32) {
                    target = rd32(x->file + q); q += 4;
                } else {
                    target = rd16(x->file + q); q += 2;
                }
                if (flags & TGT_ADDITIVE) {
                    addend=(flags&TGT_ADD32)?rd32(x->file+q):rd16(x->file+q);
                    q += (flags & TGT_ADD32) ? 4u : 2u;
                }
            } else {
                fatal("unsupported fixup reached apply pass");
            }

            p = q;
            for (i = 0; i < count; ++i) {
                if (type & SRC_LIST) {
                    source = rds16(x->file + p);
                    p += 2;
                }
                source_off = source_object_offset(x, physical, source, &src_obj_index);
                src_va = x->objects[src_obj_index].mapped_addr + source_off;

                if ((type&SRC_MASK)==SRC_SEL16 || (type&SRC_MASK)==SRC_PTR16 || (type&SRC_MASK)==SRC_PTR32) continue;
                if (kind == TGT_INTERNAL) {
                    /* Preserve LINK386's 32-bit biased-offset arithmetic. */
                    dst_va = x->objects[target - 1u].mapped_addr + target_off;
                    wr32(ram + src_va, dst_va - ((type&SRC_MASK)==SRC_REL32 ? src_va+4u : 0));
                } else if (kind == TGT_EXT_ORD) {
                    idx = import_index(x, first - 1u, target);
                    dst_va = x->imports[idx].address;
                    if (dst_va == 0)
                        fatal("unresolved ordinal import reached fixup pass");
                    wr32(ram + src_va, dst_va + addend - ((type&SRC_MASK)==SRC_REL32 ? src_va+4u : 0));
                } else {
                    idx = name_import_index(x, first - 1u, target);
                    dst_va = x->name_imports[idx].address;
                    if (dst_va == 0)
                        fatal("unresolved named import reached fixup pass");
                    wr32(ram + src_va, dst_va + addend - ((type&SRC_MASK)==SRC_REL32 ? src_va+4u : 0));
                }
            }
        }
    }
}

static void free_le_image(struct LeImage *x)
{
    free(x->pages);
    free(x->owner);
    free(x->file);
    x->pages = NULL;
    x->owner = NULL;
    x->file = NULL;
}

static const char *base_name(const char *p)
{
    const char *last;
    last = p;
    while (*p) {
        if (*p == '\\' || *p == '/')
            last = p + 1;
        ++p;
    }
    return last;
}

static int arg_needs_quotes(const char *s)
{
    if (!*s)
        return 1;
    while (*s) {
        if (*s == ' ' || *s == '\t')
            return 1;
        ++s;
    }
    return 0;
}


#ifdef _WIN32
extern char **_environ;
#else
extern char **environ;
#endif

static int guest_range(uint32_t va, uint32_t cb)
{
    if (va >= RAM_SIZE) return 0;
    return cb <= RAM_SIZE - va;
}

static uint32_t guest_u32(struct Runtime *rt, uint32_t va)
{
    if (!guest_range(va, 4)) fatal("guest read outside RAM");
    return rd32(rt->ram + va);
}

static void guest_put_u32(struct Runtime *rt, uint32_t va, uint32_t v)
{
    if (!guest_range(va, 4)) fatal("guest write outside RAM");
    wr32(rt->ram + va, v);
}

static int bridge_valid(void *opaque, uint32_t va, uint32_t cb, int write_access)
{
    (void)opaque;
    (void)write_access;
    return guest_range(va, cb);
}

static int bridge_read(void *opaque, uint32_t va, void *dst, uint32_t cb)
{
    struct Runtime *rt = (struct Runtime *)opaque;
    if (!guest_range(va, cb) || (!dst && cb)) return 0;
    if (cb) memcpy(dst, rt->ram + va, cb);
    return 1;
}

static int bridge_write(void *opaque, uint32_t va, const void *src, uint32_t cb)
{
    struct Runtime *rt = (struct Runtime *)opaque;
    if (!guest_range(va, cb) || (!src && cb)) return 0;
    if (cb) memcpy(rt->ram + va, src, cb);
    return 1;
}

static int bridge_read_cstr(void *opaque, uint32_t va, char *dst, uint32_t capacity)
{
    struct Runtime *rt = (struct Runtime *)opaque;
    uint32_t i;
    if (!va || !dst || !capacity || va >= RAM_SIZE) return 0;
    for (i = 0; i < capacity; ++i) {
        if (!guest_range(va + i, 1)) return 0;
        dst[i] = (char)rt->ram[va + i];
        if (dst[i] == '\0') return 1;
    }
    dst[capacity - 1] = '\0';
    return 0;
}

static uint32_t bridge_read_u32(void *opaque, uint32_t va)
{
    struct Runtime *rt = (struct Runtime *)opaque;
    return guest_u32(rt, va);
}

static int guest_copy_cstr(struct Runtime *rt, uint32_t va, char *dst, uint32_t capacity)
{
    return bridge_read_cstr(rt, va, dst, capacity);
}

static void bridge_memory_ops(struct Runtime *rt, struct Soft386GuestMemoryOps *ops)
{
    memset(ops, 0, sizeof(*ops));
    ops->opaque = rt;
    ops->valid = bridge_valid;
    ops->read = bridge_read;
    ops->write = bridge_write;
    ops->read_cstr = bridge_read_cstr;
    ops->read_u32 = bridge_read_u32;
}

static uint64_t monotonic_ms(void)
{
#ifdef _WIN32
    /*
     * Keep the Win32 host requirement down at the original NT-era API level.
     * Some RosBE/older MinGW import libraries do not provide GetTickCount64.
     * Soft386 is single-host-threaded, so extending the 32-bit tick counter here
     * is sufficient and preserves a monotonic 64-bit scheduler clock across the
     * ~49.7 day GetTickCount wrap.
     */
    static DWORD last_tick;
    static uint64_t tick_epoch;
    static int tick_initialized;
    DWORD now = GetTickCount();

    if (!tick_initialized) {
        last_tick = now;
        tick_initialized = 1;
    } else {
        if (now < last_tick)
            tick_epoch += UINT64_C(0x100000000);
        last_tick = now;
    }
    return tick_epoch + (uint64_t)now;
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
#endif
}

static void sleep_ms(uint64_t ms)
{
#ifdef _WIN32
    /* 0xffffffff is INFINITE to Sleep(), so never use it as a finite chunk. */
    while (ms > 0xfffffffeu) {
        Sleep(0xfffffffeu);
        ms -= 0xfffffffeu;
    }
    Sleep((DWORD)ms);
#else
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000u);
    ts.tv_nsec = (long)((ms % 1000u) * 1000000u);
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR) { }
#endif
}

static uint32_t alloc_guest(struct Runtime *rt, uint32_t size)
{
    uint32_t base, need, i;
    if (size == 0 || size > GUEST_ALLOC_LIMIT - GUEST_ALLOC_BASE) return 0;
    need = align_up(size, 0x1000u);
    for (i = 0; i < MAX_ALLOCS; ++i) {
        if (!rt->allocs[i].used && rt->allocs[i].base != 0 && rt->allocs[i].size >= need) {
            rt->allocs[i].used = 1;
            rt->allocs[i].flags = PAG_READ | PAG_WRITE | PAG_COMMIT;
            memset(rt->ram + rt->allocs[i].base, 0, need);
            return rt->allocs[i].base;
        }
    }
    base = align_up(rt->alloc_next, 0x1000u);
    if (base >= GUEST_ALLOC_LIMIT || need > GUEST_ALLOC_LIMIT - base) return 0;
    for (i = 0; i < MAX_ALLOCS; ++i) {
        if (rt->allocs[i].base == 0) {
            rt->allocs[i].base = base;
            rt->allocs[i].size = need;
            rt->allocs[i].used = 1;
            rt->allocs[i].flags = PAG_READ | PAG_WRITE | PAG_COMMIT;
            rt->alloc_next = base + need;
            memset(rt->ram + base, 0, need);
            return base;
        }
    }
    return 0;
}

static struct GuestAlloc *find_alloc(struct Runtime *rt, uint32_t base)
{
    uint32_t i;
    for (i = 0; i < MAX_ALLOCS; ++i)
        if (rt->allocs[i].base == base) return &rt->allocs[i];
    return NULL;
}

static struct GuestAlloc *find_alloc_containing(struct Runtime *rt, uint32_t address)
{
    uint32_t i;
    for (i = 0; i < MAX_ALLOCS; ++i) {
        struct GuestAlloc *ga = &rt->allocs[i];
        if (ga->used && address >= ga->base && address - ga->base < ga->size) return ga;
    }
    return NULL;
}

static os2_api_ret_t soft_validate_memory(void *opaque, os2_addr32_t address,
                                           uint32_t length, uint32_t access)
{
    (void)opaque; (void)access;
    if (length && !address) return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    return guest_range(address, length) ? OS2_PERSONALITY_NO_ERROR :
                                          OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
}

static os2_api_ret_t soft_write_memory(void *opaque, os2_addr32_t address,
                                        const void *source, uint32_t length)
{
    struct Runtime *rt = (struct Runtime *)opaque;
    if (!guest_range(address, length) || (length && !source))
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    if (length) memcpy(rt->ram + address, source, length);
    return OS2_PERSONALITY_NO_ERROR;
}

static os2_api_ret_t soft_map_read_memory(void *opaque, os2_addr32_t address,
                                           uint32_t length, const void **host_pointer)
{
    struct Runtime *rt = (struct Runtime *)opaque;
    if (!host_pointer || !guest_range(address, length))
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    *host_pointer = rt->ram + address;
    return OS2_PERSONALITY_NO_ERROR;
}

static os2_api_ret_t soft_query_handle_type(void *opaque, os2_handle32_t handle,
                                             uint32_t *type, uint32_t *attributes)
{
    (void)opaque;
    if (!type || !attributes || handle > 2u) return OS2_PERSONALITY_ERROR_INVALID_HANDLE;
    *type = 1u; /* character device */
    *attributes = 0u;
    return OS2_PERSONALITY_NO_ERROR;
}

static os2_api_ret_t soft_write_handle(void *opaque, os2_handle32_t handle,
                                        const void *buffer, uint32_t length,
                                        uint32_t *actual)
{
    int n;
    int fd;
    unsigned int chunk;
    (void)opaque;
    if (!actual) return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    *actual = 0;
    if (handle > 2u) return OS2_PERSONALITY_ERROR_INVALID_HANDLE;
    fd = (int)handle;
    if (length == 0) return OS2_PERSONALITY_NO_ERROR;
    chunk = length > 0x7fffffffu ? 0x7fffffffu : (unsigned int)length;
    n = HOST_WRITE(fd, buffer, chunk);
    if (n < 0) return OS2_PERSONALITY_ERROR_INVALID_HANDLE;
    *actual = (uint32_t)n;
    return OS2_PERSONALITY_NO_ERROR;
}

static os2_api_ret_t soft_set_file_pointer(void *opaque, os2_handle32_t handle,
                                            os2_long32_t distance, uint32_t method,
                                            uint32_t *new_position)
{
#ifdef _WIN32
    __int64 p;
#else
    off_t p;
#endif
    int whence;
    (void)opaque;
    if (!new_position || handle > 2u) return OS2_PERSONALITY_ERROR_INVALID_HANDLE;
    whence = method == 0 ? SEEK_SET : (method == 1 ? SEEK_CUR : SEEK_END);
    p = HOST_LSEEK((int)handle, distance, whence);
    if (p < 0 || (uint64_t)p > 0xffffffffu) return OS2_PERSONALITY_ERROR_INVALID_HANDLE;
    *new_position = (uint32_t)p;
    return OS2_PERSONALITY_NO_ERROR;
}

static os2_api_ret_t soft_allocate_memory(void *opaque, uint32_t size,
                                           uint32_t flags, uint32_t reserved,
                                           os2_addr32_t *base)
{
    struct Runtime *rt = (struct Runtime *)opaque;
    struct GuestAlloc *ga;
    (void)reserved;
    if (!base) return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    *base = alloc_guest(rt, size);
    if (!*base) return OS2_PERSONALITY_ERROR_NOT_ENOUGH_MEMORY;
    ga = find_alloc(rt, *base);
    if (ga) ga->flags = (flags & (PAG_READ|PAG_WRITE|PAG_EXECUTE|PAG_COMMIT)) | PAG_COMMIT;
    return OS2_PERSONALITY_NO_ERROR;
}

static os2_api_ret_t soft_free_memory(void *opaque, os2_addr32_t base)
{
    struct Runtime *rt = (struct Runtime *)opaque;
    struct GuestAlloc *ga = find_alloc(rt, base);
    if (!ga || !ga->used) return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    ga->used = 0;
    return OS2_PERSONALITY_NO_ERROR;
}

static os2_api_ret_t soft_set_memory(void *opaque, os2_addr32_t base,
                                      uint32_t size, uint32_t flags)
{
    struct Runtime *rt = (struct Runtime *)opaque;
    struct GuestAlloc *ga;
    if (!guest_range(base, size)) return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    ga = find_alloc_containing(rt, base);
    if (ga && size <= ga->size - (base - ga->base))
        ga->flags = (ga->flags & ~(PAG_READ|PAG_WRITE|PAG_EXECUTE|PAG_COMMIT)) |
                    (flags & (PAG_READ|PAG_WRITE|PAG_EXECUTE|PAG_COMMIT));
    /* Tiny386 R1 has one flat RAM mapping; protection is OS/2-visible state. */
    return OS2_PERSONALITY_NO_ERROR;
}

static uint32_t guest_query_mem(struct Runtime *rt, uint32_t base, uint32_t pcb, uint32_t pflags)
{
    struct GuestAlloc *ga;
    uint32_t requested, available, flags;
    if (!base || !guest_range(pcb,4) || !guest_range(pflags,4)) return OS2_ERROR_INVALID_PARAMETER;
    requested = guest_u32(rt, pcb);
    if (!requested) return OS2_ERROR_INVALID_PARAMETER;
    ga = find_alloc_containing(rt, base);
    if (!ga) return OS2_ERROR_INVALID_PARAMETER;
    available = ga->size - (base - ga->base);
    if (available > requested) available = requested;
    flags = ga->flags | (base == ga->base ? PAG_BASE : 0u);
    guest_put_u32(rt, pcb, available);
    guest_put_u32(rt, pflags, flags);
    return OS2_NO_ERROR;
}

static os2_api_ret_t soft_query_local_datetime(void *opaque, struct Os2LocalDateTime *value)
{
    (void)opaque;
    if (!value) return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
#ifdef _WIN32
    {
        SYSTEMTIME st;
        TIME_ZONE_INFORMATION tzi;
        LONG bias;
        DWORD tzid;
        GetLocalTime(&st);
        tzid = GetTimeZoneInformation(&tzi);
        if (tzid == TIME_ZONE_ID_INVALID)
            return OS2_PERSONALITY_ERROR_INVALID_FUNCTION;
        bias = tzi.Bias;
        if (tzid == TIME_ZONE_ID_STANDARD) bias += tzi.StandardBias;
        else if (tzid == TIME_ZONE_ID_DAYLIGHT) bias += tzi.DaylightBias;
        value->hours = (uint8_t)st.wHour;
        value->minutes = (uint8_t)st.wMinute;
        value->seconds = (uint8_t)st.wSecond;
        value->hundredths = (uint8_t)(st.wMilliseconds / 10u);
        value->day = (uint8_t)st.wDay;
        value->month = (uint8_t)st.wMonth;
        value->year = (uint16_t)st.wYear;
        value->timezone_minutes_west = (int16_t)bias;
        value->weekday = (uint8_t)st.wDayOfWeek;
    }
#else
    {
        struct timeval tv;
        time_t now;
        struct tm lt, gt;
        time_t lsec, gsec;
        if (gettimeofday(&tv, NULL) != 0) return OS2_PERSONALITY_ERROR_INVALID_FUNCTION;
        now = tv.tv_sec;
        if (!localtime_r(&now, &lt) || !gmtime_r(&now, &gt))
            return OS2_PERSONALITY_ERROR_INVALID_FUNCTION;
        lsec = mktime(&lt);
        gsec = mktime(&gt);
        value->hours = (uint8_t)lt.tm_hour;
        value->minutes = (uint8_t)lt.tm_min;
        value->seconds = (uint8_t)lt.tm_sec;
        value->hundredths = (uint8_t)(tv.tv_usec / 10000);
        value->day = (uint8_t)lt.tm_mday;
        value->month = (uint8_t)(lt.tm_mon + 1);
        value->year = (uint16_t)(lt.tm_year + 1900);
        value->timezone_minutes_west = (int16_t)((gsec - lsec) / 60);
        value->weekday = (uint8_t)lt.tm_wday;
    }
#endif
    return OS2_PERSONALITY_NO_ERROR;
}

static uint64_t soft_monotonic_milliseconds(void *opaque)
{
    (void)opaque;
    return monotonic_ms();
}

static const struct Os2PersonalityOps soft_personality_ops = {
    soft_validate_memory, soft_write_memory, soft_map_read_memory,
    soft_query_handle_type, soft_write_handle, soft_set_file_pointer,
    soft_allocate_memory, soft_free_memory, soft_set_memory,
    soft_query_local_datetime, soft_monotonic_milliseconds
};

static uint32_t hostcall_stub(struct Runtime *rt, uint32_t module, uint32_t ordinal)
{
    uint32_t i, va, id;
    uint8_t *s;
    /* C/386 register-ABI token: preserve EAX and simply return. */
    if (module == HC_DOSCALLS && ordinal == 425u) {
        va = rt->stub_next++;
        if (va >= GUEST_CALLBACK_RETURN_STUB) fatal("stub overflow");
        rt->ram[va] = 0xC3;
        return va;
    }
    id = module | (ordinal & 0x00ffffffu);
    for (i = 0; i < MAX_IMPORTS; ++i)
        if (rt->stubs[i].address && rt->stubs[i].ordinal == id) return rt->stubs[i].address;
    for (i = 0; i < MAX_IMPORTS; ++i) if (!rt->stubs[i].address) break;
    if (i == MAX_IMPORTS) fatal("too many synthetic import veneers");
    va = align_up(rt->stub_next, 8u);
    if (va + 8u > GUEST_CALLBACK_RETURN_STUB) fatal("synthetic import veneer overflow");
    s = rt->ram + va;
    s[0] = 0xB8; wr32(s + 1, id); /* mov eax,imm32 */
    s[5] = 0xE7; s[6] = (uint8_t)HOSTCALL_PORT; /* out imm8,eax */
    s[7] = 0xC3; /* ret */
    rt->stubs[i].ordinal = id;
    rt->stubs[i].address = va;
    rt->stub_next = va + 8u;
    return va;
}

 #include "soft386_telnet_profile.h"

static void install_c386_helpers(struct LeImage *x, struct Runtime *rt)
{
    uint32_t i,j,k,n=0,covered[MAX_OBJECTS]={0};
    struct {uint32_t obj,off,desc;} helpers[128];
    static const uint8_t pro[]={0x55,0x8b,0xec,0x83,0xec,4,0x53,0x57,0x56,6};
    static const uint8_t lss[]={0x66,0x0f,0xb2,0x24,0x24};
    if(x->telnet_profile){install_telnet_profile(x,rt);return;}
    for(i=0;i<x->nbridge_fix;i++) {
        struct BridgeFix *e=&x->bridge_fix[i],*ret=NULL,*alias=NULL,*stack=NULL;
        uint32_t base,sel,lo,helper=0xffffffffu;uint8_t *code,*thunk;int d;
        if(e->kind!=TGT_EXT_ORD || (e->type&0x1f)!=(SRC_PTR16|0x10))continue;
        if(e->used || !e->off || n==128)fatal("invalid C/386 migration import");
        d=soft386_c386_desc_index(x->modules[e->first-1].name,e->target);
        if(d<0){
            fprintf(stderr,"soft386: unsupported C/386 helper %s.%u\n",x->modules[e->first-1].name,e->target);
            fatal("unknown C/386 migration API");
        }
        base=e->off-1;
        if((x->objects[e->obj].flags&OBJ_BIG)||x->objects[e->obj].size<14||base>x->objects[e->obj].size-14)fatal("invalid C/386 migration fragment");
        thunk=rt->ram+x->objects[e->obj].mapped_addr+base;
        if(thunk[0]!=0x9a||thunk[5]!=0x66||thunk[6]!=0x67||thunk[7]!=0xea)fatal("unrecognized C/386 migration fragment");
        for(j=0;j<x->nbridge_fix;j++) {
            struct BridgeFix *b=&x->bridge_fix[j];
            if(!b->used&&b->obj==e->obj&&b->off==base+8&&b->kind==TGT_INTERNAL&&(b->type&0x1f)==SRC_PTR32) {
                if(ret)fatal("ambiguous C/386 migration return");ret=b;
            }
        }
        if(!ret)fatal("missing C/386 migration return");
        for(j=0;j<x->nbridge_fix;j++) {
            struct BridgeFix *b=&x->bridge_fix[j];
            if(!b->used&&b->kind==TGT_INTERNAL&&(b->type&0x1f)==(SRC_SEL16|0x10)&&b->first==e->obj+1&&b->obj==ret->first-1&&b->off+2==ret->target) {
                if(alias)fatal("ambiguous C/386 migration alias");alias=b;
            }
        }
        if(!alias || !(x->objects[alias->obj].flags&OBJ_BIG))fatal("missing 32-bit C/386 migration helper");
        sel=alias->off;code=rt->ram+x->objects[alias->obj].mapped_addr;
        if(sel<9 || code[sel-4]!=0x66 || code[sel-3]!=0xea || rd16(code+sel-2)!=base || memcmp(code+sel-9,lss,5))fatal("invalid C/386 migration transition");
        lo=sel>192?sel-192:0;
        for(j=lo;j+sizeof(pro)<=sel;j++)if(!memcmp(code+j,pro,sizeof(pro)))helper=j;
        if(helper==0xffffffffu)fatal("missing C/386 migration prologue");
        for(j=0;j<x->nbridge_fix;j++) {
            struct BridgeFix *b=&x->bridge_fix[j];
            if(!b->used&&b->kind==TGT_INTERNAL&&(b->type&0x1f)==SRC_SEL16&&b->obj==alias->obj&&b->first==x->stack_object&&b->off>helper+1&&b->off<sel) {
                if(stack)fatal("ambiguous C/386 migration stack");stack=b;
            }
        }
        if(!stack || code[stack->off-2]!=0x66 || code[stack->off-1]!=0x3d)fatal("missing C/386 migration SS comparison");
        for(k=0;k<n;k++)if(helpers[k].obj==alias->obj&&helpers[k].off==helper)fatal("overlapping C/386 migration helpers");
        helpers[n].obj=alias->obj;helpers[n].off=helper;helpers[n++].desc=(uint32_t)d;
        covered[e->obj]+=14;e->used=ret->used=alias->used=stack->used=1;
    }
    for(i=0;i<x->nbridge_fix;i++)if(!x->bridge_fix[i].used)fatal("nonflat relocation outside recognized C/386 migration helpers");
    for(i=0;i<x->num_objects;i++)if(!(x->objects[i].flags&OBJ_BIG)&&covered[i]!=x->objects[i].size)fatal("16-bit object is not entirely C/386 migration fragments");
    for(i=0;i<n;i++) {
        uint32_t va=align_up(rt->stub_next,16),src=x->objects[helpers[i].obj].mapped_addr+helpers[i].off,bytes=0;
        uint8_t *p;const struct Soft386C386ApiDesc *d=soft386_c386_desc_by_id(helpers[i].desc+1u);
        if(!d)fatal("invalid C/386 descriptor");
        if(va+16>GUEST_STUB_LIMIT)fatal("C/386 veneer overflow");p=rt->ram+va;
        for(j=0;j<d->count;j++)bytes+=d->width[j];
        p[0]=0xb8;wr32(p+1,HC_C386_CONSOLE|(helpers[i].desc+1u));p[5]=0xe7;p[6]=(uint8_t)HOSTCALL_PORT;
        p[7]=0xc2;p[8]=(uint8_t)bytes;p[9]=(uint8_t)(bytes>>8);
        rt->ram[src]=0xe9;wr32(rt->ram+src+1,va-(src+5));rt->stub_next=va+16;
        if(!rt->quiet||rt->trace_hc)
            fprintf(stderr,"soft386: C/386 bridge %s.%u %s helper=%08X packed=%u -> %08X\n",d->module,d->ordinal,d->name,src,bytes,va);
    }
}

static uint32_t image_system_mask(const struct LeImage *x)
{
    uint32_t i,mask=0;
    if(!x)return 0;
    for(i=0;i<x->num_impmods && i<MAX_MODULES;i++){
        if(_stricmp(x->modules[i].name,"VIOCALLS")==0)mask|=SOFT386_SYS_VIO;
        else if(_stricmp(x->modules[i].name,"KBDCALLS")==0)mask|=SOFT386_SYS_KBD;
        else if(_stricmp(x->modules[i].name,"SESMGR")==0)mask|=SOFT386_SYS_SES;
    }
    return mask;
}

#include "soft386_modules.h"

static uint32_t env_block_size(void)
{
    uint32_t n = 1;
    char **p;
    for (p = HOST_ENVIRON; p && *p; ++p) n += (uint32_t)strlen(*p) + 1u;
    return n;
}

static void build_startup_area(uint8_t *ram, const char *program, const char *arg0_override,
                               int argc, char **argv,
                               uint32_t *env_va, uint32_t *arg_va, uint32_t *pgm_va)
{
    uint32_t env_len = env_block_size(), pos = 0, i, n, total, pgm_len, arg0_len, tail_len = 0;
    const char *arg0 = (arg0_override && arg0_override[0]) ? arg0_override : base_name(program);
    const char *pgm = program;
    char **ep;
    pgm_len = (uint32_t)strlen(pgm);
    arg0_len = (uint32_t)strlen(arg0);
    for (i = 2; i < (uint32_t)argc; ++i) {
        if (i != 2u) ++tail_len;
        tail_len += (uint32_t)strlen(argv[i]) + (arg_needs_quotes(argv[i]) ? 2u : 0u);
    }
    total = env_len + pgm_len + 1u + arg0_len + 1u + tail_len + 2u;
    if (GUEST_STARTUP + total > GUEST_STARTUP_LIMIT) fatal("startup strings exceed reserved guest area");
    *env_va = GUEST_STARTUP;
    for (ep = HOST_ENVIRON; ep && *ep; ++ep) {
        n = (uint32_t)strlen(*ep) + 1u;
        memcpy(ram + GUEST_STARTUP + pos, *ep, n); pos += n;
    }
    ram[GUEST_STARTUP + pos++] = 0;
    *pgm_va = GUEST_STARTUP + pos;
    memcpy(ram + GUEST_STARTUP + pos, pgm, pgm_len); pos += pgm_len;
    ram[GUEST_STARTUP + pos++] = 0;
    *arg_va = GUEST_STARTUP + pos;
    memcpy(ram + GUEST_STARTUP + pos, arg0, arg0_len); pos += arg0_len;
    ram[GUEST_STARTUP + pos++] = 0;
    for (i = 2; i < (uint32_t)argc; ++i) {
        int q = arg_needs_quotes(argv[i]);
        if (i != 2u) ram[GUEST_STARTUP + pos++] = ' ';
        if (q) ram[GUEST_STARTUP + pos++] = '"';
        n = (uint32_t)strlen(argv[i]); memcpy(ram + GUEST_STARTUP + pos, argv[i], n); pos += n;
        if (q) ram[GUEST_STARTUP + pos++] = '"';
    }
    ram[GUEST_STARTUP + pos++] = 0;
    ram[GUEST_STARTUP + pos++] = 0;
    if (pos != total) fatal("startup area size mismatch");
}

/* R3 process-vessel bootstrap.
 *
 * Native DOSCALLS.283 already has mature Win32 process/environment/std-handle
 * mechanics.  Force that backend to re-enter this executable for OS/2 child
 * programs, and publish absolute DLL locations into the inherited environment
 * so a child remains able to load the same personalities after CMD changes its
 * current directory. */
static void host_set_env(const char *name, const char *value)
{
#ifdef _WIN32
    if(value && value[0]) (void)SetEnvironmentVariableA(name,value);
    else (void)SetEnvironmentVariableA(name,NULL);
#else
    if(value && value[0]) (void)setenv(name,value,1);
    else (void)unsetenv(name);
#endif
}

static int host_full_path(const char *src,char *dst,size_t cap)
{
    if(!src||!src[0]||!dst||cap<2)return 0;
#ifdef _WIN32
    {
        DWORD n=GetFullPathNameA(src,(DWORD)cap,dst,NULL);
        return n>0 && n<(DWORD)cap;
    }
#else
    {
        char cwd[2048];
        size_t n;
        if(src[0]=='/'){
            n=strlen(src);if(n+1>cap)return 0;memcpy(dst,src,n+1);return 1;
        }
        if(!getcwd(cwd,sizeof(cwd)))return 0;
        n=strlen(cwd);
        if(n+1+strlen(src)+1>cap)return 0;
        memcpy(dst,cwd,n);dst[n++]='/';strcpy(dst+n,src);return 1;
    }
#endif
}

static void publish_path_env(const char *env_name,const char *path)
{
    char full[4096];
    if(!path||!path[0])return;
    if(host_full_path(path,full,sizeof(full)))host_set_env(env_name,full);
    else host_set_env(env_name,path);
}

static void publish_child_bootstrap(struct Runtime *rt,const char *argv0)
{
    char self[4096];
    self[0]=0;
#ifdef _WIN32
    {
        (void)argv0;
        DWORD n=GetModuleFileNameA(NULL,self,(DWORD)sizeof(self));
        if(!n||n>=sizeof(self))self[0]=0;
    }
#else
    if(argv0 && !host_full_path(argv0,self,sizeof(self))){
        strncpy(self,argv0,sizeof(self)-1);self[sizeof(self)-1]=0;
    }
#endif
    if(self[0])host_set_env("OS2HOST32_LOADER",self);
    publish_path_env("SOFT386_DOSCALLS_DLL",rt->native_dos_path);
    publish_path_env("SOFT386_VIOCALLS_DLL",rt->native_vio_path);
    publish_path_env("SOFT386_KBDCALLS_DLL",rt->native_kbd_path);
    publish_path_env("SOFT386_SESMGR_DLL",rt->native_ses_path);
    publish_path_env("SOFT386_PMWIN_DLL",rt->pm.paths[0]);
    publish_path_env("SOFT386_PMGPI_DLL",rt->pm.paths[1]);
    publish_path_env("SOFT386_PMCTLS_DLL",rt->pm.paths[2]);
    publish_path_env("SOFT386_MSG_DLL",rt->pm.paths[3]);
    publish_path_env("SOFT386_PMSHAPI_DLL",rt->pm.paths[4]);
    publish_path_env("SOFT386_HELPMGR_DLL",rt->pm.paths[5]);
    publish_path_env("SOFT386_QUECALLS_DLL",rt->queue.path);
    publish_path_env("SOFT386_SO32DLL_DLL",rt->net.paths[0]);publish_path_env("SOFT386_TCP32DLL_DLL",rt->net.paths[1]);
    host_set_env("SOFT386_NO_DOSCALLS",rt->native_dos_disabled?"1":NULL);
    host_set_env("SOFT386_NO_SYSTEM_DLLS",rt->native_sys_disabled?"1":NULL);
}

static void put_flat_desc(uint8_t *d, uint8_t access)
{
    d[0]=0xff; d[1]=0xff; d[2]=0; d[3]=0; d[4]=0; d[5]=access; d[6]=0xcf; d[7]=0;
}

static void init_gdt(struct Runtime *rt)
{
    memset(rt->ram + GUEST_GDT, 0, GUEST_GDT_BYTES);
    put_flat_desc(rt->ram + GUEST_GDT + 8, 0x9b);
    put_flat_desc(rt->ram + GUEST_GDT + 16, 0x93);
}

static uint32_t info_tib_address(uint32_t slot)
{
    return GUEST_INFO + 0x100u + slot * 0x100u;
}

static void init_process_info(struct Runtime *rt, uint32_t env, uint32_t cmd, uint32_t module_flags)
{
    uint8_t *p = rt->ram + GUEST_INFO;
    memset(p, 0, GUEST_INFO_LIMIT - GUEST_INFO);
    wr32(p, 1u); wr32(p + 4, 0u); wr32(p + 8, 1u);
    wr32(p + 12, cmd); wr32(p + 16, env);
    wr32(p + 24, (module_flags & 0x300u) == 0x300u ? 3u :
                 ((module_flags & 0x300u) == 0x200u ? 2u : 0u));
}

static void init_thread_info(struct Runtime *rt, uint32_t slot)
{
    struct GuestThread *t = &rt->threads[slot];
    uint32_t base = info_tib_address(slot), selector = (3u + slot) * 8u;
    uint8_t *p = rt->ram + base, *d = rt->ram + GUEST_GDT + selector;
    rt->signal_focus[slot]=0;
    memset(p, 0, 0x100u);
    wr32(p, 0xffffffffu); wr32(p+4, t->stack_base); wr32(p+8, t->stack_base+t->stack_size);
    wr32(p+12, base+0x40u); wr32(p+16, 20u); wr32(p+20, t->tid);
    wr32(p+0x40, t->tid); wr32(p+0x44, 0x0200u); wr32(p+0x48, 20u);
    d[0]=0xff; d[1]=0xff; d[2]=(uint8_t)base; d[3]=(uint8_t)(base>>8);
    d[4]=(uint8_t)(base>>16); d[5]=0x93; d[6]=0x40; d[7]=(uint8_t)(base>>24);
    t->regs.seg[CPUI386_SEG_FS] = selector;
}

static uint32_t get_info_blocks(struct Runtime *rt, uint32_t pptib, uint32_t pppib)
{
    if ((pptib && !guest_range(pptib,4)) || (pppib && !guest_range(pppib,4)))
        return OS2_ERROR_INVALID_PARAMETER;
    if (rt->current_thread < 0 || rt->current_thread >= (int)MAX_THREADS)
        return OS2_ERROR_INVALID_FUNCTION;
    if (pptib) guest_put_u32(rt, pptib, info_tib_address((uint32_t)rt->current_thread));
    if (pppib) guest_put_u32(rt, pppib, GUEST_INFO);
    return OS2_NO_ERROR;
}

static struct GuestThread *current_guest_thread(struct Runtime *rt)
{
    if (rt->current_thread < 0 || rt->current_thread >= (int)MAX_THREADS) return NULL;
    return &rt->threads[rt->current_thread];
}

static struct GuestThread *find_thread(struct Runtime *rt, uint32_t tid)
{
    uint32_t i;
    for (i=0;i<MAX_THREADS;++i)
        if (rt->threads[i].state != THREAD_FREE && rt->threads[i].tid == tid) return &rt->threads[i];
    return NULL;
}

static void set_saved_thread_rc(struct GuestThread *t, uint32_t rc)
{
    if (t->regs_valid) t->regs.gpr[0] = rc;
}


static uint32_t event_make_handle(uint32_t slot, uint32_t generation)
{
    return (generation << 8) | (slot + 1u);
}

static struct GuestEventSem *event_from_handle(struct Runtime *rt, uint32_t hev)
{
    uint32_t raw = hev & 0xffu, slot, generation;
    if (!raw || raw > MAX_EVENT_SEMS) return NULL;
    slot = raw - 1u; generation = hev >> 8;
    if (!generation || !rt->events[slot].used || rt->events[slot].generation != generation) return NULL;
    return &rt->events[slot];
}

static uint32_t sync_handle(uint32_t tag, uint32_t slot, uint32_t generation)
{
    return tag | (generation << 8) | (slot + 1u);
}

static struct GuestMutex *mutex_from_handle(struct Runtime *rt, uint32_t handle)
{
    uint32_t raw = handle & 0xffu;
    struct GuestMutex *m;
    if ((handle & 0xc0000000u) != MUTEX_TAG || !raw || raw > MAX_MUTEXES) return NULL;
    m = &rt->mutexes[raw - 1u];
    if (!m->used || m->generation != ((handle >> 8) & SYNC_GENERATION_MAX)) return NULL;
    return m;
}

static struct GuestThread *sync_first_waiter(struct Runtime *rt,
                                              enum GuestThreadState state,
                                              uint32_t handle)
{
    uint32_t i;
    struct GuestThread *best = NULL;
    for (i = 0; i < MAX_THREADS; ++i) {
        struct GuestThread *t = &rt->threads[i];
        if (t->state == state && t->wait_event == handle &&
            (!best || t->wait_order < best->wait_order)) best = t;
    }
    return best;
}

static void sync_ready(struct GuestThread *t, uint32_t rc)
{
    t->wait_event = 0;
    t->wait_deadline_ms = 0;
    t->wait_order = 0;
    set_saved_thread_rc(t, rc);
    t->state = THREAD_RUNNABLE;
}

static uint32_t sync_block(struct Runtime *rt, enum GuestThreadState state,
                           uint32_t handle, uint32_t timeout)
{
    struct GuestThread *t = current_guest_thread(rt);
    if (!t) return OS2_ERROR_INVALID_FUNCTION;
    t->state = state;
    t->wait_event = handle;
    t->wait_deadline_ms = timeout == SEM_INDEFINITE_WAIT ? 0 : monotonic_ms() + (uint64_t)timeout;
    t->wait_order = ++rt->wait_serial;
    rt->switch_requested = 1;
    return OS2_NO_ERROR;
}

static void wake_event_waiters(struct Runtime *rt, uint32_t hev)
{
    uint32_t i;
    for (i = 0; i < MAX_THREADS; ++i) {
        struct GuestThread *t = &rt->threads[i];
        if (t->state == THREAD_WAIT_EVENT && t->wait_event == hev) {
            if (rt->trace_hc||rt->trace_sched) fprintf(stderr,"soft386: wake TID %u on HEV %08X\n",t->tid,hev);
            sync_ready(t, OS2_NO_ERROR);
        }
    }
}

static void mutex_handoff(struct Runtime *rt, struct GuestMutex *m, uint32_t handle)
{
    struct GuestThread *t = sync_first_waiter(rt, THREAD_WAIT_MUTEX, handle);
    if (!t) return;
    m->owner = t->tid; m->count = 1;
    sync_ready(t, m->abandoned ? OS2_ERROR_SEM_OWNER_DIED : OS2_NO_ERROR);
    m->abandoned = 0;
}

static void mutex_owner_exit(struct Runtime *rt, uint32_t tid)
{
    uint32_t i;
    for (i = 0; i < MAX_MUTEXES; ++i) {
        struct GuestMutex *m = &rt->mutexes[i];
        if (!m->used || m->owner != tid) continue;
        m->owner = 0; m->count = 0; m->abandoned = 1;
        mutex_handoff(rt, m, sync_handle(MUTEX_TAG, i, m->generation));
    }
}

static uint32_t dispatch_event_sem(struct Runtime *rt, uint32_t ordinal,
                                   uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
{
    uint32_t i, slot, hev, old_count;
    struct GuestEventSem *sem;
    char name[EVENT_NAME_MAX + 1];
    int have_name;
    switch (ordinal) {
    case 324:
        if (!a2 || !guest_range(a2,4) || (a3 & ~1u)) return OS2_ERROR_INVALID_PARAMETER;
        have_name = a1 != 0; name[0] = 0;
        if (have_name && !guest_copy_cstr(rt,a1,name,sizeof(name))) return OS2_ERROR_INVALID_PARAMETER;
        slot = MAX_EVENT_SEMS;
        for (i=0;i<MAX_EVENT_SEMS;++i) {
            sem=&rt->events[i];
            if (!sem->used) { if (slot==MAX_EVENT_SEMS && sem->generation<SYNC_GENERATION_MAX) slot=i; continue; }
            if (have_name && sem->name[0] && !_stricmp(sem->name,name)) return OS2_ERROR_DUPLICATE_NAME;
        }
        if (slot==MAX_EVENT_SEMS) return OS2_ERROR_TOO_MANY_OPENS;
        sem=&rt->events[slot]; ++sem->generation; if(!sem->generation)++sem->generation;
        sem->used=1; sem->refs=1; sem->post_count=a4?1u:0u; strcpy(sem->name,name);
        guest_put_u32(rt,a2,event_make_handle(slot,sem->generation)); return OS2_NO_ERROR;
    case 325:
        if(!a2||!guest_range(a2,4))return OS2_ERROR_INVALID_PARAMETER;
        if(a1){
            if(!guest_copy_cstr(rt,a1,name,sizeof(name)))return OS2_ERROR_INVALID_PARAMETER;
            for(i=0;i<MAX_EVENT_SEMS;++i){sem=&rt->events[i];if(sem->used&&sem->name[0]&&!_stricmp(sem->name,name)){++sem->refs;guest_put_u32(rt,a2,event_make_handle(i,sem->generation));return OS2_NO_ERROR;}}
            return OS2_ERROR_SEM_NOT_FOUND;
        }
        hev=guest_u32(rt,a2);sem=event_from_handle(rt,hev);if(!sem)return OS2_ERROR_INVALID_HANDLE;++sem->refs;return OS2_NO_ERROR;
    case 326:
        sem=event_from_handle(rt,a1);if(!sem)return OS2_ERROR_INVALID_HANDLE;
        if(sem->refs>1){--sem->refs;return OS2_NO_ERROR;}
        if(sync_first_waiter(rt,THREAD_WAIT_EVENT,a1))return OS2_ERROR_SEM_BUSY;
        memset(sem,0,sizeof(*sem));return OS2_NO_ERROR;
    case 327:
        if(!a2||!guest_range(a2,4))return OS2_ERROR_INVALID_PARAMETER;
        sem=event_from_handle(rt,a1);if(!sem)return OS2_ERROR_INVALID_HANDLE;
        old_count=sem->post_count;guest_put_u32(rt,a2,old_count);if(!old_count)return OS2_ERROR_ALREADY_RESET;sem->post_count=0;return OS2_NO_ERROR;
    case 328:
        sem=event_from_handle(rt,a1);if(!sem)return OS2_ERROR_INVALID_HANDLE;
        old_count=sem->post_count++;wake_event_waiters(rt,a1);return old_count?OS2_ERROR_ALREADY_POSTED:OS2_NO_ERROR;
    case 329:
        sem=event_from_handle(rt,a1);if(!sem)return OS2_ERROR_INVALID_HANDLE;
        if(sem->post_count)return OS2_NO_ERROR;if(!a2)return OS2_ERROR_SEM_TIMEOUT;
        return sync_block(rt,THREAD_WAIT_EVENT,a1,a2);
    case 330:
        if(!a2||!guest_range(a2,4))return OS2_ERROR_INVALID_PARAMETER;
        sem=event_from_handle(rt,a1);if(!sem)return OS2_ERROR_INVALID_HANDLE;guest_put_u32(rt,a2,sem->post_count);return OS2_NO_ERROR;
    default:return OS2_ERROR_INVALID_FUNCTION;
    }
}

static uint32_t dispatch_mutex_sem(struct Runtime *rt, uint32_t ordinal,
                                   uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
{
    uint32_t i,slot,handle,rc;
    struct GuestMutex *m;
    struct GuestThread *cur=current_guest_thread(rt);
    char name[EVENT_NAME_MAX+1];
    if(!cur)return OS2_ERROR_INVALID_FUNCTION;
    if(ordinal==331||ordinal==332){
        if(!a2||!guest_range(a2,4))return OS2_ERROR_INVALID_PARAMETER;
        name[0]=0;if(a1&&!guest_copy_cstr(rt,a1,name,sizeof(name)))return OS2_ERROR_INVALID_PARAMETER;
        if(ordinal==331&&((a3&~1u)||a4>1u))return OS2_ERROR_INVALID_PARAMETER;
        slot=MAX_MUTEXES;
        for(i=0;i<MAX_MUTEXES;++i){m=&rt->mutexes[i];if(!m->used){if(slot==MAX_MUTEXES&&m->generation<SYNC_GENERATION_MAX)slot=i;}else if(name[0]&&!_stricmp(name,m->name)){if(ordinal==331)return OS2_ERROR_DUPLICATE_NAME;++m->refs;guest_put_u32(rt,a2,sync_handle(MUTEX_TAG,i,m->generation));return OS2_NO_ERROR;}}
        if(ordinal==332){if(a1)return OS2_ERROR_SEM_NOT_FOUND;m=mutex_from_handle(rt,guest_u32(rt,a2));if(!m)return OS2_ERROR_INVALID_HANDLE;++m->refs;return OS2_NO_ERROR;}
        if(slot==MAX_MUTEXES)return OS2_ERROR_TOO_MANY_OPENS;
        m=&rt->mutexes[slot];++m->generation;m->used=1;m->refs=1;m->abandoned=0;m->owner=a4?cur->tid:0;m->count=a4?1:0;strcpy(m->name,name);guest_put_u32(rt,a2,sync_handle(MUTEX_TAG,slot,m->generation));return OS2_NO_ERROR;
    }
    handle=a1;m=mutex_from_handle(rt,handle);if(!m)return OS2_ERROR_INVALID_HANDLE;
    switch(ordinal){
    case 333:if(m->refs>1){--m->refs;return OS2_NO_ERROR;}if(m->owner||sync_first_waiter(rt,THREAD_WAIT_MUTEX,handle))return OS2_ERROR_SEM_BUSY;memset(m,0,sizeof(*m));return OS2_NO_ERROR;
    case 334:
        if(m->owner==cur->tid){if(m->count==0xffffffffu)return OS2_ERROR_TOO_MANY_SEM_REQUESTS;++m->count;return OS2_NO_ERROR;}
        if(!m->owner){rc=m->abandoned?OS2_ERROR_SEM_OWNER_DIED:OS2_NO_ERROR;m->owner=cur->tid;m->count=1;m->abandoned=0;return rc;}
        if(!a2)return OS2_ERROR_SEM_TIMEOUT;return sync_block(rt,THREAD_WAIT_MUTEX,handle,a2);
    case 335:if(m->owner!=cur->tid)return OS2_ERROR_NOT_OWNER;if(--m->count==0){m->owner=0;mutex_handoff(rt,m,handle);}return OS2_NO_ERROR;
    case 336:if(!a2||!a3||!a4||!guest_range(a2,4)||!guest_range(a3,4)||!guest_range(a4,4))return OS2_ERROR_INVALID_PARAMETER;guest_put_u32(rt,a2,m->owner?1u:0u);guest_put_u32(rt,a3,m->owner);guest_put_u32(rt,a4,m->count);return OS2_NO_ERROR;
    default:return OS2_ERROR_INVALID_FUNCTION;
    }
}

static void wake_thread_waiters(struct Runtime *rt, uint32_t dead_tid)
{
    uint32_t i;
    for (i=0;i<MAX_THREADS;++i) {
        struct GuestThread *t=&rt->threads[i];
        if (t->state != THREAD_WAIT_THREAD) continue;
        if (t->wait_tid && t->wait_tid != dead_tid) continue;
        if (t->wait_ptid && guest_range(t->wait_ptid,4)) guest_put_u32(rt,t->wait_ptid,dead_tid);
        t->wait_tid=0; t->wait_ptid=0; set_saved_thread_rc(t,OS2_NO_ERROR); t->state=THREAD_RUNNABLE;
        if(rt->trace_hc||rt->trace_sched)fprintf(stderr,"soft386: wake TID %u after TID %u completion\n",t->tid,dead_tid);
    }
}

static void finish_guest_thread(struct Runtime *rt)
{
    struct GuestThread *t=current_guest_thread(rt); uint32_t i;
    if (!t) return;
    if(rt->trace_hc||rt->trace_sched)fprintf(stderr,"soft386: TID %u exited\n",t->tid);
    mutex_owner_exit(rt,t->tid);
    t->state=THREAD_DEAD; wake_thread_waiters(rt,t->tid);
    if (t->owns_stack && !t->callback_depth) { struct GuestAlloc *ga=find_alloc(rt,t->stack_base); if (ga) ga->used=0; t->owns_stack=0; }
    rt->current_thread_exited=1; rt->switch_requested=1;
    for(i=0;i<MAX_THREADS;++i)
        if(rt->threads[i].state!=THREAD_FREE && rt->threads[i].state!=THREAD_DEAD) break;
    if(i==MAX_THREADS){rt->process_exited=1;rt->process_rc=0;rt->process_reason="last guest thread returned";}
}

static uint32_t create_guest_thread(struct Runtime *rt, uint32_t ptid, uint32_t entry,
                                    uint32_t param, uint32_t flags, uint32_t stack_size)
{
    uint32_t i, stack_bytes, stack_base, sp, tid;
    struct GuestThread *t;
    if (!ptid || !entry || !stack_size || !guest_range(ptid,4) || !guest_range(entry,1) || (flags & ~1u))
        return OS2_ERROR_INVALID_PARAMETER;
    for(i=0;i<MAX_THREADS;++i)
        if(!rt->threads[i].callback_depth && (rt->threads[i].state==THREAD_FREE || rt->threads[i].state==THREAD_DEAD)) break;
    if(i==MAX_THREADS) return OS2_ERROR_NOT_ENOUGH_MEMORY;
    stack_bytes=align_up(stack_size,0x1000u); stack_base=alloc_guest(rt,stack_bytes);
    if(!stack_base) return OS2_ERROR_NOT_ENOUGH_MEMORY;
    t=&rt->threads[i]; memset(t,0,sizeof(*t));
    cpui386_get_state(rt->cpu,&t->regs);
    sp=(stack_base+stack_bytes)&~0x0fu;
    sp-=4; guest_put_u32(rt,sp,param);
    sp-=4; guest_put_u32(rt,sp,GUEST_THREAD_EXIT_STUB);
    t->regs.gpr[0]=0; t->regs.gpr[1]=0; t->regs.gpr[2]=0; t->regs.gpr[3]=0;
    t->regs.gpr[4]=sp; t->regs.gpr[5]=0; t->regs.gpr[6]=0; t->regs.gpr[7]=0;
    t->regs.ip=entry; t->regs.next_ip=entry; t->regs.flags=2;
    tid=rt->next_tid++; if(!tid) tid=rt->next_tid++;
    t->tid=tid; t->stack_base=stack_base; t->stack_size=stack_bytes; t->owns_stack=1;
    init_thread_info(rt,i); t->regs_valid=1;
    t->state=(flags&1u)?THREAD_SUSPENDED:THREAD_RUNNABLE;
    guest_put_u32(rt,ptid,tid);
    if(rt->trace_hc||rt->trace_sched)
        fprintf(stderr,"soft386: create TID %u EIP=%08X stack=%08X..%08X ESP=%08X state=%s\n",
                tid,entry,stack_base,stack_base+stack_bytes,sp,(flags&1u)?"SUSPENDED":"RUNNABLE");
    return OS2_NO_ERROR;
}

static uint32_t guest_wait_thread(struct Runtime *rt, uint32_t ptid, uint32_t option)
{
    uint32_t target_tid,i; struct GuestThread *target,*cur;
    if(!guest_range(ptid,4)||(option!=DCWW_WAIT&&option!=DCWW_NOWAIT)) return OS2_ERROR_INVALID_PARAMETER;
    target_tid=guest_u32(rt,ptid); cur=current_guest_thread(rt); if(!cur) return OS2_ERROR_INVALID_FUNCTION;
    if(target_tid==0){
        target=NULL; for(i=0;i<MAX_THREADS;++i) if(rt->threads[i].state==THREAD_DEAD){target=&rt->threads[i];break;}
        if(target){guest_put_u32(rt,ptid,target->tid);target->state=THREAD_FREE;return OS2_NO_ERROR;}
    } else {
        target=find_thread(rt,target_tid);
        if(!target&&target_tid>=1u&&target_tid<rt->next_tid) return OS2_NO_ERROR;
        if(!target||target==cur) return OS2_ERROR_INVALID_THREADID;
        if(target->state==THREAD_DEAD){guest_put_u32(rt,ptid,target_tid);target->state=THREAD_FREE;return OS2_NO_ERROR;}
    }
    if(option==DCWW_NOWAIT) return OS2_ERROR_THREAD_NOT_TERMINATED;
    cur->wait_tid=target_tid; cur->wait_ptid=ptid; cur->state=THREAD_WAIT_THREAD; rt->switch_requested=1;
    if(rt->trace_hc||rt->trace_sched)fprintf(stderr,"soft386: TID %u waits for %s%u\n",cur->tid,target_tid?"TID ":"any TID ",target_tid);
    return OS2_NO_ERROR;
}

static uint32_t guest_sleep(struct Runtime *rt, uint32_t ms)
{
    struct GuestThread *cur=current_guest_thread(rt); if(!cur) return OS2_ERROR_INVALID_FUNCTION;
    if(ms){cur->wait_deadline_ms=monotonic_ms()+ms;cur->state=THREAD_SLEEP;}
    rt->switch_requested=1; return OS2_NO_ERROR;
}

static void wake_expired_waiters(struct Runtime *rt)
{
    uint64_t now=monotonic_ms(); uint32_t i;
    for(i=0;i<MAX_THREADS;++i){
        struct GuestThread *t=&rt->threads[i];
        if (!t->wait_deadline_ms || t->wait_deadline_ms > now) continue;
        if(t->state==THREAD_SLEEP||t->state==THREAD_WAIT_PM||t->state==THREAD_WAIT_QUEUE){sync_ready(t,OS2_NO_ERROR);continue;}
        if(t->state==THREAD_WAIT_EVENT||t->state==THREAD_WAIT_MUTEX){
            if(rt->trace_hc||rt->trace_sched)fprintf(stderr,"soft386: TID %u semaphore wait timed out\n",t->tid);
            sync_ready(t,OS2_ERROR_SEM_TIMEOUT);
        }
    }
}

static int next_runnable_thread(struct Runtime *rt)
{
    uint32_t n; int start=rt->current_thread;
    for(n=1;n<=MAX_THREADS;++n){int i=(start+(int)n)%(int)MAX_THREADS;if(rt->threads[i].state==THREAD_RUNNABLE)return i;}
    return -1;
}

static int schedule_next_thread(struct Runtime *rt)
{
    int next;
    for(;;){
        uint64_t earliest=0,now; uint32_t i; wake_expired_waiters(rt); next=next_runnable_thread(rt); if(next>=0)break;
        if(rt->modal_pump_active)return 2;
        for(i=0;i<MAX_THREADS;++i) {
            struct GuestThread *t=&rt->threads[i];
            if((t->state==THREAD_SLEEP||t->state==THREAD_WAIT_PM||t->state==THREAD_WAIT_QUEUE||t->state==THREAD_WAIT_EVENT||t->state==THREAD_WAIT_MUTEX) &&
               t->wait_deadline_ms && (!earliest||t->wait_deadline_ms<earliest)) earliest=t->wait_deadline_ms;
        }
        if(!earliest){fprintf(stderr,"soft386: scheduler deadlock: no runnable thread\n");return 1;}
        now=monotonic_ms(); if(earliest>now)sleep_ms(earliest-now);
    }
    rt->current_thread=next; rt->threads[next].state=THREAD_RUNNING;
    if(!cpui386_set_state(rt->cpu,&rt->threads[next].regs)){fprintf(stderr,"soft386: state restore failed for TID %u\n",rt->threads[next].tid);return 1;}
    if(rt->trace_hc||rt->trace_sched)
        fprintf(stderr,"soft386: scheduler -> TID %u EIP=%08X ESP=%08X FS=%04X\n",
                rt->threads[next].tid,rt->threads[next].regs.next_ip,rt->threads[next].regs.gpr[4],
                (unsigned)rt->threads[next].regs.seg[CPUI386_SEG_FS]);
    return 0;
}

static int switch_after_hypercall(struct Runtime *rt)
{
    struct GuestThread *cur=current_guest_thread(rt);
    if(!cur)return 1;
    if(!rt->current_thread_exited){
        cpui386_get_state(rt->cpu,&cur->regs);
        cur->regs.ip=cur->regs.next_ip; /* OUT completed: resume at veneer RET */
        cur->regs_valid=1;
        if(cur->state==THREAD_RUNNING)cur->state=THREAD_RUNNABLE;
    }
    rt->switch_requested=0; rt->current_thread_exited=0;
    return schedule_next_thread(rt);
}

static int pm_thread_hostcall(void *opaque,uint32_t tid)
{
    struct Runtime *rt=(struct Runtime *)opaque;unsigned i;
    if(!rt||!tid)return 0;
    for(i=0;i<MAX_THREADS;i++)if(rt->threads[i].state!=THREAD_FREE&&rt->threads[i].tid==tid)
        return rt->threads[i].state==THREAD_HOSTCALL;
    return 0;
}

static void modal_pause_current(struct Runtime *rt)
{
    struct GuestThread *cur=current_guest_thread(rt);
    if(!cur||cur->state!=THREAD_RUNNING)return;
    cpui386_get_state(rt->cpu,&cur->regs);
    cur->regs.ip=cur->regs.next_ip;
    cur->regs_valid=1;cur->state=THREAD_RUNNABLE;
}

/* Called by PMWIN only at idle points inside a native modal message loop.
 * Tiny386 is never used concurrently: park the modal owner, run other guest
 * threads for a bounded cooperative slice, then restore the exact native-call
 * continuation and pending hypercall state. */
static int pm_modal_scheduler_yield(void *opaque)
{
    struct Runtime *rt=(struct Runtime *)opaque;struct GuestThread *owner,*nextt;
    CPUI386_State owner_cpu;enum GuestThreadState owner_state;
    int owner_index,next,sw,ex,retry,pending,rc;uint32_t api;
    if(!rt||rt->process_exited||rt->modal_pump_active)return rt&&rt->process_exited;
    owner_index=rt->current_thread;owner=current_guest_thread(rt);if(!owner||owner->state!=THREAD_RUNNING)return 0;
    wake_expired_waiters(rt);owner_state=owner->state;owner->state=THREAD_HOSTCALL;next=next_runnable_thread(rt);
    if(next<0){owner->state=owner_state;return 0;}
    cpui386_get_state(rt->cpu,&owner_cpu);
    sw=rt->switch_requested;ex=rt->current_thread_exited;retry=rt->retry_hostcall;pending=rt->pending_hostcall;api=rt->pending_api;
    rt->modal_pump_active=1;rt->modal_owner=owner_index;rt->modal_budget=4096;
    rt->current_thread=next;nextt=&rt->threads[next];nextt->state=THREAD_RUNNING;
    rt->switch_requested=rt->current_thread_exited=rt->retry_hostcall=rt->pending_hostcall=0;rt->pending_api=0;
    if(!cpui386_set_state(rt->cpu,&nextt->regs))fatal("modal scheduler CPU restore failed");
    if(rt->trace_hc||rt->trace_sched)fprintf(stderr,"soft386: PM modal yield owner=TID %u -> TID %u\n",owner->tid,nextt->tid);
    rc=run_guest_until(rt,NULL);(void)rc;
    if(!rt->process_exited)modal_pause_current(rt);
    owner->state=owner_state;rt->current_thread=owner_index;
    if(!cpui386_set_state(rt->cpu,&owner_cpu))fatal("modal owner continuation restore failed");
    rt->switch_requested=sw;rt->current_thread_exited=ex;rt->retry_hostcall=retry;rt->pending_hostcall=pending;rt->pending_api=api;
    rt->modal_pump_active=0;rt->modal_owner=-1;rt->modal_budget=0;
    if(rt->trace_hc||rt->trace_sched)fprintf(stderr,"soft386: PM modal resume owner=TID %u process_exited=%u\n",owner->tid,rt->process_exited?1u:0u);
    return rt->process_exited?1:0;
}

#include "soft386_callbacks.h"

static uint32_t dispatch_network(struct Runtime *rt,unsigned module,uint32_t ordinal,uint32_t esp)
{
    struct Soft386GuestMemoryOps memops;struct GuestThread *t=current_guest_thread(rt);int handled=0;uint32_t rc;
    unsigned slot=(unsigned)rt->current_thread;
    unsigned result_kind=module==1?(ordinal==10?2:((ordinal==23||ordinal==24)?1:0)):0;
    if(!t||slot>=MAX_THREADS)return UINT32_MAX;
    if(!rt->net_arenas[slot][result_kind])rt->net_arenas[slot][result_kind]=alloc_guest(rt,S386_NET_ARENA);
    if(!rt->net_arenas[slot][result_kind])return module==1?0:UINT32_MAX;
    bridge_memory_ops(rt,&memops);
    rc=soft386_net_dispatch(&rt->net,&memops,module,ordinal,esp,slot,t->tid,rt->net_arenas[slot][result_kind],&handled);
    if(!handled)fprintf(stderr,"soft386: unsupported network module=%u ordinal=%u\n",module,ordinal);
    if(rt->net.waiting){t->state=THREAD_WAIT_QUEUE;t->wait_deadline_ms=monotonic_ms()+2;
        rt->retry_hostcall=1;rt->switch_requested=1;}
    return rc;
}
#include "soft386_search_path.h"
#include "soft386_exitlist.h"
static uint32_t dispatch_doscalls(struct Runtime *rt, uint32_t ordinal, uint32_t esp)
{
    uint32_t a1=guest_u32(rt,esp+4),a2=guest_u32(rt,esp+8),a3=guest_u32(rt,esp+12),a4=guest_u32(rt,esp+16);
    uint32_t a5=guest_range(esp+20,4)?guest_u32(rt,esp+20):0;
    struct Os2PersonalityContext personality;
    os2_personality_context_init(&personality,rt,&soft_personality_ops);
    os2_personality_context_set_nls(&personality,&rt->nls_state);
    if(rt->trace_hc)fprintf(stderr,"soft386: DOSCALLS.%u ESP=%08X args=%08X,%08X,%08X,%08X,%08X\n",ordinal,esp,a1,a2,a3,a4,a5);
    if(rt->exit_processing&&(ordinal==311||ordinal==283))return 87;
    if (rt->native_dos.loaded && soft386_doscalls_bridge_export(&rt->native_dos, ordinal)) {
        struct Soft386GuestMemoryOps memops;
        unsigned abi_nargs = 0, abi_flags = soft386_doscalls_bridge_abi(ordinal, &abi_nargs);
        int handled = 0;
        uint32_t native_rc;
        (void)abi_nargs;
        if (abi_flags) {
            bridge_memory_ops(rt, &memops);
            if (rt->trace_hc)
                fprintf(stderr,"soft386: service DOSCALLS.%u export resolved; ABI class=%s\n",ordinal,
                    (abi_flags&SOFT386_DOS_ABI_SCALAR)?"scalar":"marshal");
            if ((abi_flags & (SOFT386_DOS_ABI_SCALAR|SOFT386_DOS_MAY_BLOCK)) ==
                (SOFT386_DOS_ABI_SCALAR|SOFT386_DOS_MAY_BLOCK)) {
                struct GuestThread *t=current_guest_thread(rt);
                int waiting=0;
                if(!t)return OS2_ERROR_INVALID_FUNCTION;
                native_rc=soft386_doscalls_bridge_dispatch_async_scalar(&rt->native_dos,&memops,ordinal,esp,
                    (unsigned)rt->current_thread,t->tid,&waiting,&handled);
                if(handled){
                    if(waiting){t->state=THREAD_WAIT_QUEUE;t->wait_deadline_ms=monotonic_ms()+2;
                        rt->retry_hostcall=1;rt->switch_requested=1;}
                    return native_rc;
                }
            }
            native_rc = soft386_doscalls_bridge_dispatch(&rt->native_dos, &memops, ordinal, esp, &handled);
            if (handled) return native_rc;
        }
    }
    switch(ordinal){
    case 32:return guest_sleep(rt,a1);
    case 224:return os2_core_DosQueryHType(&personality,a1,a2,a3);
    case 228:return guest_search_path(rt,a1,a2,a3,a4,a5);
    case 229:return guest_sleep(rt,a1);
    case 230:return os2_core_DosGetDateTime(&personality,a1);
    case 234:
        if(a1==EXIT_THREAD){finish_guest_thread(rt);return OS2_NO_ERROR;}
        if(a1!=EXIT_PROCESS)return OS2_ERROR_INVALID_PARAMETER;
        rt->process_exited=1;rt->process_rc=a2;rt->process_reason="DosExit(EXIT_PROCESS)";return OS2_NO_ERROR;
    case 236:{
        struct GuestThread *t=current_guest_thread(rt);
        int32_t delta=(int32_t)a3;
        /* DosSetPriority is guest scheduling state.  Soft386 multiplexes all
         * guest TIDs on one host thread, so forwarding this call to Win32
         * would incorrectly change every guest thread at once.  Keep only
         * the OS/2-visible per-thread scheduling attributes in the jar. */
        if(!t||a1!=2u||a4!=0u||a2>4u||delta<-31||delta>31)return OS2_ERROR_INVALID_PARAMETER;
        if(a2)t->priority_class=a2;
        t->priority_delta=delta;
        return OS2_NO_ERROR;
    }
    case 256:return os2_core_DosSetFilePtr(&personality,a1,(int32_t)a2,a3,a4);
    case 282:return os2_core_DosWrite(&personality,a1,a2,a3,a4);
    case 289:return os2_nls_api_DosSetProcessCp(&personality,a1);
    case 291:return os2_nls_api_DosQueryCp(&personality,a1,a2,a3);
    case 296:return guest_exit_list(rt,a1,a2);
    case 354:case 355:return guest_exception_chain(rt,ordinal,a1);
    case 378:
        if(a1>1||!a2||!guest_range(a2,4))return 87;
        if(a1){if(rt->signal_focus[rt->current_thread]!=UINT32_MAX)rt->signal_focus[rt->current_thread]++;}
        else if(rt->signal_focus[rt->current_thread])rt->signal_focus[rt->current_thread]--;
        guest_put_u32(rt,a2,rt->signal_focus[rt->current_thread]);return 0;
    case 299:return os2_core_DosAllocMem(&personality,a1,a2,a3,a4);
    case 304:return os2_core_DosFreeMem(&personality,a1);
    case 305:return os2_core_DosSetMem(&personality,a1,a2,a3);
    case 306:return guest_query_mem(rt,a1,a2,a3);
    case 311:return create_guest_thread(rt,a1,a2,a3,a4,a5);
    case 312:return get_info_blocks(rt,a1,a2);
    case 318:case 319:case 320:case 321:case 322:case 352:case 353:case 572:
        return dispatch_modules(rt,ordinal,a1,a2,a3,a4);
    case 324:case 325:case 326:case 327:case 328:case 329:case 330:
        return dispatch_event_sem(rt,ordinal,a1,a2,a3,a4);
    case 331:case 332:case 333:case 334:case 335:case 336:
        return dispatch_mutex_sem(rt,ordinal,a1,a2,a3,a4);
    case 348:return os2_core_DosQuerySysInfo(&personality,a1,a2,a3,a4);
    case 395:return os2_nls_api_DosQueryCtryInfo(&personality,a1,a2,a3,a4);
    case 396:return os2_nls_api_DosQueryDBCSEnv(&personality,a1,a2,a3);
    case 397:return os2_nls_api_DosMapCase(&personality,a1,a2,a3);
    case 349:return guest_wait_thread(rt,a1,a2);
    default:
        if(rt->native_dos.loaded){
            if(!soft386_doscalls_bridge_export(&rt->native_dos,ordinal))
                fprintf(stderr,"soft386: service DOSCALLS.%u native export missing\n",ordinal);
            else
                fprintf(stderr,"soft386: service DOSCALLS.%u export resolved; ABI descriptor missing; call not attempted\n",ordinal);
        }else fprintf(stderr,"soft386: service DOSCALLS.%u native provider unavailable\n",ordinal);
        return OS2_ERROR_INVALID_FUNCTION;
    }
}

static uint32_t dispatch_nls_module(struct Runtime *rt,uint32_t ordinal,uint32_t esp)
{
    uint32_t a1=guest_u32(rt,esp+4),a2=guest_u32(rt,esp+8),a3=guest_u32(rt,esp+12),a4=guest_u32(rt,esp+16);
    struct Os2PersonalityContext personality;
    os2_personality_context_init(&personality,rt,&soft_personality_ops);
    os2_personality_context_set_nls(&personality,&rt->nls_state);
    if(rt->trace_hc)fprintf(stderr,"soft386: NLS.%u ESP=%08X\n",ordinal,esp);
    switch(ordinal){
    case 5:return os2_nls_api_DosQueryCtryInfo(&personality,a1,a2,a3,a4);
    case 6:return os2_nls_api_DosQueryDBCSEnv(&personality,a1,a2,a3);
    case 7:return os2_nls_api_DosMapCase(&personality,a1,a2,a3);
    default:return OS2_ERROR_INVALID_FUNCTION;
    }
}

static void ensure_system_module(struct Runtime *rt,const char *module)
{
    if(!rt->native_sys_disabled) {
        uint32_t mask=!_stricmp(module,"VIOCALLS")?SOFT386_SYS_VIO:(!_stricmp(module,"KBDCALLS")?SOFT386_SYS_KBD:SOFT386_SYS_SES);
        soft386_system_bridge_open_mask(&rt->native_sys,mask,rt->native_vio_path,rt->native_vio_required,
            rt->native_kbd_path,rt->native_kbd_required,rt->native_ses_path,rt->native_ses_required,rt->native_sys.trace);
    }
}

static uint32_t dispatch_system_module(struct Runtime *rt,const char *module,uint32_t ordinal,uint32_t esp)
{
    struct Soft386GuestMemoryOps memops;int handled=0;uint32_t rc;
    ensure_system_module(rt,module);
    bridge_memory_ops(rt,&memops);
    if(rt->trace_hc)fprintf(stderr,"soft386: %s.%u ESP=%08X\n",module,ordinal,esp);
    rc=soft386_system_bridge_dispatch32(&rt->native_sys,&memops,module,ordinal,esp,&handled);
    if(!handled)fprintf(stderr,"soft386: unsupported %s.%u\n",module,ordinal);
    return rc;
}

static uint32_t dispatch_c386_console(struct Runtime *rt,uint32_t desc_id,uint32_t esp)
{
    struct Soft386GuestMemoryOps memops;int handled=0;uint32_t rc;
    const struct Soft386C386ApiDesc *d=soft386_c386_desc_by_id(desc_id);
    bridge_memory_ops(rt,&memops);
    if(rt->trace_hc&&d)fprintf(stderr,"soft386: C/386 %s.%u %s ESP=%08X\n",d->module,d->ordinal,d->name,esp);

    /* DOSCALLS helpers which define scheduler/kernel semantics must not cross
     * into the native DLL.  Life and other INCL_16 C/386 programs import the
     * historical DOSCALLS.32 DosSleep entry through the migration helper.
     * Its packed Pascal frame is simply one ULONG at ESP+4. */
    if(d&&_stricmp(d->module,"DOSCALLS")==0){
        if(d->ordinal==32u){
            if(!guest_range(esp+4,4))return OS2_ERROR_INVALID_PARAMETER;
            return guest_sleep(rt,guest_u32(rt,esp+4));
        }
        fprintf(stderr,"soft386: unsupported jar-owned C/386 %s.%u\n",d->module,d->ordinal);
        return OS2_ERROR_INVALID_FUNCTION;
    }

    if(d)ensure_system_module(rt,d->module);
    rc=soft386_system_bridge_dispatch_c386(&rt->native_sys,&memops,desc_id,esp,&handled);
    if(!handled)fprintf(stderr,"soft386: unsupported C/386 console descriptor %u\n",desc_id);
    return rc;
}

static void emit_runtime_stubs(struct Runtime *rt)
{
    uint8_t *p=rt->ram+GUEST_THREAD_EXIT_STUB;
    p[0]=0xB8;wr32(p+1,HC_RUNTIME|HC_RUNTIME_THREAD_RETURN);p[5]=0xE7;p[6]=(uint8_t)HOSTCALL_PORT;p[7]=0xF4;
    p=rt->ram+GUEST_CALLBACK_RETURN_STUB;
    p[0]=0x50;p[1]=0xB8;wr32(p+2,HC_RUNTIME|HC_RUNTIME_CALLBACK_RETURN);
    p[6]=0xE7;p[7]=(uint8_t)HOSTCALL_PORT;p[8]=0xF4;
}

static uint8_t io_read8(void *o,int p){(void)o;(void)p;return 0xff;}
static uint16_t io_read16(void *o,int p){(void)o;(void)p;return 0xffff;}
static uint32_t io_read32(void *o,int p){(void)o;(void)p;return 0xffffffffu;}
static void io_write8(void *o,int p,uint8_t v){struct Runtime*rt=o;if((p&0xffff)==0xe9){fputc(v,stdout);fflush(stdout);}else if(rt->trace_hc)fprintf(stderr,"soft386: OUT8 %04X=%02X\n",p&0xffff,v);}
static void io_write16(void *o,int p,uint16_t v){struct Runtime*rt=o;if(rt->trace_hc)fprintf(stderr,"soft386: OUT16 %04X=%04X\n",p&0xffff,v);}
static void io_write32(void *o,int p,uint32_t v){struct Runtime*rt=o;if((p&0xffff)==HOSTCALL_PORT){rt->pending_hostcall=1;rt->pending_api=v;}else if(rt->trace_hc)fprintf(stderr,"soft386: OUT32 %04X=%08X\n",p&0xffff,v);}

static void force_process_termination(struct Runtime *rt,uint32_t rc,const char *reason)
{
    unsigned i;
    rt->process_exited=1;rt->process_forced=1;rt->process_rc=rc;rt->process_reason=reason;
    fprintf(stderr,"soft386: forced process termination reason=%s rc=%u\n",reason?reason:"forced",(unsigned)rc);
    rt->switch_requested=0;rt->retry_hostcall=0;rt->current_thread_exited=1;
    for(i=0;i<MAX_THREADS;i++)if(rt->threads[i].state!=THREAD_FREE)rt->threads[i].state=THREAD_DEAD;
}

static void trace_callback_cpu(struct Runtime *rt,struct GuestCallback *stop,const CPUI386_State *st)
{
    uint32_t ip;unsigned i,n;
    if(!rt||!stop||!st||!stop->cpu_trace_id||stop->cpu_trace_steps>=stop->cpu_trace_limit)return;
    ip=st->next_ip;
    fprintf(stderr,"soft386: cb-cpu id=%u step=%u TID=%u depth=%u EIP=%08X EFLAGS=%08X EAX=%08X ECX=%08X EDX=%08X EBX=%08X ESP=%08X EBP=%08X ESI=%08X EDI=%08X bytes=",
            stop->cpu_trace_id,stop->cpu_trace_steps,stop->tid,
            current_guest_thread(rt)?current_guest_thread(rt)->callback_depth:0u,
            ip,st->flags,st->gpr[0],st->gpr[1],st->gpr[2],st->gpr[3],st->gpr[4],st->gpr[5],st->gpr[6],st->gpr[7]);
    n=guest_range(ip,12)?12u:(guest_range(ip,1)?1u:0u);
    for(i=0;i<n;i++)fprintf(stderr,"%02X",rt->ram[ip+i]);
    if(!n)fputs("<unmapped>",stderr);
    fputc('\n',stderr);
    stop->cpu_trace_steps++;
}

static void trace_watch_changes(struct Runtime *rt,const uint8_t *before,uint32_t ip,const char *phase)
{
    unsigned i,j;
    if(!rt||!before||!rt->watch_len||!rt->ram)return;
    i=0;
    while(i<rt->watch_len){
        if(before[i]==rt->ram[rt->watch_addr+i]){i++;continue;}
        j=i+1;
        while(j<rt->watch_len&&before[j]!=rt->ram[rt->watch_addr+j])j++;
        fprintf(stderr,"soft386: mem-watch %s EIP=%08X addr=%08X size=%u old=",phase?phase:"cpu",ip,rt->watch_addr+i,j-i);
        {unsigned k;for(k=i;k<j;k++)fprintf(stderr,"%02X",before[k]);}
        fputs(" new=",stderr);
        {unsigned k;for(k=i;k<j;k++)fprintf(stderr,"%02X",rt->ram[rt->watch_addr+k]);}
        fputc('\n',stderr);
        i=j;
    }
}

static int run_guest_until(struct Runtime *rt,struct GuestCallback *stop)
{
    long stagnant=0,last=-1;unsigned quantum=0,modal_steps=0;
    for(;;){
        CPUI386_State st; uint32_t module,ordinal,rc,esp,watch_ip=0;
        uint8_t watch_before[256];
        long before=cpui386_get_cycle(rt->cpu);
        if(rt->modal_pump_active&&rt->modal_budget&&modal_steps>=rt->modal_budget){modal_pause_current(rt);return 0;}
        if(rt->process_exited)return (int)rt->process_rc;
        if(stop&&stop->done)return 0;
        if(stop){struct GuestThread *owner=find_thread(rt,stop->tid);if(!owner||owner->state==THREAD_DEAD){
            fprintf(stderr,"soft386: thread exited inside synchronous native callback\n");return 1;
        }}
        if(rt->max_cycles && before>=rt->max_cycles){
            fprintf(stderr,"soft386: cycle limit reached\n");
            force_process_termination(rt,124,"cycle-limit");
            return 124;
        }
        rt->pending_hostcall=0;
        if(stop&&stop->cpu_trace_id){cpui386_get_state(rt->cpu,&st);trace_callback_cpu(rt,stop,&st);}
        if(rt->watch_len){cpui386_get_state(rt->cpu,&st);watch_ip=st.next_ip;memcpy(watch_before,rt->ram+rt->watch_addr,rt->watch_len);}
        cpui386_step(rt->cpu,1);
        if(rt->watch_len){trace_watch_changes(rt,watch_before,watch_ip,"cpu");memcpy(watch_before,rt->ram+rt->watch_addr,rt->watch_len);}
        if(rt->modal_pump_active)modal_steps++;
        if(rt->pending_hostcall){
            cpui386_get_state(rt->cpu,&st); esp=st.gpr[4]; module=rt->pending_api&0xff000000u;ordinal=rt->pending_api&0x00ffffffu;
            if(!guest_range(esp,4)){fprintf(stderr,"soft386: invalid hypercall ESP %08X\n",esp);return 1;}
            if(module==HC_DOSCALLS)rc=dispatch_doscalls(rt,ordinal,esp);
            else if(module==HC_VIOCALLS)rc=dispatch_system_module(rt,"VIOCALLS",ordinal,esp);
            else if(module==HC_KBDCALLS)rc=dispatch_system_module(rt,"KBDCALLS",ordinal,esp);
            else if(module==HC_SESMGR)rc=dispatch_system_module(rt,"SESMGR",ordinal,esp);
            else if(module==HC_NLS)rc=dispatch_nls_module(rt,ordinal,esp);
            else if(module==HC_C386_CONSOLE)rc=dispatch_c386_console(rt,ordinal,esp);
            else if(module==HC_PMWP){
                /* PMWP.203 delegates to DosLoadModule. Ownership belongs to
                 * this jar; the native DLL must not map LE/LX into host RAM. */
                if(ordinal==203&&guest_range(esp,20))
                    rc=dispatch_modules(rt,318,0,0,guest_u32(rt,esp+12),guest_u32(rt,esp+8));
                else{fprintf(stderr,"soft386: unsupported PMWP.%u\n",ordinal);rc=1;}
            }
            else if(module==HC_QUECALLS){
                struct Soft386GuestMemoryOps memops;int handled=0;bridge_memory_ops(rt,&memops);
                rc=soft386_queue_dispatch(&rt->queue,&memops,ordinal,esp,&handled);
                if(!handled)fprintf(stderr,"soft386: QUECALLS.%u bridge did not classify call\n",ordinal);
                if(rt->queue.waiting){struct GuestThread *t=current_guest_thread(rt);
                    t->state=THREAD_WAIT_QUEUE;t->wait_deadline_ms=monotonic_ms()+10;
                    rt->retry_hostcall=1;rt->switch_requested=1;
                }
            }
            else if(module==HC_SO32DLL||module==HC_TCP32DLL)rc=dispatch_network(rt,module==HC_TCP32DLL,ordinal,esp);
            else if(module==HC_PM_OLDPROC){
                struct Soft386GuestMemoryOps memops;bridge_memory_ops(rt,&memops);
                rc=soft386_pm_call_proc(&rt->pm,&memops,ordinal,esp);
            }
            else if(module>=HC_PMWIN&&module<=HC_HELPMGR){
                struct Soft386GuestMemoryOps memops;int handled=0;bridge_memory_ops(rt,&memops);
                rc=soft386_pm_dispatch(&rt->pm,&memops,(module-HC_PMWIN)>>24,ordinal,esp,&handled);
                if(!handled)fprintf(stderr,"soft386: PM hostcall %08X bridge did not classify call\n",rt->pending_api);
                if(rt->pm.waiting){struct GuestThread *t=current_guest_thread(rt);
                    t->state=THREAD_WAIT_PM;t->wait_deadline_ms=monotonic_ms()+10;
                    rt->retry_hostcall=1;rt->switch_requested=1;
                }
            }
            else if(module==HC_RUNTIME&&ordinal==HC_RUNTIME_CALLBACK_RETURN){
                if(!return_guest_callback(rt,&st)){rt->process_exited=1;rt->process_rc=1;rt->process_reason="invalid callback return frame";return 1;}
                if(stop&&stop->done)return 0;
                fprintf(stderr,"soft386: callback return does not match active native frame\n");return 1;
            }
            else if(module==HC_RUNTIME&&ordinal==3){fprintf(stderr,"soft386: TELNETPM unsupported external far callback\n");rt->process_exited=1;rt->process_rc=1;rt->process_reason="unsupported external far callback";rc=1;}
            else if(module==HC_RUNTIME&&ordinal==HC_RUNTIME_THREAD_RETURN){finish_guest_thread(rt);rc=0;}
            else{fprintf(stderr,"soft386: unknown hostcall %08X\n",rt->pending_api);return 1;}
            if(rt->watch_len)trace_watch_changes(rt,watch_before,watch_ip,"hostcall");
            if(rt->process_exited)return (int)rt->process_rc;
            if(rt->retry_hostcall){
                cpui386_get_state(rt->cpu,&st);
                if(st.next_ip<GUEST_STUB_BASE+7u||st.next_ip>=GUEST_CALLBACK_RETURN_STUB||
                   rt->ram[st.next_ip-7u]!=0xB8||rd32(rt->ram+st.next_ip-6u)!=(module|ordinal)){
                    fprintf(stderr,"soft386: wait outside a generated import veneer\n");return 1;
                }
                st.ip=st.next_ip=st.next_ip-7u;
                if(!cpui386_set_state(rt->cpu,&st))fatal("cannot retry waiting service call");
                rt->retry_hostcall=0;
            }else if(!rt->current_thread_exited)cpui386_set_gpr(rt->cpu,0,rc);
            if(rt->switch_requested){int sr=switch_after_hypercall(rt);if(sr==2&&rt->modal_pump_active)return 0;if(sr)return 1;}
            continue;
        }
        {
            long after=cpui386_get_cycle(rt->cpu);
            if(after==before&&after==last){if(++stagnant>1){fprintf(stderr,"soft386: guest halted without process exit\n");return 1;}}
            else stagnant=0;
            last=after;
        }
        /* A compute-bound worker must not starve a PM waiter. */
        if(++quantum==4096){int sr;quantum=0;sr=switch_after_hypercall(rt);if(sr==2&&rt->modal_pump_active)return 0;if(sr)return 1;}
    }
}
static int run_guest(struct Runtime *rt){return run_guest_until(rt,NULL);}


static int check_image(const char *path)
{
    struct Runtime rt; struct LeImage x; uint32_t i;int dll;
    memset(&rt,0,sizeof(rt));memset(&x,0,sizeof(x));
    rt.ram=calloc(1,RAM_SIZE);rt.alloc_next=GUEST_ALLOC_BASE;rt.stub_next=GUEST_STUB_BASE;rt.module_next=GUEST_MODULE_BASE;rt.main_image=&x;
    snprintf(rt.main_path,sizeof(rt.main_path),"%s",path);rt.pm.disabled=1;
    if(!rt.ram)fatal("out of memory");
    x.file=load_file(path,&x.file_size);if(!x.file){fprintf(stderr,"soft386: cannot read %s\n",path);free(rt.ram);return 1;}
    loader_path=path;parse_header(&x);dll=(x.module_flags&MOD_TYPE_MASK)==MOD_TYPE_DLL;
    parse_objects(&x,&rt,!dll);parse_import_modules(&x);module_resources(&rt,&x,1);scan_fixups(&x);resolve_imports(&rt,&x);apply_fixups(&x,rt.ram);install_c386_helpers(&x,&rt);loader_path=NULL;
    fprintf(stderr,"soft386 CHECK: %s pages=%u objects=%u entry=%08X stack=%08X imports=%u\n",x.is_lx?"LX":"LE",x.num_pages,x.num_objects,
            x.entry_object?x.objects[x.entry_object-1].mapped_addr+x.entry_offset:0,x.stack_object?x.objects[x.stack_object-1].mapped_addr+x.stack_offset:0,x.nimports);
    if(dll)fprintf(stderr,"soft386 CHECK: guest DLL; %u resource copies\n",rt.pm.nresources);
    for(i=0;i<x.nimports;++i){const char *name=x.modules[x.imports[i].module].name;uint32_t id=system_module_id(name);
        int missing=id>=HC_PMWIN&&id<=HC_HELPMGR&&!soft386_pm_api_name((id-HC_PMWIN)>>24,x.imports[i].ordinal);
        fprintf(stderr,"  %s.%u -> %08X (%u sites)%s\n",name,x.imports[i].ordinal,x.imports[i].address,x.imports[i].sites,missing?" [PM API not yet marshalled]":"");
    }
    soft386_pm_close(&rt.pm);free_guest_modules(&rt);free_le_image(&x);free(rt.ram);return 0;
}

static void usage(const char *a0)
{
    fprintf(stderr,"usage: %s [--check] [--trace-hc] [--trace-sched] [--trace-native] [DLL options] [--watch-mem ADDR:LEN] [--max-cycles N] program.exe [guest args...]\n",a0);
    fprintf(stderr,"  diagnostic: --watch-mem ADDR:LEN (1..256 bytes of guest RAM)\n");
    fprintf(stderr,"  DLL options: --doscalls-dll P --viocalls-dll P --kbdcalls-dll P --sesmgr-dll P\n");
    fprintf(stderr,"               --pmwin-dll P --pmgpi-dll P --pmctls-dll P --msg-dll P\n");
    fprintf(stderr,"               --pmshapi-dll P --helpmgr-dll P --quecalls-dll P\n");
    fprintf(stderr,"               --so32dll-dll P --tcp32dll-dll P\n");
    fprintf(stderr,"               --no-doscalls-dll --no-system-dlls (also disables PM)\n");
    fprintf(stderr,"  Win32 default: DOSCALLS is available normally; VIO/KBD/SESMGR DLLs are loaded only when the image imports them.\n");
}

int main(int argc,char **argv)
{
    struct Runtime rt; struct LeImage x; CPUI386_State st; uint32_t entry,stack_top,esp,env_va,arg_va,pgm_va; int rc,i,argi=1,check=0;
    memset(&rt,0,sizeof(rt));memset(&x,0,sizeof(x));rt.current_thread=-1;rt.alloc_next=GUEST_ALLOC_BASE;rt.stub_next=GUEST_STUB_BASE;rt.module_next=GUEST_MODULE_BASE;rt.next_tid=2;rt.max_cycles=100000000;rt.main_image=&x;
    soft386_pm_init(&rt.pm,&rt,pm_current_tid,invoke_guest);
    soft386_pm_set_scheduler_yield(&rt.pm,pm_modal_scheduler_yield);
    soft386_pm_set_thread_hostcall(&rt.pm,pm_thread_hostcall);
    rt.pm.veneer=pm_oldproc_veneer;rt.pm.code_address=pm_code_address;
#ifdef SOFT386_TEST_PROVIDER
    { extern void soft386_test_pm_provider(struct Soft386PmBridge *);soft386_test_pm_provider(&rt.pm); }
#endif
    {const char *envs[]={"SOFT386_PMWIN_DLL","SOFT386_PMGPI_DLL","SOFT386_PMCTLS_DLL","SOFT386_MSG_DLL","SOFT386_PMSHAPI_DLL","SOFT386_HELPMGR_DLL"};
     for(i=0;i<S386_PM_COUNT;i++)rt.pm.paths[i]=getenv(envs[i]);}
    rt.queue.path=getenv("SOFT386_QUECALLS_DLL");
    rt.net.paths[0]=getenv("SOFT386_SO32DLL_DLL");rt.net.paths[1]=getenv("SOFT386_TCP32DLL_DLL");
    rt.native_dos_path=getenv("SOFT386_DOSCALLS_DLL");if(rt.native_dos_path&&rt.native_dos_path[0])rt.native_dos_required=1;
    rt.native_vio_path=getenv("SOFT386_VIOCALLS_DLL");if(rt.native_vio_path&&rt.native_vio_path[0])rt.native_vio_required=1;
    rt.native_kbd_path=getenv("SOFT386_KBDCALLS_DLL");if(rt.native_kbd_path&&rt.native_kbd_path[0])rt.native_kbd_required=1;
    rt.native_ses_path=getenv("SOFT386_SESMGR_DLL");if(rt.native_ses_path&&rt.native_ses_path[0])rt.native_ses_required=1;
    if(getenv("SOFT386_NO_DOSCALLS"))rt.native_dos_disabled=1;
    if(getenv("SOFT386_NO_SYSTEM_DLLS"))rt.native_sys_disabled=1;
    while(argi<argc&&argv[argi][0]=='-'){
        if(!strcmp(argv[argi],"--check")){check=1;++argi;}
        else if(!strcmp(argv[argi],"--trace-hc")){rt.trace_hc=1;++argi;}
        else if(!strcmp(argv[argi],"--trace-sched")){rt.trace_sched=1;++argi;}
        else if(!strcmp(argv[argi],"--trace-native")){rt.native_dos.trace=1;rt.native_sys.trace=1;rt.pm.trace=1;rt.queue.trace=1;rt.net.trace=1;++argi;}
        else if(!strcmp(argv[argi],"--run")){rt.quiet=0;++argi;}
        else if(!strcmp(argv[argi],"--run-quiet")){rt.quiet=1;++argi;}
        else if(!strcmp(argv[argi],"--argv0")&&argi+1<argc){rt.guest_argv0=argv[argi+1];argi+=2;}
        else if(!strcmp(argv[argi],"--doscalls-dll")&&argi+1<argc){rt.native_dos_path=argv[argi+1];rt.native_dos_required=1;argi+=2;}
        else if(!strcmp(argv[argi],"--viocalls-dll")&&argi+1<argc){rt.native_vio_path=argv[argi+1];rt.native_vio_required=1;argi+=2;}
        else if(!strcmp(argv[argi],"--kbdcalls-dll")&&argi+1<argc){rt.native_kbd_path=argv[argi+1];rt.native_kbd_required=1;argi+=2;}
        else if(!strcmp(argv[argi],"--sesmgr-dll")&&argi+1<argc){rt.native_ses_path=argv[argi+1];rt.native_ses_required=1;argi+=2;}
        else if(!strcmp(argv[argi],"--pmwin-dll")&&argi+1<argc){rt.pm.paths[0]=argv[argi+1];argi+=2;}
        else if(!strcmp(argv[argi],"--pmgpi-dll")&&argi+1<argc){rt.pm.paths[1]=argv[argi+1];argi+=2;}
        else if(!strcmp(argv[argi],"--pmctls-dll")&&argi+1<argc){rt.pm.paths[2]=argv[argi+1];argi+=2;}
        else if(!strcmp(argv[argi],"--msg-dll")&&argi+1<argc){rt.pm.paths[3]=argv[argi+1];argi+=2;}
        else if(!strcmp(argv[argi],"--pmshapi-dll")&&argi+1<argc){rt.pm.paths[4]=argv[argi+1];argi+=2;}
        else if(!strcmp(argv[argi],"--helpmgr-dll")&&argi+1<argc){rt.pm.paths[5]=argv[argi+1];argi+=2;}
        else if(!strcmp(argv[argi],"--quecalls-dll")&&argi+1<argc){rt.queue.path=argv[argi+1];argi+=2;}
        else if(!strcmp(argv[argi],"--so32dll-dll")&&argi+1<argc){rt.net.paths[0]=argv[argi+1];argi+=2;}
        else if(!strcmp(argv[argi],"--tcp32dll-dll")&&argi+1<argc){rt.net.paths[1]=argv[argi+1];argi+=2;}
        else if(!strcmp(argv[argi],"--no-doscalls-dll")){rt.native_dos_disabled=1;++argi;}
        else if(!strcmp(argv[argi],"--no-system-dlls")){rt.native_sys_disabled=1;++argi;}
        else if(!strcmp(argv[argi],"--watch-mem")&&argi+1<argc){
            char *end=NULL,*sep;unsigned long a,n;
            a=strtoul(argv[argi+1],&end,0);sep=end;
            if(!sep||*sep!=':'){fprintf(stderr,"soft386: --watch-mem expects ADDR:LEN\n");return 2;}
            n=strtoul(sep+1,&end,0);
            if(!end||*end||!n||n>256u||a>=RAM_SIZE||n>RAM_SIZE-a){fprintf(stderr,"soft386: invalid --watch-mem range\n");return 2;}
            rt.watch_addr=(uint32_t)a;rt.watch_len=(unsigned)n;argi+=2;
        }
        else if(!strcmp(argv[argi],"--max-cycles")&&argi+1<argc){rt.max_cycles=strtol(argv[argi+1],NULL,0);argi+=2;}
        else if(!strcmp(argv[argi],"--help")){usage(argv[0]);return 0;}
        else break;
    }
    if(argi>=argc){usage(argv[0]);return 2;}
    if(check)return check_image(argv[argi]);
    rt.pm.disabled=rt.native_sys_disabled;rt.queue.disabled=rt.native_sys_disabled;rt.net.disabled=rt.native_sys_disabled;
    publish_child_bootstrap(&rt,argv[0]);
    /* Keep argv[1] semantics used by build_startup_area by compacting options away. */
    if(argi!=1){for(i=argi;i<argc;++i)argv[i-argi+1]=argv[i];argc-=argi-1;}
    rt.ram=calloc(1,RAM_SIZE);if(!rt.ram)fatal("cannot allocate guest RAM");
#ifdef _WIN32
    os2_nls_win32_init_session(&rt.nls_state);
#else
    os2_nls_state_init(&rt.nls_state);
#endif
    {
        int trace_native = rt.native_dos.trace || rt.native_sys.trace;
        if (!rt.native_dos_disabled) {
            int loaded = soft386_doscalls_bridge_open(&rt.native_dos, rt.native_dos_path, rt.native_dos_required, trace_native);
            if (rt.native_dos_required && !loaded) { free(rt.ram); return 2; }
        }
    }
    if(!host_full_path(argv[1],rt.main_path,sizeof(rt.main_path)))snprintf(rt.main_path,sizeof(rt.main_path),"%s",argv[1]);
    x.file=load_file(argv[1],&x.file_size);if(!x.file){fprintf(stderr,"soft386: cannot read %s\n",argv[1]);free(rt.ram);return 2;}
    loader_path=argv[1];parse_header(&x);if((x.module_flags&MOD_TYPE_MASK)==MOD_TYPE_DLL)fatal("main image is a DLL");
    parse_objects(&x,&rt,1);parse_import_modules(&x);module_resources(&rt,&x,1);
    if(!rt.native_sys_disabled){
        uint32_t sysmask=image_system_mask(&x);
        int trace_native=rt.native_dos.trace||rt.native_sys.trace;
        int ok=soft386_system_bridge_open_mask(&rt.native_sys,sysmask,
            rt.native_vio_path,rt.native_vio_required,
            rt.native_kbd_path,rt.native_kbd_required,
            rt.native_ses_path,rt.native_ses_required,trace_native);
        if(!ok){soft386_doscalls_bridge_close(&rt.native_dos);free_le_image(&x);free(rt.ram);return 2;}
        if(trace_native)fprintf(stderr,"soft386: image system-module mask=%u (VIO=%s KBD=%s SES=%s)\n",
            (unsigned)sysmask,(sysmask&SOFT386_SYS_VIO)?"yes":"no",
            (sysmask&SOFT386_SYS_KBD)?"yes":"no",(sysmask&SOFT386_SYS_SES)?"yes":"no");
    }
    scan_fixups(&x);resolve_imports(&rt,&x);emit_runtime_stubs(&rt);apply_fixups(&x,rt.ram);install_c386_helpers(&x,&rt);loader_path=NULL;
    entry=x.objects[x.entry_object-1].mapped_addr+x.entry_offset;stack_top=x.objects[x.stack_object-1].mapped_addr+x.stack_offset;
    build_startup_area(rt.ram,argv[1],rt.guest_argv0,argc,argv,&env_va,&arg_va,&pgm_va);if(x.telnet_profile&&tp_canonical_etc((char *)rt.ram+env_va,pgm_va-env_va)<0)fatal("invalid TELNETPM environment");
    init_process_info(&rt,env_va,arg_va,x.module_flags);init_gdt(&rt);
    esp=stack_top;esp-=4;wr32(rt.ram+esp,arg_va);esp-=4;wr32(rt.ram+esp,env_va);esp-=4;wr32(rt.ram+esp,0);esp-=4;wr32(rt.ram+esp,0);esp-=4;wr32(rt.ram+esp,0);
    rt.cpu=cpui386_new(3,(char*)rt.ram,RAM_SIZE,&rt.cb);if(!rt.cpu||!rt.cb)fatal("cpui386_new failed");
    cpui386_enable_fpu(rt.cpu);
    rt.cb->io=&rt;rt.cb->io_read8=io_read8;rt.cb->io_read16=io_read16;rt.cb->io_read32=io_read32;rt.cb->io_write8=io_write8;rt.cb->io_write16=io_write16;rt.cb->io_write32=io_write32;
    cpui386_reset_pm_flat(rt.cpu,entry);cpui386_get_state(rt.cpu,&st);
    st.gdt_base=GUEST_GDT;st.gdt_limit=GUEST_GDT_BYTES-1;st.ip=entry;st.next_ip=entry;st.flags=2;st.gpr[0]=entry;st.gpr[4]=esp;
    st.seg[CPUI386_SEG_CS]=0x08;st.seg[CPUI386_SEG_SS]=0x10;st.seg[CPUI386_SEG_DS]=0x10;st.seg[CPUI386_SEG_ES]=0x10;st.seg[CPUI386_SEG_FS]=0x10;st.seg[CPUI386_SEG_GS]=0x10;
    if(!cpui386_set_state(rt.cpu,&st))fatal("cannot install initial protected-mode state");
    rt.current_thread=0;rt.threads[0].tid=1;rt.threads[0].state=THREAD_RUNNING;rt.threads[0].stack_base=x.objects[x.stack_object-1].mapped_addr;rt.threads[0].stack_size=x.stack_offset;
    cpui386_get_state(rt.cpu,&rt.threads[0].regs);init_thread_info(&rt,0);rt.threads[0].regs_valid=1;if(!cpui386_set_state(rt.cpu,&rt.threads[0].regs))fatal("cannot install initial TIB/FS");
    if(!rt.quiet)fprintf(stderr,"Soft386 OS/2 I386-H2S - deterministic Win32 vessel teardown\n");
    if(!rt.quiet&&rt.watch_len)fprintf(stderr,"soft386: mem-watch range=%08X:%u\n",rt.watch_addr,rt.watch_len);
#ifdef _WIN32
    if(!rt.quiet)fprintf(stderr,"soft386: host=Win32 x86 pointer_bits=%u\n",(unsigned)(sizeof(void*)*8u));
#else
    if(!rt.quiet)fprintf(stderr,"soft386: host=POSIX pointer_bits=%u\n",(unsigned)(sizeof(void*)*8u));
#endif
    if(!rt.quiet){
        fprintf(stderr,"soft386: 80387/x87=ON (Tiny386 optional FPU)\n");
        fprintf(stderr,"soft386: native DOSCALLS bridge=%s\n",rt.native_dos.loaded?"ON":"OFF");
        fprintf(stderr,"soft386: native system bridges VIO=%s KBD=%s SES=%s\n",
            rt.native_sys.vio.loaded?"ON":"OFF",rt.native_sys.kbd.loaded?"ON":"OFF",rt.native_sys.ses.loaded?"ON":"OFF");
        fprintf(stderr,"soft386: input=%s type=%s entry=%08X initial_ESP=%08X objects=%u imports=%u FS=%04X\n",argv[1],x.is_lx?"LX":"LE",entry,esp,x.num_objects,x.nimports,(unsigned)rt.threads[0].regs.seg[CPUI386_SEG_FS]);
        fprintf(stderr,"---------------- guest begins ----------------\n");fflush(stderr);
    }
    rc=initialize_modules(&rt)?run_guest(&rt):295;
    if(rt.process_exited){if(!rt.process_forced){terminate_exit_list(&rt);terminate_modules(&rt);}rc=(int)rt.process_rc;}
    if(!rt.quiet){
        fprintf(stderr,"---------------- guest ended -----------------\n");
        fprintf(stderr,"soft386: termination=%s rc=%d cycles=%ld\n",rt.process_reason?rt.process_reason:(rc==124?"cycle limit":(rc?"runtime/initialization failure":"guest runner returned")),rc,cpui386_get_cycle(rt.cpu));
    }
#ifdef _WIN32
    /* This executable is a one-guest Win32 process vessel.  Once OS/2 process
     * termination is complete, do not walk residual native PM windows or
     * unload provider DLLs synchronously: DestroyWindow/FreeLibrary may enter
     * arbitrary host teardown paths while the guest is already dead.  Quiesce
     * every guest re-entry path, release jar-owned memory, then use the Win32
     * process-termination primitive to reclaim process-owned HWNDs, DLLs and
     * any still-blocked native worker threads deterministically. */
    if(!rt.quiet){fprintf(stderr,"soft386: cleanup enter net-quiesce\n");fflush(stderr);}
    soft386_net_quiesce_process(&rt.net);
    if(!rt.quiet){fprintf(stderr,"soft386: cleanup leave net-quiesce pending=%d\n",rt.net.pending_on_close);fflush(stderr);}
    if(!rt.quiet){fprintf(stderr,"soft386: cleanup enter pm-quiesce\n");fflush(stderr);}
    soft386_pm_quiesce_process(&rt.pm);
    if(!rt.quiet){fprintf(stderr,"soft386: cleanup leave pm-quiesce\n");fflush(stderr);}
    if(!rt.quiet){fprintf(stderr,"soft386: cleanup enter guest-memory\n");fflush(stderr);}
    cpui386_delete(rt.cpu);rt.cpu=NULL;
    free_guest_modules(&rt);
    free_le_image(&x);
    free(rt.ram);rt.ram=NULL;
    if(!rt.quiet){fprintf(stderr,"soft386: cleanup leave guest-memory\n");fprintf(stderr,"soft386: vessel exit rc=%d\n",rc);fflush(stderr);}
    ExitProcess((UINT)rc);
    return rc; /* not reached */
#else
    if(!rt.quiet){fprintf(stderr,"soft386: cleanup enter net\n");fflush(stderr);}
    soft386_net_close(&rt.net);
    if(!rt.quiet){fprintf(stderr,"soft386: cleanup leave net pending=%d\n",rt.net.pending_on_close);fflush(stderr);}
    if(!rt.quiet){fprintf(stderr,"soft386: cleanup enter pm\n");fflush(stderr);}
    soft386_pm_close(&rt.pm);
    if(!rt.quiet){fprintf(stderr,"soft386: cleanup leave pm\n");fflush(stderr);}
    soft386_queue_close(&rt.queue);
    cpui386_delete(rt.cpu);
    free_guest_modules(&rt);
    soft386_system_bridge_close(&rt.native_sys);
    if(!rt.net.pending_on_close)soft386_doscalls_bridge_close(&rt.native_dos);
    free_le_image(&x);free(rt.ram);
    if(!rt.quiet){fprintf(stderr,"soft386: vessel return rc=%d\n",rc);fflush(stderr);}
    return rc;
#endif
}
