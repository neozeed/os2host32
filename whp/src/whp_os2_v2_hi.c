/*
 * whp_os2_v2_hi.c
 *
 * OS/2 V2 experimental runtime: execute untouched flat 32-bit OS/2 LE/LX
 * code under Windows Hypervisor Platform from a Win64 host.
 *
 * Milestone 3 links the loader-neutral OS/2 personality core also used by
 * native DOSCALLS.DLL.  Ordinary shared APIs receive explicit 32-bit guest
 * addresses through a bounded WHP adapter; scheduler, process, callback, and
 * synchronization operations remain WHP intrinsics.
 *
 * The loader also retains the proven LE/LX object mapper, fixups, synthetic
 * ordinal veneers, guest DLL loading, saved guest-thread contexts, file/path
 * backends, synchronization, console, PM/GPI, and callback machinery from the
 * earlier WHP revisions.
 *
 * Build from an x64 Visual Studio Developer Command Prompt:
 *
 *   make
 *
 * Run:
 *
 *   whp_os2_v2_hi.exe hi.exe
 *
 * Guest hypercall ABI:
 *
 *   EAX = 0x01000000 | ordinal    DOSCALLS ordinal
 *   OUT 0xF0,EAX
 *
 * At the OUT exit, the synthetic stub has not touched ESP. Therefore:
 *
 *   [ESP+0]  return EIP
 *   [ESP+4]  argument 1
 *   [ESP+8]  argument 2
 *   ...
 *
 * The host writes the OS/2 API return code to guest EAX, advances EIP past
 * OUT, and resumes. The stub RET then returns to the untouched application.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <WinHvPlatform.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <io.h>
#include <fcntl.h>

#include "os2_api_catalog.h"
#include "os2_doscalls_core.h"
#include "os2_win32_services.h"

#pragma comment(lib, "WinHvPlatform.lib")

#define RAM_SIZE            0x04000000u  /* 64 MiB */
#define GUEST_GDT           0x00007000u
#define GUEST_STARTUP       0x00E00000u
#define GUEST_STARTUP_LIMIT 0x00E20000u
#define GUEST_INFO          0x00E20000u
#define GUEST_INFO_LIMIT    0x00E23000u
#define GUEST_GDT_BYTES     ((3u + MAX_THREADS) * 8u)
#define GUEST_STUB_BASE     0x000F0000u
#define GUEST_STUB_LIMIT    0x000F8000u
#define GUEST_ALLOC_BASE    0x01000000u
#define GUEST_ALLOC_LIMIT   0x03000000u  /* leave high RAM for guest DLLs */
#define GUEST_MODULE_BASE   0x03000000u
#define GUEST_MODULE_LIMIT  0x03F00000u
#define HOSTCALL_PORT       0x00F0u
#define HC_DOSCALLS         0x01000000u
#define HC_QUECALLS         0x03000000u
#define HC_WHPTEST          0x04000000u
#define HC_PMGPI           0x06000000u
#define HC_PMWIN            0x05000000u
#define HC_RUNTIME_CALLBACK_RETURN 2u
#define GUEST_CALLBACK_RETURN_STUB 0x000F7FE0u
#define HC_RUNTIME          0x02000000u
#define HC_RUNTIME_THREAD_RETURN 1u
#define GUEST_THREAD_EXIT_STUB  0x000F7FF0u

#define MAX_OBJECTS         32u
#define MAX_PAGES           65535u
#define MAX_IMPORTS         512u
#define MAX_NAME_IMPORTS    512u
#define MAX_MODULES         32u
#define MAX_GUEST_MODULES   16u
#define MAX_SEARCHES        32u
#define MAX_FILES           128u
#define MAX_ALLOCS          256u
#define MAX_THREADS         32u
#define MAX_EVENT_SEMS      64u
#define EVENT_NAME_MAX      127u
#define THREAD_REG_COUNT    16u

#define SRC_MASK            0x0f
#define SRC_SEL16 2u
#define SRC_PTR16 3u
#define SRC_PTR32 6u
#define HC_CONSOLE 0x07000000u
#define HC_SESMGR 0x08000000u
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

/* Guest OS/2 status codes: keep independent of Windows SDK macros. */
#define OS2_NO_ERROR                    0u
#define OS2_ERROR_INVALID_FUNCTION      1u
#define OS2_ERROR_INVALID_HANDLE        6u
#define OS2_ERROR_NOT_ENOUGH_MEMORY     8u
#define OS2_ERROR_INVALID_PARAMETER     87u
#define OS2_ERROR_SEM_TIMEOUT           121u
#define OS2_ERROR_SEM_NOT_FOUND         187u
#define OS2_ERROR_DUPLICATE_NAME        285u
#define OS2_ERROR_TOO_MANY_OPENS        291u
#define OS2_ERROR_THREAD_NOT_TERMINATED 294u
#define OS2_ERROR_ALREADY_POSTED        299u
#define OS2_ERROR_ALREADY_RESET         300u
#define OS2_ERROR_INVALID_THREADID      309u

#define EXIT_THREAD                  0u
#define EXIT_PROCESS                 1u
#define DCWW_WAIT                    0u
#define DCWW_NOWAIT                  1u
#define SEM_INDEFINITE_WAIT          0xffffffffu

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

struct GuestAlloc {
    uint32_t base;
    uint32_t size;
    int used;
};

enum GuestThreadState {
    THREAD_FREE = 0,
    THREAD_RUNNABLE,
    THREAD_RUNNING,
    THREAD_WAIT_THREAD,
    THREAD_WAIT_EVENT,
    THREAD_SLEEP,
    THREAD_WAIT_MUTEX,
    THREAD_WAIT_QUEUE,
    THREAD_WAIT_PM,
    THREAD_WAIT_KBD,
    THREAD_WAIT_PROCESS,
    THREAD_SUSPENDED,
    THREAD_DEAD
};

struct Runtime;
struct GuestCallback {
    struct GuestCallback *previous;
    WHV_REGISTER_VALUE caller[THREAD_REG_COUNT];
    uint32_t result_out, entry_sp;
    uint32_t (*complete)(struct Runtime *, uint32_t);
};

struct GuestThread {
    uint32_t tid;
    enum GuestThreadState state;
    uint32_t stack_base;
    uint32_t stack_size;
    uint32_t wait_tid;
    uint32_t wait_ptid;
    uint32_t wait_event;
    uint64_t wait_deadline_ms;
    uint64_t wait_order;
    uint32_t queue_args[8];
    int queue_peek;
    int owns_stack;
    struct GuestCallback *callbacks;
    uint32_t callback_depth;
    WHV_REGISTER_VALUE regs[THREAD_REG_COUNT];
    int regs_valid;
};


struct GuestEventSem {
    int used;
    uint32_t generation;
    uint32_t refs;
    uint32_t post_count;
    char name[EVENT_NAME_MAX + 1];
};

struct GuestModule {
    int used;
    int state;              /* 1=loading, 2=ready */
    uint32_t handle;
    char request_name[64];
    char path[MAX_PATH];
    struct LeImage image;
};

struct HostStub {
    uint32_t ordinal;
    uint32_t address;
};

struct GuestSearch {
    int active, pending, ended;
    uint32_t id, filter;
    HANDLE native;
    WIN32_FIND_DATAA data;
};

#include "v2_sync_types.h"

struct Runtime {
    WHV_PARTITION_HANDLE partition;
    struct GuestPM *pm;
    struct GuestGPI *gpi;
    uint8_t *ram;
    struct GuestAlloc allocs[MAX_ALLOCS];
    HANDLE files[MAX_FILES];
    int std_closed[3];
    struct GuestSearch searches[MAX_SEARCHES];
    uint32_t next_search_id;
    uint32_t alloc_next;

    struct GuestThread threads[MAX_THREADS];
    struct GuestEventSem events[MAX_EVENT_SEMS];
    struct GuestMutex mutexes[MAX_MUTEXES];
    struct GuestQueue queues[MAX_QUEUES];
    uint64_t wait_serial;
    struct GuestModule modules[MAX_GUEST_MODULES];
    struct HostStub doscall_stubs[MAX_IMPORTS];
    uint32_t module_count;
    uint32_t module_next;
    uint32_t stub_next;
    uint32_t next_module_handle;
    uint32_t next_tid;
    int current_thread;
    int switch_requested;
    int current_thread_exited;

    int process_exited;
    uint32_t process_rc;
};

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

__declspec(noreturn) static void fatal(const char *what)
{
    fprintf(stderr, "v2: %s\n", what);
    exit(1);
}

static int failed(const char *what, HRESULT hr)
{
    fprintf(stderr, "v2: %s failed: HRESULT=0x%08lX\n",
            what, (unsigned long)hr);
    return 1;
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
        (x->file[x->le + 1] != 'E' && x->file[x->le + 1] != 'X'))
        fatal("input is not an LE/LX executable");
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

static void parse_objects(struct LeImage *x, struct Runtime *rt, int is_main)
{
    uint32_t i, j, off, phys, src, room, actual, last_map;
    uint32_t min_addr, max_end, span, module_base;
    uint8_t *m;
    uint16_t pflags;

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

        if (!(o->flags & OBJ_BIG) && (!(o->flags & OBJ_EXEC) || o->size > 65536u))
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
            if (o->size != 0 &&
                (o->mapped_addr >= RAM_SIZE || o->size > RAM_SIZE - o->mapped_addr))
                fatal("main LE/LX object lies outside guest RAM");
            if (o->size &&
                ((o->addr < GUEST_GDT + GUEST_GDT_BYTES && o->addr + o->size > GUEST_GDT) ||
                 (o->addr < GUEST_INFO_LIMIT && o->addr + o->size > GUEST_STARTUP) ||
                 (o->addr < GUEST_STUB_LIMIT && o->addr + o->size > GUEST_STUB_BASE) ||
                 o->addr + o->size > GUEST_ALLOC_BASE))
                fatal("main object overlaps reserved V2 runtime memory");
            for (j = 0; j < i; ++j) {
                struct LeObject *prev = &x->objects[j];
                if (o->size && prev->size && o->addr < prev->addr + prev->size &&
                    prev->addr < o->addr + o->size)
                    fatal("overlapping main objects");
            }
        }
    } else {
        span = align_up(max_end - min_addr, 0x10000u);
        module_base = align_up(rt->module_next, 0x10000u);
        if (span == 0)
            span = 0x10000u;
        if (module_base >= GUEST_MODULE_LIMIT || span > GUEST_MODULE_LIMIT - module_base)
            fatal("guest DLL arena exhausted");
        for (i = 0; i < x->num_objects; ++i) {
            struct LeObject *o = &x->objects[i];
            o->mapped_addr = module_base + (o->addr - min_addr);
            if (o->size != 0 &&
                (o->mapped_addr >= RAM_SIZE || o->size > RAM_SIZE - o->mapped_addr))
                fatal("guest DLL object lies outside guest RAM");
        }
        rt->module_next = module_base + span;
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
            if (x->pages[i].data_size > x->page_size)
                fatal("LX page data exceeds page size");
            if (pflags == 0 && x->pages[i].data_size != 0) {
                offset = (uint64_t)x->data_pages_off +
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
        } else if (pflags != 3) {
            fatal("V2 supports enumerated/zero LE/LX pages only");
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

static const char *import_name_at(struct LeImage *x, uint32_t off,
                                  char *buf, uint32_t cap)
{
    uint32_t p, n;
    if (cap == 0)
        return "";
    buf[0] = 0;
    p = x->impproc_off + off;
    if (!file_range(x, p, 1))
        return "<bad-name-offset>";
    n = x->file[p++];
    if (n >= cap || !file_range(x, p, n))
        return "<bad-name>";
    memcpy(buf, x->file + p, n);
    buf[n] = 0;
    return buf;
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
                if (st != SRC_OFF32 && st != SRC_SEL16 && st != SRC_PTR32)
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
                if (st != SRC_REL32 && st != SRC_PTR16)
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
                if (st != SRC_REL32)
                    fatal("external named fixup is not REL32");
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

#include "v2_far16.h"

static uint32_t hostcall_stub(struct Runtime *rt, uint32_t module, uint32_t ordinal)
{
    uint32_t i, va, id;
    uint8_t *s;

    /* C/386 register ABI: keep the original flat guest token in EAX. */
    if(module==HC_DOSCALLS && ordinal==425) {
        va=rt->stub_next++;if(va>=GUEST_STUB_LIMIT)fatal("stub overflow");
        rt->ram[va]=0xc3;return va;
    }
    id = module | (ordinal & 0x00ffffffu);
    for (i = 0; i < MAX_IMPORTS; ++i) {
        if (rt->doscall_stubs[i].address != 0 &&
            rt->doscall_stubs[i].ordinal == id)
            return rt->doscall_stubs[i].address;
    }
    for (i = 0; i < MAX_IMPORTS; ++i) {
        if (rt->doscall_stubs[i].address == 0)
            break;
    }
    if (i == MAX_IMPORTS)
        fatal("too many shared DOSCALLS veneers");

    va = align_up(rt->stub_next, 8u);
    if (va + 8u > GUEST_STUB_LIMIT)
        fatal("synthetic import veneer overflow");
    s = rt->ram + va;
    s[0] = 0xB8; wr32(s + 1, id);      /* mov eax,imm32 */
    s[5] = 0xE7; s[6] = 0xF0;          /* out 0f0h,eax */
    s[7] = 0xC3;                       /* ret */

    rt->doscall_stubs[i].ordinal = id;
    rt->doscall_stubs[i].address = va;
    rt->stub_next = va + 8u;
    return va;
}

static void apply_fixups(struct LeImage *x, uint8_t *ram)
{
    uint32_t physical, start, endoff, p, end, q, first, target, target_off;
    uint32_t count, i, src_obj_index, source_off, src_va, dst_va, idx;
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
            target_off = 0;

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
                if (flags & TGT_ADDITIVE)
                    q += (flags & TGT_ADD32) ? 4u : 2u;
            } else if (kind == TGT_EXT_NAME) {
                if (flags & TGT_OFF32) {
                    target = rd32(x->file + q); q += 4;
                } else {
                    target = rd16(x->file + q); q += 2;
                }
                if (flags & TGT_ADDITIVE)
                    q += (flags & TGT_ADD32) ? 4u : 2u;
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
                    wr32(ram + src_va, dst_va);
                } else if (kind == TGT_EXT_ORD) {
                    idx = import_index(x, first - 1u, target);
                    dst_va = x->imports[idx].address;
                    if (dst_va == 0)
                        fatal("unresolved ordinal import reached fixup pass");
                    wr32(ram + src_va, dst_va - (src_va + 4u));
                } else {
                    idx = name_import_index(x, first - 1u, target);
                    dst_va = x->name_imports[idx].address;
                    if (dst_va == 0)
                        fatal("unresolved named import reached fixup pass");
                    wr32(ram + src_va, dst_va - (src_va + 4u));
                }
            }
        }
    }
}

/* Phase-1 offline loader check: relocation targets below are diagnostic only.
   Never enter WHP or claim PM/queue implementations from this path. */
static int check_image(const char *path)
{
    struct LeImage x;
    struct Runtime rt;
    const struct Os2ApiDescriptor *api;
    uint32_t i;
    memset(&x, 0, sizeof(x));
    memset(&rt, 0, sizeof(rt));
    x.file = load_file(path, &x.file_size);
    if (!x.file) { fprintf(stderr, "cannot read %s\n", path); return 1; }
    rt.ram = (uint8_t *)calloc(1, RAM_SIZE);
    if (!rt.ram) fatal("out of memory for loader check");
    rt.module_next = GUEST_MODULE_BASE;
    parse_header(&x);
    parse_objects(&x, &rt, (x.module_flags & MOD_TYPE_MASK) != MOD_TYPE_DLL);
    parse_import_modules(&x);
    scan_fixups(&x);
    fprintf(stderr, "%s loader check: %u pages, %u objects\n", x.is_lx ? "LX" : "LE", x.num_pages, x.num_objects);
    for (i = 0; i < x.num_objects; ++i)
        fprintf(stderr, "object %u: guest=%08X size=%08X pages=%u @%u\n", i + 1u,
               x.objects[i].mapped_addr, x.objects[i].size, x.objects[i].mapsize, x.objects[i].mapidx);
    fprintf(stderr, "fixups: %u internal records / %u sites; %u external sites\n",
           x.internal_records, x.internal_sites, x.external_sites);
    for (i = 0; i < x.nimports; ++i) {
        const char *module_name;
        module_name = x.modules[x.imports[i].module].name;
        api = os2_api_lookup(module_name, x.imports[i].ordinal);
        if (api != NULL)
            fprintf(stderr, "  %s.%u %s [%s] (%u sites)\n",
                    module_name, x.imports[i].ordinal, api->api_name,
                    os2_api_route_name(api->route), x.imports[i].sites);
        else
            fprintf(stderr, "  %s.%u [uncatalogued] (%u sites)\n",
                    module_name, x.imports[i].ordinal, x.imports[i].sites);
        x.imports[i].address = GUEST_STUB_BASE + i * 8u;
    }
    for (i = 0; i < x.nname_imports; ++i)
        x.name_imports[i].address = GUEST_STUB_BASE + (x.nimports + i) * 8u;
    rt.stub_next=GUEST_STUB_BASE+(x.nimports+x.nname_imports)*8u;
    apply_fixups(&x, rt.ram);
    install_far16(&x, &rt);
    fprintf(stderr, "PASS: page mapping and fixup application; %u ordinal + %u named imports.\n",
           x.nimports, x.nname_imports);
    fprintf(stderr, "CHECK ONLY: import addresses are placeholders; no guest code executed.\n");
    free(x.owner); free(x.pages); free(x.file); free(rt.ram);
    return 0;
}

static int module_name_equal(const char *a, const char *b)
{
    char aa[64], bb[64];
    const char *pa, *pb;
    size_t na, nb;

    pa = a;
    pb = b;
    while (*a) { if (*a == '\\' || *a == '/') pa = a + 1; ++a; }
    while (*b) { if (*b == '\\' || *b == '/') pb = b + 1; ++b; }
    na = strlen(pa); nb = strlen(pb);
    if (na >= 4 && _stricmp(pa + na - 4, ".DLL") == 0) na -= 4;
    if (nb >= 4 && _stricmp(pb + nb - 4, ".DLL") == 0) nb -= 4;
    if (na >= sizeof(aa) || nb >= sizeof(bb))
        return 0;
    memcpy(aa, pa, na); aa[na] = 0;
    memcpy(bb, pb, nb); bb[nb] = 0;
    return _stricmp(aa, bb) == 0;
}

static int file_exists_regular(const char *path)
{
    FILE *f;
    f = fopen(path, "rb");
    if (!f)
        return 0;
    fclose(f);
    return 1;
}

static void module_leaf_name(const char *name, char *leaf, uint32_t cap)
{
    const char *p;
    size_t n;
    int has_ext;
    if (cap == 0)
        return;
    p = name;
    while (*name) {
        if (*name == '\\' || *name == '/')
            p = name + 1;
        ++name;
    }
    n = strlen(p);
    has_ext = (n >= 4 && _stricmp(p + n - 4, ".DLL") == 0);
    if (n + (has_ext ? 1u : 5u) > cap)
        fatal("guest DLL name is too long");
    strcpy(leaf, p);
    if (!has_ext)
        strcat(leaf, ".DLL");
}

static int locate_guest_module(const char *name, char *out, uint32_t cap)
{
    char leaf[MAX_PATH];
    char path[MAX_PATH];
    const char *env, *p, *q;
    size_t n;

    module_leaf_name(name, leaf, (uint32_t)sizeof(leaf));

    if (strchr(name, '\\') || strchr(name, '/') || strchr(name, ':')) {
        if (strlen(name) + 1u < sizeof(path)) {
            strcpy(path, name);
            if (!strrchr(path, '.'))
                strcat(path, ".DLL");
            if (file_exists_regular(path)) {
                if (strlen(path) + 1u > cap) return 0;
                strcpy(out, path);
                return 1;
            }
        }
    }

    if (file_exists_regular(leaf)) {
        if (strlen(leaf) + 1u > cap) return 0;
        strcpy(out, leaf);
        return 1;
    }

    env = getenv("OS2LIBPATH");
    if (!env || !*env)
        return 0;
    p = env;
    while (*p) {
        q = strchr(p, ';');
        n = q ? (size_t)(q - p) : strlen(p);
        if (n == 0) {
            strcpy(path, leaf);
        } else if (n + 1u + strlen(leaf) + 1u <= sizeof(path)) {
            memcpy(path, p, n);
            path[n] = 0;
            if (path[n - 1] != '\\' && path[n - 1] != '/')
                strcat(path, "\\");
            strcat(path, leaf);
        } else {
            path[0] = 0;
        }
        if (path[0] && file_exists_regular(path)) {
            if (strlen(path) + 1u > cap) return 0;
            strcpy(out, path);
            return 1;
        }
        if (!q) break;
        p = q + 1;
    }
    return 0;
}

static uint32_t name_table_find_ordinal(struct LeImage *x, uint32_t start,
                                        uint32_t limit, const char *wanted)
{
    uint32_t p, n, ord;
    char name[256];
    if (start == 0 || start >= x->file_size)
        return 0;
    if (limit == 0 || limit > x->file_size)
        limit = x->file_size;
    p = start;
    while (p < limit) {
        n = x->file[p++];
        if (n == 0)
            break;
        if (n >= sizeof(name) || p > limit || n > limit - p ||
            limit - (p + n) < 2u)
            fatal("malformed LE export name table");
        memcpy(name, x->file + p, n);
        name[n] = 0;
        p += n;
        ord = rd16(x->file + p);
        p += 2;
        if (_stricmp(name, wanted) == 0)
            return ord;
    }
    return 0;
}

static uint32_t guest_export_by_ordinal(struct GuestModule *g, uint32_t wanted)
{
    struct LeImage *x;
    uint32_t p, ordinal, i, object, off;
    uint8_t count, type, base_type, flags;

    x = &g->image;
    if (wanted == 0 || x->entry_table_off == 0 ||
        x->entry_table_off >= x->file_size)
        return 0;
    p = x->entry_table_off;
    ordinal = 1;
    for (;;) {
        if (!file_range(x, p, 1))
            fatal("truncated LE entry table");
        count = x->file[p++];
        if (count == 0)
            break;
        if (!file_range(x, p, 1))
            fatal("truncated LE entry bundle");
        type = x->file[p++];
        if (type & 0x80)
            fatal("typed LE entry bundles are unsupported in V2 DLL milestone");
        base_type = type & 0x7f;
        if (base_type == 0) {
            if (wanted >= ordinal && wanted < ordinal + (uint32_t)count)
                return 0;
            ordinal += count;
            continue;
        }
        if (base_type == 1 || base_type == 2 || base_type == 3) {
            if (!file_range(x, p, 2))
                fatal("truncated LE entry bundle object");
            object = rd16(x->file + p); p += 2;
            for (i = 0; i < (uint32_t)count; ++i, ++ordinal) {
                if (!file_range(x, p, 1))
                    fatal("truncated LE entry flags");
                flags = x->file[p++];
                if (base_type == 3) {
                    if (!file_range(x, p, 4))
                        fatal("truncated LE 32-bit entry");
                    off = rd32(x->file + p); p += 4;
                } else {
                    if (!file_range(x, p, base_type == 2 ? 4u : 2u))
                        fatal("truncated LE 16-bit entry");
                    off = rd16(x->file + p); p += 2;
                    if (base_type == 2) p += 2;
                }
                if (ordinal == wanted) {
                    if ((flags & 1u) == 0)
                        return 0;
                    if (base_type != 3)
                        fatal("16-bit/call-gate DLL exports are not supported in V2 DLL milestone");
                    if (object == 0 || object > x->num_objects)
                        fatal("DLL export references invalid object");
                    if (off >= x->objects[object - 1u].size)
                        fatal("DLL export offset lies outside object");
                    return x->objects[object - 1u].mapped_addr + off;
                }
            }
            continue;
        }
        if (base_type == 4)
            fatal("DLL forwarder exports are deferred in V2 DLL milestone");
        fatal("unknown LE entry bundle type");
    }
    return 0;
}

static uint32_t guest_export_by_name(struct GuestModule *g, const char *name)
{
    struct LeImage *x;
    uint32_t ord, resident_end, nonresident_end;
    x = &g->image;
    resident_end = x->entry_table_off;
    if (resident_end <= x->resident_name_off || resident_end > x->file_size)
        resident_end = x->file_size;
    ord = name_table_find_ordinal(x, x->resident_name_off, resident_end, name);
    if (ord == 0 && x->nonresident_name_off != 0 && x->nonresident_name_len != 0) {
        if (x->nonresident_name_off > x->file_size ||
            x->nonresident_name_len > x->file_size - x->nonresident_name_off)
            fatal("bad non-resident name table");
        nonresident_end = x->nonresident_name_off + x->nonresident_name_len;
        ord = name_table_find_ordinal(x, x->nonresident_name_off,
                                      nonresident_end, name);
    }
    return ord ? guest_export_by_ordinal(g, ord) : 0;
}

static struct GuestModule *find_guest_module(struct Runtime *rt, const char *name)
{
    uint32_t i;
    for (i = 0; i < MAX_GUEST_MODULES; ++i) {
        if (rt->modules[i].used &&
            module_name_equal(rt->modules[i].request_name, name))
            return &rt->modules[i];
    }
    return NULL;
}

static struct GuestModule *load_guest_module(struct Runtime *rt, const char *name);

static void resolve_imports(struct Runtime *rt, struct LeImage *x,
                            const char *owner)
{
    uint32_t i, addr;
    struct GuestModule *g;
    char namebuf[256];

    for (i = 0; i < x->nimports; ++i) {
        const char *mod = x->modules[x->imports[i].module].name;
        const struct Os2ApiDescriptor *api;
        if (_stricmp(mod, "DOSCALLS") == 0) {
            addr = hostcall_stub(rt, HC_DOSCALLS, x->imports[i].ordinal);
        } else if (_stricmp(mod,"VIOCALLS")==0 || _stricmp(mod,"KBDCALLS")==0) {
            int d=far16_desc(mod,x->imports[i].ordinal);
            if(d<0)fatal("unsupported VIO/KBD import");
            addr=hostcall_stub(rt,HC_CONSOLE,(uint32_t)d+1);
        } else if (_stricmp(mod,"SESMGR")==0) {
            addr=hostcall_stub(rt,HC_SESMGR,x->imports[i].ordinal);
        } else if (_stricmp(mod, "PMGPI") == 0) {
            addr = hostcall_stub(rt, HC_PMGPI, x->imports[i].ordinal);
        } else if (_stricmp(mod, "PMWIN") == 0) {
            addr = hostcall_stub(rt, HC_PMWIN, x->imports[i].ordinal);
        } else if (_stricmp(mod, "WHPTEST") == 0) {
            addr = hostcall_stub(rt, HC_WHPTEST, x->imports[i].ordinal);
        } else if (_stricmp(mod, "QUECALLS") == 0) {
            addr = hostcall_stub(rt, HC_QUECALLS, x->imports[i].ordinal);
        } else {
            g = load_guest_module(rt, mod);
            addr = guest_export_by_ordinal(g, x->imports[i].ordinal);
        }
        if (addr == 0) {
            fprintf(stderr, "v2: %s: unresolved %s.%u\n",
                    owner, mod, x->imports[i].ordinal);
            fatal("guest DLL ordinal import resolution failed");
        }
        x->imports[i].address = addr;
        api = os2_api_lookup(mod, x->imports[i].ordinal);
        if (api != NULL)
            fprintf(stderr,
                    "v2: resolve %-12s .%-4u %-24s [%-11s] -> %08X  (%u site%s) [%s]\n",
                    mod, x->imports[i].ordinal, api->api_name,
                    os2_api_route_name(api->route), addr,
                    x->imports[i].sites,
                    x->imports[i].sites == 1 ? "" : "s", owner);
        else
            fprintf(stderr,
                    "v2: resolve %-12s .%-4u %-24s [%-11s] -> %08X  (%u site%s) [%s]\n",
                    mod, x->imports[i].ordinal, "?", "uncatalogued", addr,
                    x->imports[i].sites,
                    x->imports[i].sites == 1 ? "" : "s", owner);
    }

    for (i = 0; i < x->nname_imports; ++i) {
        const char *mod = x->modules[x->name_imports[i].module].name;
        const char *proc = import_name_at(x, x->name_imports[i].name_offset,
                                          namebuf, (uint32_t)sizeof(namebuf));
        if (_stricmp(mod, "DOSCALLS") == 0) {
            fprintf(stderr, "v2: DOSCALLS named import %s is unsupported; OS/2 DOSCALLS is ordinal-only\n",
                    proc);
            fatal("named DOSCALLS import");
        }
        g = load_guest_module(rt, mod);
        addr = guest_export_by_name(g, proc);
        if (addr == 0) {
            fprintf(stderr, "v2: %s: unresolved %s.%s\n", owner, mod, proc);
            fatal("guest DLL named import resolution failed");
        }
        x->name_imports[i].address = addr;
        fprintf(stderr, "v2: resolve %-12s .%-20s -> %08X  (%u site%s) [%s]\n",
               mod, proc, addr, x->name_imports[i].sites,
               x->name_imports[i].sites == 1 ? "" : "s", owner);
    }
}

static struct GuestModule *load_guest_module(struct Runtime *rt, const char *name)
{
    uint32_t i;
    struct GuestModule *g;
    char path[MAX_PATH];

    g = find_guest_module(rt, name);
    if (g) {
        if (g->state == 1)
            fatal("cyclic guest DLL dependency is deferred in V2 DLL milestone");
        return g;
    }
    if (rt->module_count >= MAX_GUEST_MODULES)
        fatal("too many guest DLL modules");
    if (!locate_guest_module(name, path, (uint32_t)sizeof(path))) {
        fprintf(stderr, "v2: guest DLL %s not found (current directory or OS2LIBPATH)\n", name);
        fatal("guest DLL not found");
    }

    g = NULL;
    for (i = 0; i < MAX_GUEST_MODULES; ++i) {
        if (!rt->modules[i].used) {
            g = &rt->modules[i];
            break;
        }
    }
    if (!g)
        fatal("guest DLL registry full");
    memset(g, 0, sizeof(*g));
    g->used = 1;
    g->state = 1;
    g->handle = rt->next_module_handle++;
    strncpy(g->request_name, name, sizeof(g->request_name) - 1u);
    strncpy(g->path, path, sizeof(g->path) - 1u);
    ++rt->module_count;

    g->image.file = load_file(path, &g->image.file_size);
    if (!g->image.file)
        fatal("cannot read guest DLL");
    parse_header(&g->image);
    if ((g->image.module_flags & MOD_TYPE_MASK) != MOD_TYPE_DLL)
        fatal("imported guest module is not an OS/2 DLL");

    fprintf(stderr, "v2: GUESTDLL LOAD   %-12s -> %s handle=%08X\n",
           name, path, g->handle);
    parse_objects(&g->image, rt, 0);
    parse_import_modules(&g->image);
    scan_fixups(&g->image);
    resolve_imports(rt, &g->image, g->request_name);
    apply_fixups(&g->image, rt->ram);
    install_far16(&g->image, rt);
    g->state = 2;

    for (i = 0; i < g->image.num_objects; ++i) {
        fprintf(stderr, "v2: GUESTDLL object %-2u %-12s pref=%08X guest=%08X size=%08X flags=%08X\n",
               i + 1u, g->request_name, g->image.objects[i].addr,
               g->image.objects[i].mapped_addr, g->image.objects[i].size,
               g->image.objects[i].flags);
    }
    fprintf(stderr, "v2: GUESTDLL READY  %-12s imports=%u+%u sites=%u\n",
           g->request_name, g->image.nimports, g->image.nname_imports,
           g->image.external_sites);
    return g;
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

#include "v2_process_boot.h"

static uint32_t environment_size(const char *env)
{
    const char *p;
    p = env;
    while (p[0] != 0 || p[1] != 0)
        p += strlen(p) + 1;
    return (uint32_t)(p - env) + 2u;
}

static void build_startup_area(uint8_t *ram, const char *program,
                               int argc, char **argv,
                               uint32_t *env_va, uint32_t *arg_va,
                               uint32_t *pgm_va)
{
    LPCH winenv;
    uint32_t env_len, pgm_len, arg0_len, tail_len, total, pos, i, n;
    char full[MAX_PATH];
    DWORD full_len;
    const char *pgm;
    const char *arg0;

    if(child_boot) { child_startup(ram,env_va,arg_va,pgm_va);return; }
    arg0 = base_name(program);
    full_len = GetFullPathNameA(program, (DWORD)sizeof(full), full, NULL);
    pgm = (full_len != 0 && full_len < (DWORD)sizeof(full)) ? full : program;

    winenv = GetEnvironmentStringsA();
    if (!winenv)
        fatal("GetEnvironmentStringsA failed");
    env_len = environment_size(winenv);
    pgm_len = (uint32_t)strlen(pgm);
    arg0_len = (uint32_t)strlen(arg0);
    tail_len = 0;
    for (i = 2; i < (uint32_t)argc; ++i) {
        if (i != 2u) ++tail_len;
        tail_len += (uint32_t)strlen(argv[i]);
        if (arg_needs_quotes(argv[i])) tail_len += 2u;
    }

    total = env_len + pgm_len + 1u + arg0_len + 1u + tail_len + 2u;
    if (GUEST_STARTUP + total > GUEST_STARTUP_LIMIT) {
        FreeEnvironmentStringsA(winenv);
        fatal("startup environment does not fit reserved guest area");
    }

    memcpy(ram + GUEST_STARTUP, winenv, env_len);
    FreeEnvironmentStringsA(winenv);
    *env_va = GUEST_STARTUP;
    pos = env_len;

    *pgm_va = GUEST_STARTUP + pos;
    memcpy(ram + GUEST_STARTUP + pos, pgm, pgm_len);
    pos += pgm_len;
    ram[GUEST_STARTUP + pos++] = 0;

    *arg_va = GUEST_STARTUP + pos;
    memcpy(ram + GUEST_STARTUP + pos, arg0, arg0_len);
    pos += arg0_len;
    ram[GUEST_STARTUP + pos++] = 0;

    for (i = 2; i < (uint32_t)argc; ++i) {
        int quote;
        if (i != 2u)
            ram[GUEST_STARTUP + pos++] = ' ';
        quote = arg_needs_quotes(argv[i]);
        if (quote)
            ram[GUEST_STARTUP + pos++] = '"';
        n = (uint32_t)strlen(argv[i]);
        memcpy(ram + GUEST_STARTUP + pos, argv[i], n);
        pos += n;
        if (quote)
            ram[GUEST_STARTUP + pos++] = '"';
    }
    ram[GUEST_STARTUP + pos++] = 0;
    ram[GUEST_STARTUP + pos++] = 0;
    if (pos != total)
        fatal("internal startup-area size mismatch");
}

static int guest_range(uint32_t va, uint32_t cb)
{
    if (va >= RAM_SIZE)
        return 0;
    return cb <= RAM_SIZE - va;
}

static uint32_t guest_u32(struct Runtime *rt, uint32_t va)
{
    if (!guest_range(va, 4))
        fatal("guest stack/pointer read is outside RAM");
    return rd32(rt->ram + va);
}

static void guest_put_u32(struct Runtime *rt, uint32_t va, uint32_t v)
{
    if (!guest_range(va, 4))
        fatal("guest pointer write is outside RAM");
    wr32(rt->ram + va, v);
}

static int guest_copy_cstr(struct Runtime *rt, uint32_t va, char *out, uint32_t cap)
{
    uint32_t i;
    if (va == 0 || cap == 0)
        return 0;
    for (i = 0; i + 1u < cap; ++i) {
        if (!guest_range(va + i, 1))
            return 0;
        out[i] = (char)rt->ram[va + i];
        if (out[i] == '\0')
            return 1;
    }
    out[cap - 1u] = '\0';
    return 0;
}

#include "v2_fileio.h"
#include "v2_find.h"

static uint32_t alloc_guest(struct Runtime *rt, uint32_t size)
{
    uint32_t base, need, i;
    if (size == 0 || size > GUEST_ALLOC_LIMIT - GUEST_ALLOC_BASE)
        return 0;
    need = align_up(size, 0x1000u);

    /* First-fit reuse of freed blocks. */
    for (i = 0; i < MAX_ALLOCS; ++i) {
        if (!rt->allocs[i].used && rt->allocs[i].base != 0 && rt->allocs[i].size >= need) {
            rt->allocs[i].used = 1;
            memset(rt->ram + rt->allocs[i].base, 0, need);
            return rt->allocs[i].base;
        }
    }

    base = align_up(rt->alloc_next, 0x1000u);
    if (base >= GUEST_ALLOC_LIMIT || need > GUEST_ALLOC_LIMIT - base)
        return 0;
    for (i = 0; i < MAX_ALLOCS; ++i) {
        if (rt->allocs[i].base == 0) {
            rt->allocs[i].base = base;
            rt->allocs[i].size = need;
            rt->allocs[i].used = 1;
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
    for (i = 0; i < MAX_ALLOCS; ++i) {
        if (rt->allocs[i].base == base)
            return &rt->allocs[i];
    }
    return NULL;
}

/* WHP adapter for the loader-neutral OS/2 personality core. */
static os2_api_ret_t whp_validate_memory(void *opaque,
                                         os2_addr32_t address,
                                         uint32_t length,
                                         uint32_t access)
{
    struct Runtime *rt;
    (void)access;
    rt = (struct Runtime *)opaque;
    if (rt == NULL)
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    if (length != 0u && address == 0u)
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    return guest_range(address, length) ? OS2_PERSONALITY_NO_ERROR :
                                         OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
}

static os2_api_ret_t whp_write_memory(void *opaque,
                                      os2_addr32_t address,
                                      const void *source,
                                      uint32_t length)
{
    struct Runtime *rt;
    rt = (struct Runtime *)opaque;
    if (rt == NULL || (length != 0u && !guest_range(address, length)))
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    if (length != 0u)
        memcpy(rt->ram + address, source, (size_t)length);
    return OS2_PERSONALITY_NO_ERROR;
}

static os2_api_ret_t whp_map_read_memory(void *opaque,
                                         os2_addr32_t address,
                                         uint32_t length,
                                         const void **host_pointer)
{
    struct Runtime *rt;
    rt = (struct Runtime *)opaque;
    if (rt == NULL || host_pointer == NULL ||
        (length != 0u && !guest_range(address, length)))
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    *host_pointer = length != 0u ? (const void *)(rt->ram + address) : NULL;
    return OS2_PERSONALITY_NO_ERROR;
}

static os2_api_ret_t whp_query_handle_type(void *opaque,
                                           os2_handle32_t hfile,
                                           uint32_t *type,
                                           uint32_t *attributes)
{
    struct Runtime *rt;
    HANDLE h;
    DWORD native_type;
    rt = (struct Runtime *)opaque;
    h = os2_handle(rt, hfile);
    if (h == INVALID_HANDLE_VALUE)
        return OS2_ERROR_INVALID_HANDLE;
    native_type = GetFileType(h);
    *type = native_type == FILE_TYPE_CHAR ? 1u :
            (native_type == FILE_TYPE_PIPE ? 2u : 0u);
    *attributes = 0u;
    return OS2_NO_ERROR;
}

static os2_api_ret_t whp_write_handle(void *opaque,
                                      os2_handle32_t hfile,
                                      const void *buffer,
                                      uint32_t length,
                                      uint32_t *actual)
{
    struct Runtime *rt;
    HANDLE h;
    DWORD done;
    const void *native_buffer;
    rt = (struct Runtime *)opaque;
    h = os2_handle(rt, hfile);
    if (h == INVALID_HANDLE_VALUE)
        return OS2_ERROR_INVALID_HANDLE;
    done = 0;
    native_buffer = length != 0u ? buffer : (const void *)"";
    if (!WriteFile(h, native_buffer, (DWORD)length, &done, NULL)) {
        *actual = (uint32_t)done;
        return (os2_api_ret_t)GetLastError();
    }
    *actual = (uint32_t)done;
    return OS2_NO_ERROR;
}

static os2_api_ret_t whp_set_file_pointer(void *opaque,
                                          os2_handle32_t hfile,
                                          os2_long32_t distance,
                                          uint32_t method,
                                          uint32_t *new_position)
{
    struct Runtime *rt;
    HANDLE h;
    DWORD native_method;
    DWORD position;
    rt = (struct Runtime *)opaque;
    h = os2_handle(rt, hfile);
    if (h == INVALID_HANDLE_VALUE)
        return OS2_ERROR_INVALID_HANDLE;
    native_method = method == 0u ? FILE_BEGIN :
                    (method == 1u ? FILE_CURRENT : FILE_END);
    SetLastError(NO_ERROR);
    position = SetFilePointer(h, (LONG)distance, NULL, native_method);
    if (position == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
        return (os2_api_ret_t)GetLastError();
    *new_position = (uint32_t)position;
    return OS2_NO_ERROR;
}

static os2_api_ret_t whp_allocate_memory(void *opaque,
                                         uint32_t size,
                                         uint32_t flags,
                                         uint32_t reserved,
                                         os2_addr32_t *base)
{
    struct Runtime *rt;
    uint32_t address;
    (void)flags;
    (void)reserved;
    rt = (struct Runtime *)opaque;
    address = alloc_guest(rt, size);
    if (address == 0u)
        return OS2_ERROR_NOT_ENOUGH_MEMORY;
    *base = address;
    fprintf(stderr, "v2:              -> guest %08X\n", address);
    return OS2_NO_ERROR;
}

static os2_api_ret_t whp_free_memory(void *opaque, os2_addr32_t base)
{
    struct Runtime *rt;
    struct GuestAlloc *allocation;
    rt = (struct Runtime *)opaque;
    allocation = find_alloc(rt, base);
    if (allocation == NULL || !allocation->used)
        return OS2_ERROR_INVALID_PARAMETER;
    allocation->used = 0;
    return OS2_NO_ERROR;
}

static os2_api_ret_t whp_set_memory(void *opaque,
                                    os2_addr32_t base,
                                    uint32_t size,
                                    uint32_t flags)
{
    struct Runtime *rt;
    (void)flags;
    rt = (struct Runtime *)opaque;
    if (rt == NULL || !guest_range(base, size))
        return OS2_ERROR_INVALID_PARAMETER;
    /* Guest RAM is already host-backed.  Protection/commit tracking remains
       a WHP backend concern and can be tightened without changing semantics. */
    return OS2_NO_ERROR;
}

static const struct Os2PersonalityOps whp_personality_ops = {
    whp_validate_memory,
    whp_write_memory,
    whp_map_read_memory,
    whp_query_handle_type,
    whp_write_handle,
    whp_set_file_pointer,
    whp_allocate_memory,
    whp_free_memory,
    whp_set_memory,
    os2_win32_query_local_datetime,
    os2_win32_monotonic_milliseconds
};

static HRESULT get_reg64(WHV_PARTITION_HANDLE p, WHV_REGISTER_NAME n, UINT64 *v)
{
    WHV_REGISTER_VALUE r;
    HRESULT hr;
    memset(&r, 0, sizeof(r));
    hr = WHvGetVirtualProcessorRegisters(p, 0, &n, 1, &r);
    if (SUCCEEDED(hr))
        *v = r.Reg64;
    return hr;
}

static HRESULT set_reg64(WHV_PARTITION_HANDLE p, WHV_REGISTER_NAME n, UINT64 v)
{
    WHV_REGISTER_VALUE r;
    memset(&r, 0, sizeof(r));
    r.Reg64 = v;
    return WHvSetVirtualProcessorRegisters(p, 0, &n, 1, &r);
}

static const WHV_REGISTER_NAME thread_reg_names[THREAD_REG_COUNT] = {
    WHvX64RegisterRip, WHvX64RegisterRsp, WHvX64RegisterRflags,
    WHvX64RegisterRax, WHvX64RegisterRbx, WHvX64RegisterRcx,
    WHvX64RegisterRdx, WHvX64RegisterRsi, WHvX64RegisterRdi,
    WHvX64RegisterRbp,
    WHvX64RegisterCs, WHvX64RegisterSs, WHvX64RegisterDs,
    WHvX64RegisterEs, WHvX64RegisterFs, WHvX64RegisterGs
};

static HRESULT save_thread_context(struct Runtime *rt, struct GuestThread *t)
{
    HRESULT hr;
    hr = WHvGetVirtualProcessorRegisters(rt->partition, 0,
        thread_reg_names, THREAD_REG_COUNT, t->regs);
    if (SUCCEEDED(hr))
        t->regs_valid = 1;
    return hr;
}

static HRESULT load_thread_context(struct Runtime *rt, const struct GuestThread *t)
{
    if (!t->regs_valid)
        return E_FAIL;
    return WHvSetVirtualProcessorRegisters(rt->partition, 0,
        thread_reg_names, THREAD_REG_COUNT, t->regs);
}

static struct GuestThread *current_guest_thread(struct Runtime *rt)
{
    if (rt->current_thread < 0 || rt->current_thread >= (int)MAX_THREADS)
        return NULL;
    return &rt->threads[rt->current_thread];
}

static struct GuestThread *find_thread(struct Runtime *rt, uint32_t tid)
{
    uint32_t i;
    for (i = 0; i < MAX_THREADS; ++i) {
        if (rt->threads[i].state != THREAD_FREE && rt->threads[i].tid == tid)
            return &rt->threads[i];
    }
    return NULL;
}

static int next_runnable_thread(struct Runtime *rt)
{
    uint32_t n;
    int start;
    start = rt->current_thread;
    for (n = 1; n <= MAX_THREADS; ++n) {
        int i = (start + (int)n) % (int)MAX_THREADS;
        if (rt->threads[i].state == THREAD_RUNNABLE)
            return i;
    }
    return -1;
}

static const char *thread_state_name(enum GuestThreadState st)
{
    switch (st) {
    case THREAD_FREE: return "FREE";
    case THREAD_RUNNABLE: return "RUNNABLE";
    case THREAD_RUNNING: return "RUNNING";
    case THREAD_WAIT_THREAD: return "WAIT_THREAD";
    case THREAD_WAIT_EVENT: return "WAIT_EVENT";
    case THREAD_SLEEP: return "SLEEP";
    case THREAD_WAIT_MUTEX: return "WAIT_MUTEX";
    case THREAD_WAIT_QUEUE: return "WAIT_QUEUE";
    case THREAD_WAIT_PROCESS: return "WAIT_PROCESS";
    case THREAD_WAIT_KBD: return "WAIT_KBD";
    case THREAD_WAIT_PM: return "WAIT_PM";
    case THREAD_SUSPENDED: return "SUSPENDED";
    case THREAD_DEAD: return "DEAD";
    default: return "?";
    }
}

static void wake_thread_waiters(struct Runtime *rt, uint32_t dead_tid)
{
    uint32_t i;
    for (i = 0; i < MAX_THREADS; ++i) {
        struct GuestThread *t = &rt->threads[i];
        if (t->state != THREAD_WAIT_THREAD)
            continue;
        if (t->wait_tid != 0 && t->wait_tid != dead_tid)
            continue;
        if (t->wait_ptid && guest_range(t->wait_ptid, 4))
            guest_put_u32(rt, t->wait_ptid, dead_tid);
        t->wait_tid = 0;
        t->wait_ptid = 0;
        t->state = THREAD_RUNNABLE;
        fprintf(stderr, "v2: thread %u awakened by completion of TID %u\n",
               t->tid, dead_tid);
        {
            struct GuestThread *dead = find_thread(rt, dead_tid);
            if (dead && dead->state == THREAD_DEAD) dead->state = THREAD_FREE;
        }
    }
}

static uint32_t event_make_handle(uint32_t slot, uint32_t generation)
{
    return (generation << 8) | (slot + 1u);
}

static struct GuestEventSem *event_from_handle(struct Runtime *rt, uint32_t hev,
                                                uint32_t *slot_out)
{
    uint32_t raw_slot, slot, generation;
    raw_slot = hev & 0xffu;
    if (raw_slot == 0 || raw_slot > MAX_EVENT_SEMS)
        return NULL;
    slot = raw_slot - 1u;
    generation = hev >> 8;
    if (generation == 0 || !rt->events[slot].used ||
        rt->events[slot].generation != generation)
        return NULL;
    if (slot_out)
        *slot_out = slot;
    return &rt->events[slot];
}

static void set_saved_thread_rc(struct GuestThread *t, uint32_t rc)
{
    /* thread_reg_names[3] is RAX/EAX.  A blocking API has already advanced
       past OUT before the context is saved, so changing saved EAX changes
       exactly the API result observed when that thread resumes at RET. */
    if (t->regs_valid)
        t->regs[3].Reg64 = rc;
}

static void wake_event_waiters(struct Runtime *rt, uint32_t hev)
{
    uint32_t i;
    for (i = 0; i < MAX_THREADS; ++i) {
        struct GuestThread *t = &rt->threads[i];
        if (t->state != THREAD_WAIT_EVENT || t->wait_event != hev)
            continue;
        t->wait_event = 0;
        t->wait_deadline_ms = 0;
        set_saved_thread_rc(t, OS2_NO_ERROR);
        t->state = THREAD_RUNNABLE;
        fprintf(stderr, "v2: thread %u awakened by event semaphore %08X\n",
               t->tid, hev);
    }
}

static int console_poll_wait(struct Runtime *rt, struct GuestThread *t);
static int process_poll_wait(struct Runtime *rt,struct GuestThread *t);
static int wake_expired_waiters(struct Runtime *rt)
{
    uint32_t i;
    uint64_t now;
    int woke;
    now = GetTickCount64();
    woke = 0;
    for (i = 0; i < MAX_THREADS; ++i) {
        struct GuestThread *t = &rt->threads[i];
        if(t->state==THREAD_WAIT_PROCESS) {
            if(t->wait_deadline_ms<=now) {
                if(process_poll_wait(rt,t))woke=1;
                else t->wait_deadline_ms=now+10;
            }
            continue;
        }
        if(t->state==THREAD_WAIT_KBD) {
            if(t->wait_deadline_ms<=now) {
                if(console_poll_wait(rt,t))woke=1;
                else t->wait_deadline_ms=now+10;
            }
            continue;
        }
        if ((t->state != THREAD_WAIT_EVENT && t->state != THREAD_WAIT_MUTEX &&
             t->state != THREAD_SLEEP) || t->wait_deadline_ms == 0)
            continue;
        if (t->wait_deadline_ms > now)
            continue;
        fprintf(stderr, "v2: thread %u %s deadline reached\n",
               t->tid, thread_state_name(t->state));
        t->wait_event = 0;
        t->wait_deadline_ms = 0;
        set_saved_thread_rc(t, t->state == THREAD_SLEEP ? OS2_NO_ERROR : OS2_ERROR_SEM_TIMEOUT);
        t->state = THREAD_RUNNABLE;
        woke = 1;
    }
    return woke;
}

static int next_wait_timeout(struct Runtime *rt, DWORD *wait_ms)
{
    uint32_t i;
    uint64_t now, earliest, delta;
    earliest = 0;
    for (i = 0; i < MAX_THREADS; ++i) {
        struct GuestThread *t = &rt->threads[i];
        if ((t->state != THREAD_WAIT_EVENT && t->state != THREAD_WAIT_MUTEX &&
             t->state != THREAD_SLEEP && t->state != THREAD_WAIT_KBD && t->state != THREAD_WAIT_PROCESS) || t->wait_deadline_ms == 0)
            continue;
        if (earliest == 0 || t->wait_deadline_ms < earliest)
            earliest = t->wait_deadline_ms;
    }
    if (earliest == 0)
        return 0;
    now = GetTickCount64();
    if (earliest <= now)
        *wait_ms = 0;
    else {
        delta = earliest - now;
        *wait_ms = (DWORD)(delta > 0xfffffffeULL ? 0xfffffffeULL : delta);
    }
    return 1;
}

static void emit_runtime_stubs(struct Runtime *rt)
{
    uint8_t *p;
    if (!guest_range(GUEST_THREAD_EXIT_STUB, 8))
        fatal("thread-exit stub outside guest RAM");
    p = rt->ram + GUEST_THREAD_EXIT_STUB;
    p[0] = 0xB8;
    wr32(p + 1, HC_RUNTIME | HC_RUNTIME_THREAD_RETURN);
    p[5] = 0xE7;
    p[6] = (uint8_t)HOSTCALL_PORT;
    p[7] = 0xF4; /* HLT if the host accidentally resumes this dead thread. */
    p = rt->ram + GUEST_CALLBACK_RETURN_STUB;
    p[0] = 0x50; /* push eax: preserve callback result before loading trap ID */
    p[1] = 0xB8; wr32(p + 2, HC_RUNTIME | HC_RUNTIME_CALLBACK_RETURN);
    p[6] = 0xE7; p[7] = (uint8_t)HOSTCALL_PORT; p[8] = 0xF4;
}

#include "v2_info.h"
#include "v2_callback.h"
#include "v2_sync.h"
#include "v2_queue.h"
static void gpi_cleanup(struct Runtime *rt);
static int gpi_window_ps(struct Runtime *rt,uint32_t id);
#include "v2_pm.h"
#include "v2_gpi.h"
#include "v2_beep.h"
#include "v2_console.h"
#include "v2_cmdfs.h"
#include "v2_process.h"

static uint32_t create_guest_thread(struct Runtime *rt, uint32_t ptid,
                                    uint32_t entry, uint32_t param,
                                    uint32_t flags, uint32_t stack_size)
{
    uint32_t i, stack_base, stack_bytes, sp, tid;
    struct GuestThread *t;
    HRESULT hr;

    if (!guest_range(ptid, 4) || !guest_range(entry, 1) || ptid == 0 || entry == 0 ||
        stack_size == 0 || stack_size > GUEST_ALLOC_LIMIT - GUEST_ALLOC_BASE ||
        (flags & ~1u) || rt->next_tid == 0xffffffffu)
        return OS2_ERROR_INVALID_PARAMETER;

    for (i = 0; i < MAX_THREADS; ++i)
        if (rt->threads[i].state == THREAD_FREE || rt->threads[i].state == THREAD_DEAD)
            break;
    if (i == MAX_THREADS)
        return OS2_ERROR_NOT_ENOUGH_MEMORY;

    stack_bytes = align_up(stack_size, 0x1000u);
    stack_base = alloc_guest(rt, stack_bytes);
    if (!stack_base)
        return OS2_ERROR_NOT_ENOUGH_MEMORY;

    t = &rt->threads[i];
    memset(t, 0, sizeof(*t));

    /* Copy the current protected-mode segment setup, then replace the new
       thread's integer execution state and install its own TIB/FS descriptor. */
    hr = WHvGetVirtualProcessorRegisters(rt->partition, 0,
        thread_reg_names, THREAD_REG_COUNT, t->regs);
    if (FAILED(hr)) {
        struct GuestAlloc *ga = find_alloc(rt, stack_base);
        if (ga) ga->used = 0;
        memset(t, 0, sizeof(*t));
        return OS2_ERROR_INVALID_FUNCTION;
    }

    sp = stack_base + stack_bytes;
    sp &= ~0x0fu;
    sp -= 4; guest_put_u32(rt, sp, param);
    sp -= 4; guest_put_u32(rt, sp, GUEST_THREAD_EXIT_STUB);

    t->regs[0].Reg64 = entry;       /* RIP */
    t->regs[1].Reg64 = sp;          /* RSP */
    t->regs[2].Reg64 = 0x00000002u; /* RFLAGS */
    t->regs[3].Reg64 = 0;           /* RAX */
    t->regs[4].Reg64 = 0;           /* RBX */
    t->regs[5].Reg64 = 0;           /* RCX */
    t->regs[6].Reg64 = 0;           /* RDX */
    t->regs[7].Reg64 = 0;           /* RSI */
    t->regs[8].Reg64 = 0;           /* RDI */
    t->regs[9].Reg64 = 0;           /* RBP */

    tid = rt->next_tid++;
    if (tid == 0)
        tid = rt->next_tid++;
    t->tid = tid;
    t->stack_base = stack_base;
    t->stack_size = stack_bytes;
    t->owns_stack = 1;
    init_thread_info(rt, i);
    t->regs_valid = 1;
    t->state = (flags & 1u) ? THREAD_SUSPENDED : THREAD_RUNNABLE;
    guest_put_u32(rt, ptid, tid);

    fprintf(stderr, "v2:              -> TID %u EIP=%08X stack=%08X..%08X ESP=%08X state=%s\n",
           tid, entry, stack_base, stack_base + stack_bytes, sp,
           thread_state_name(t->state));
    return OS2_NO_ERROR;
}

static WHV_X64_SEGMENT_REGISTER flat_segment(UINT16 selector, UINT16 attributes)
{
    WHV_X64_SEGMENT_REGISTER s;
    memset(&s, 0, sizeof(s));
    s.Base = 0;
    s.Limit = 0xffffffffu;
    s.Selector = selector;
    s.Attributes = attributes;
    return s;
}

static void dump_regs(struct Runtime *rt, const char *why)
{
    const WHV_REGISTER_NAME names[] = {
        WHvX64RegisterRip, WHvX64RegisterRsp, WHvX64RegisterRflags,
        WHvX64RegisterRax, WHvX64RegisterRbx, WHvX64RegisterRcx,
        WHvX64RegisterRdx, WHvX64RegisterRsi, WHvX64RegisterRdi,
        WHvX64RegisterRbp
    };
    WHV_REGISTER_VALUE v[sizeof(names) / sizeof(names[0])];
    HRESULT hr;
    memset(v, 0, sizeof(v));
    hr = WHvGetVirtualProcessorRegisters(rt->partition, 0, names,
        (UINT32)(sizeof(names) / sizeof(names[0])), v);
    if (FAILED(hr)) {
        failed("WHvGetVirtualProcessorRegisters(dump)", hr);
        return;
    }
    fprintf(stderr,
        "\nv2: %s\n"
        "  EIP=%08X ESP=%08X EFLAGS=%08X\n"
        "  EAX=%08X EBX=%08X ECX=%08X EDX=%08X\n"
        "  ESI=%08X EDI=%08X EBP=%08X\n",
        why,
        (uint32_t)v[0].Reg64, (uint32_t)v[1].Reg64, (uint32_t)v[2].Reg64,
        (uint32_t)v[3].Reg64, (uint32_t)v[4].Reg64,
        (uint32_t)v[5].Reg64, (uint32_t)v[6].Reg64,
        (uint32_t)v[7].Reg64, (uint32_t)v[8].Reg64,
        (uint32_t)v[9].Reg64);
}

static const char *dos_name(uint32_t ordinal)
{
    const char *name;
    name = os2_api_name("DOSCALLS", ordinal);
    return name != NULL ? name : "?";
}

static uint32_t dispatch_doscalls(struct Runtime *rt, uint32_t ordinal, uint32_t esp)
{
    uint32_t a1, a2, a3, a4, a5;
    struct Os2PersonalityContext personality;

    os2_personality_context_init(&personality, rt, &whp_personality_ops);
    a1 = guest_u32(rt, esp + 4u);
    a2 = guest_u32(rt, esp + 8u);
    a3 = guest_u32(rt, esp + 12u);
    a4 = guest_u32(rt, esp + 16u);
    a5 = guest_range(esp + 20u, 4) ? guest_u32(rt, esp + 20u) : 0u;

    switch (ordinal) {
    case 220: case 226: case 239: case 255: case 260: case 270: case 271:
    case 274: case 275: case 323:
        return dispatch_cmdfs(rt,ordinal,esp);
    case 280:case 283: return dispatch_process(rt,ordinal,esp);
    case 263: case 264: case 265:
    {
        uint32_t find_rc = dispatch_find(rt, ordinal, esp);
        fprintf(stderr, "v2: DOSCALLS.%u -> rc=%u\n", ordinal, find_rc);
        return find_rc;
    }
    case 223: case 257: case 259:
    case 272: case 273: case 279: case 281:
    {
        uint32_t file_rc = dispatch_fileio(rt, ordinal, esp);
        fprintf(stderr, "v2: DOSCALLS.%u -> rc=%u\n", ordinal, file_rc);
        return file_rc;
    }
    case 230: /* DosGetDateTime(PDATETIME) */
        fprintf(stderr, "v2: DOSCALLS.230 DosGetDateTime(%08X) [shared]\n", a1);
        return os2_core_DosGetDateTime(&personality, a1);

    case 224: /* DosQueryHType(HFILE, PULONG type, PULONG attr) */
        fprintf(stderr, "v2: DOSCALLS.224 DosQueryHType(%u,%08X,%08X) [shared]\n", a1, a2, a3);
        return os2_core_DosQueryHType(&personality, a1, a2, a3);

    case 234: /* DosExit(action,result), does not return */
        fprintf(stderr, "v2: DOSCALLS.234 DosExit(action=%u,result=%u)\n", a1, a2);
        if (a1 == EXIT_THREAD) {
            struct GuestThread *cur = current_guest_thread(rt);
            if (!cur)
                return OS2_ERROR_INVALID_FUNCTION;
            fprintf(stderr, "v2: thread %u exited rc=%u\n", cur->tid, a2);
            finish_guest_thread(rt);
            return OS2_NO_ERROR;
        }
        if (a1 != EXIT_PROCESS)
            return OS2_ERROR_INVALID_PARAMETER;
        rt->process_exited = 1;
        rt->process_rc = a2;
        return OS2_NO_ERROR;

    case 256: /* DosSetFilePtr(HFILE,LONG,method,PULONG) */
        fprintf(stderr, "v2: DOSCALLS.256 DosSetFilePtr(%u,%ld,%u,%08X) [shared]\n",
               a1, (long)(int32_t)a2, a3, a4);
        return os2_core_DosSetFilePtr(&personality, a1,
                                      (int32_t)a2, a3, a4);

    case 282: /* DosWrite(HFILE,buffer,count,PULONG actual) */
        fprintf(stderr, "v2: DOSCALLS.282 DosWrite(%u,%08X,%u,%08X) [shared]\n", a1, a2, a3, a4);
        return os2_core_DosWrite(&personality, a1, a2, a3, a4);

    case 299: /* prerelease C/386 ABI: ppBase,size,flags,reserved */
        fprintf(stderr, "v2: DOSCALLS.299 DosAllocMem(pp=%08X,size=%u,flags=%08X,res=%08X) [shared]\n",
               a1, a2, a3, a4);
        return os2_core_DosAllocMem(&personality, a1, a2, a3, a4);

    case 304: /* DosFreeMem(base) */
        fprintf(stderr, "v2: DOSCALLS.304 DosFreeMem(%08X) [shared]\n", a1);
        return os2_core_DosFreeMem(&personality, a1);

    case 305: /* DosSetMem(base,size,flags) */
        fprintf(stderr, "v2: DOSCALLS.305 DosSetMem(%08X,%u,%08X) [shared]\n", a1, a2, a3);
        return os2_core_DosSetMem(&personality, a1, a2, a3);

    case 229: return guest_sleep(rt, a1);
    case 286: return guest_beep(rt,a1,a2);
    case 331: case 332: case 333: case 334: case 335: case 336:
        return dispatch_mutex(rt, ordinal, a1, a2, a3, a4);

    case 312: /* DosGetInfoBlocks(PTIB *, PPIB *) */
        return get_info_blocks(rt, a1, a2);

    case 311: /* DosCreateThread(PTID,PFNTHREAD,param,flags,stackSize) */
        fprintf(stderr, "v2: DOSCALLS.311 DosCreateThread(ptid=%08X,fn=%08X,param=%08X,flags=%08X,stack=%u)\n",
               a1, a2, a3, a4, a5);
        return create_guest_thread(rt, a1, a2, a3, a4, a5);

    case 324: /* DosCreateEventSem(name,phev,flags,initialState) */
    {
        uint32_t i, free_slot;
        struct GuestEventSem *sem;
        char name[EVENT_NAME_MAX + 1];
        int have_name;
        fprintf(stderr, "v2: DOSCALLS.324 DosCreateEventSem(name=%08X,phev=%08X,flags=%08X,initial=%u)\n",
               a1, a2, a3, a4);
        if (!guest_range(a2, 4))
            return OS2_ERROR_INVALID_PARAMETER;
        have_name = (a1 != 0);
        if (have_name && !guest_copy_cstr(rt, a1, name, (uint32_t)sizeof(name)))
            return OS2_ERROR_INVALID_PARAMETER;

        free_slot = MAX_EVENT_SEMS;
        for (i = 0; i < MAX_EVENT_SEMS; ++i) {
            sem = &rt->events[i];
            if (!sem->used) {
                if (free_slot == MAX_EVENT_SEMS && sem->generation < SYNC_GENERATION_MAX)
                    free_slot = i;
                continue;
            }
            if (have_name && sem->name[0] && strcmp(sem->name, name) == 0)
                return OS2_ERROR_DUPLICATE_NAME;
        }
        if (free_slot == MAX_EVENT_SEMS)
            return OS2_ERROR_TOO_MANY_OPENS;

        sem = &rt->events[free_slot];
        sem->generation++;
        if (sem->generation == 0)
            sem->generation = 1;
        sem->used = 1;
        sem->refs = 1;
        sem->post_count = a4 ? 1u : 0u;
        sem->name[0] = '\0';
        if (have_name)
            strcpy(sem->name, name);
        guest_put_u32(rt, a2, event_make_handle(free_slot, sem->generation));
        fprintf(stderr, "v2:              -> HEV %08X post_count=%u%s%s\n",
               event_make_handle(free_slot, sem->generation), sem->post_count,
               have_name ? " name=" : "", have_name ? sem->name : "");
        return OS2_NO_ERROR;
    }

    case 325: /* DosOpenEventSem(name,phev) */
    {
        uint32_t i, hev;
        struct GuestEventSem *sem;
        char name[EVENT_NAME_MAX + 1];
        fprintf(stderr, "v2: DOSCALLS.325 DosOpenEventSem(name=%08X,phev=%08X)\n", a1, a2);
        if (!guest_range(a2, 4))
            return OS2_ERROR_INVALID_PARAMETER;
        if (a1 != 0) {
            if (!guest_copy_cstr(rt, a1, name, (uint32_t)sizeof(name)))
                return OS2_ERROR_INVALID_PARAMETER;
            for (i = 0; i < MAX_EVENT_SEMS; ++i) {
                sem = &rt->events[i];
                if (sem->used && sem->name[0] && strcmp(sem->name, name) == 0) {
                    sem->refs++;
                    hev = event_make_handle(i, sem->generation);
                    guest_put_u32(rt, a2, hev);
                    fprintf(stderr, "v2:              -> HEV %08X refs=%u\n", hev, sem->refs);
                    return OS2_NO_ERROR;
                }
            }
            return OS2_ERROR_SEM_NOT_FOUND;
        }
        hev = guest_u32(rt, a2);
        sem = event_from_handle(rt, hev, NULL);
        if (!sem)
            return OS2_ERROR_INVALID_HANDLE;
        sem->refs++;
        fprintf(stderr, "v2:              -> HEV %08X refs=%u\n", hev, sem->refs);
        return OS2_NO_ERROR;
    }

    case 326: /* DosCloseEventSem(hev) */
    {
        struct GuestEventSem *sem;
        fprintf(stderr, "v2: DOSCALLS.326 DosCloseEventSem(%08X)\n", a1);
        sem = event_from_handle(rt, a1, NULL);
        if (!sem)
            return OS2_ERROR_INVALID_HANDLE;
        if (sem->refs > 1) {
            sem->refs--;
            return OS2_NO_ERROR;
        }
        if (sync_first_waiter(rt, THREAD_WAIT_EVENT, a1))
            return OS2_ERROR_SEM_BUSY;
        sem->used = 0;
        sem->refs = 0;
        sem->post_count = 0;
        sem->name[0] = '\0';
        return OS2_NO_ERROR;
    }

    case 327: /* DosResetEventSem(hev,PULONG postCount) */
    {
        struct GuestEventSem *sem;
        uint32_t old_count;
        fprintf(stderr, "v2: DOSCALLS.327 DosResetEventSem(%08X,%08X)\n", a1, a2);
        if (!guest_range(a2, 4))
            return OS2_ERROR_INVALID_PARAMETER;
        sem = event_from_handle(rt, a1, NULL);
        if (!sem)
            return OS2_ERROR_INVALID_HANDLE;
        old_count = sem->post_count;
        guest_put_u32(rt, a2, old_count);
        if (old_count == 0)
            return OS2_ERROR_ALREADY_RESET;
        sem->post_count = 0;
        return OS2_NO_ERROR;
    }

    case 328: /* DosPostEventSem(hev) */
    {
        struct GuestEventSem *sem;
        int already_posted;
        fprintf(stderr, "v2: DOSCALLS.328 DosPostEventSem(%08X)\n", a1);
        sem = event_from_handle(rt, a1, NULL);
        if (!sem)
            return OS2_ERROR_INVALID_HANDLE;
        already_posted = (sem->post_count != 0);
        sem->post_count++;
        fprintf(stderr, "v2:              -> post_count=%u%s\n", sem->post_count,
               already_posted ? " (already posted)" : "");
        wake_event_waiters(rt, a1);
        return already_posted ? OS2_ERROR_ALREADY_POSTED : OS2_NO_ERROR;
    }

    case 329: /* DosWaitEventSem(hev,timeout_ms) */
    {
        struct GuestEventSem *sem;
        struct GuestThread *cur;
        fprintf(stderr, "v2: DOSCALLS.329 DosWaitEventSem(%08X,%u%s)\n", a1, a2,
               a2 == SEM_INDEFINITE_WAIT ? " [indefinite]" : "");
        sem = event_from_handle(rt, a1, NULL);
        if (!sem)
            return OS2_ERROR_INVALID_HANDLE;
        if (sem->post_count != 0)
            return OS2_NO_ERROR;
        if (a2 == 0)
            return OS2_ERROR_SEM_TIMEOUT;
        cur = current_guest_thread(rt);
        if (!cur)
            return OS2_ERROR_INVALID_FUNCTION;
        cur->wait_event = a1;
        cur->wait_deadline_ms = (a2 == SEM_INDEFINITE_WAIT) ? 0 :
                                (GetTickCount64() + (uint64_t)a2);
        cur->state = THREAD_WAIT_EVENT;
        rt->switch_requested = 1;
        fprintf(stderr, "v2: thread %u waiting on event semaphore %08X%s\n",
               cur->tid, a1, a2 == SEM_INDEFINITE_WAIT ? " indefinitely" : "");
        return OS2_NO_ERROR;
    }

    case 330: /* DosQueryEventSem(hev,PULONG postCount) */
    {
        struct GuestEventSem *sem;
        fprintf(stderr, "v2: DOSCALLS.330 DosQueryEventSem(%08X,%08X)\n", a1, a2);
        if (!guest_range(a2, 4))
            return OS2_ERROR_INVALID_PARAMETER;
        sem = event_from_handle(rt, a1, NULL);
        if (!sem)
            return OS2_ERROR_INVALID_HANDLE;
        guest_put_u32(rt, a2, sem->post_count);
        return OS2_NO_ERROR;
    }

    case 348: /* DosQuerySysInfo(first,last,buffer,cb) */
        fprintf(stderr, "v2: DOSCALLS.348 DosQuerySysInfo(%u,%u,%08X,%u) [shared]\n",
               a1, a2, a3, a4);
        return os2_core_DosQuerySysInfo(&personality, a1, a2, a3, a4);

    case 349: return guest_wait_thread(rt, a1, a2);

    default:
        fprintf(stderr, "v2: unsupported DOSCALLS ordinal %u (%s)\n",
                ordinal, dos_name(ordinal));
        return OS2_ERROR_INVALID_FUNCTION;
    }
}

static HRESULT advance_rip(struct Runtime *rt, const WHV_RUN_VP_EXIT_CONTEXT *ec)
{
    return set_reg64(rt->partition, WHvX64RegisterRip,
                     ec->VpContext.Rip + ec->VpContext.InstructionLength);
}

static int schedule_next_thread(struct Runtime *rt)
{
    int next;
    struct GuestThread *t;
    HRESULT hr;

    for (;;) {
        DWORD wait_ms;
        pm_pump(rt);
        if (rt->process_exited) return 1;
        wake_expired_waiters(rt);
        next = next_runnable_thread(rt);
        if (next >= 0)
            break;

        /* Host sleeps only when every guest is blocked, until the nearest
           sleep/event/mutex deadline. Guest waits never block a runnable peer. */
        if (pm_waiting(rt)) {
            if (!next_wait_timeout(rt, &wait_ms)) wait_ms = INFINITE;
            if (MsgWaitForMultipleObjectsEx(0,NULL,wait_ms,QS_ALLINPUT,MWMO_INPUTAVAILABLE)==WAIT_FAILED) {
                fprintf(stderr,"v2: PM message wait failed\n"); return 1;
            }
            continue;
        }
        if (!next_wait_timeout(rt, &wait_ms)) {
            fprintf(stderr, "v2: scheduler deadlock: no runnable guest thread and no finite timeout\n");
            return 1;
        }
        if (wait_ms != 0)
            Sleep(wait_ms);
    }

    t = &rt->threads[next];
    hr = load_thread_context(rt, t);
    if (FAILED(hr)) {
        failed("load guest thread context", hr);
        return 1;
    }
    rt->current_thread = next;
    t->state = THREAD_RUNNING;
    fprintf(stderr, "v2: scheduler -> TID %u EIP=%08X ESP=%08X\n",
           t->tid, (uint32_t)t->regs[0].Reg64, (uint32_t)t->regs[1].Reg64);
    return 0;
}

static int switch_after_hypercall(struct Runtime *rt)
{
    struct GuestThread *cur;
    HRESULT hr;

    cur = current_guest_thread(rt);
    if (!cur)
        return 1;

    if (!rt->current_thread_exited) {
        hr = save_thread_context(rt, cur);
        if (FAILED(hr)) {
            failed("save guest thread context", hr);
            return 1;
        }
        if (cur->state == THREAD_RUNNING)
            cur->state = THREAD_RUNNABLE;
    }

    rt->switch_requested = 0;
    rt->current_thread_exited = 0;
    return schedule_next_thread(rt);
}

static int run_guest(struct Runtime *rt, uint32_t entry, uint32_t initial_esp)
{
    HRESULT hr;

    for (;;) {
        WHV_RUN_VP_EXIT_CONTEXT ec;
        pm_pump(rt);
        if (rt->process_exited) return (int)rt->process_rc;
        memset(&ec, 0, sizeof(ec));
        hr = WHvRunVirtualProcessor(rt->partition, 0, &ec, sizeof(ec));
        if (FAILED(hr)) {
            failed("WHvRunVirtualProcessor", hr);
            dump_regs(rt, "run failure");
            return 1;
        }

        switch (ec.ExitReason) {
        case WHvRunVpExitReasonX64IoPortAccess:
        {
            const WHV_X64_IO_PORT_ACCESS_CONTEXT *io;
            uint32_t api, module, ordinal, rc;
            UINT64 rsp;
            io = &ec.IoPortAccess;
            if (!io->AccessInfo.IsWrite || io->PortNumber != HOSTCALL_PORT) {
                fprintf(stderr, "v2: unexpected I/O exit: %s port=%04X size=%u\n",
                    io->AccessInfo.IsWrite ? "OUT" : "IN",
                    io->PortNumber, io->AccessInfo.AccessSize);
                dump_regs(rt, "unexpected I/O");
                return 1;
            }
            api = (uint32_t)io->Rax;
            module = api & 0xff000000u;
            ordinal = api & 0x00ffffffu;
            hr = get_reg64(rt->partition, WHvX64RegisterRsp, &rsp);
            if (FAILED(hr)) {
                failed("get RSP", hr);
                return 1;
            }
            if (module == HC_RUNTIME && ordinal == HC_RUNTIME_CALLBACK_RETURN) {
                rc = return_guest_callback(rt, (uint32_t)rsp, (uint32_t)ec.VpContext.Rip);
                if (rc != 0) {
                    fprintf(stderr,"v2: invalid callback return rc=%u\n",rc);
                    dump_regs(rt,"callback return"); return 1;
                }
                continue; /* restored RIP is the suspended caller's continuation */
            }
            if (!guest_range((uint32_t)rsp, 20)) {
                dump_regs(rt, "hypercall with invalid guest ESP");
                return 1;
            }

            wake_expired_waiters(rt);
            if (module == HC_WHPTEST) {
                uint32_t args[2];
                args[0]=guest_u32(rt,(uint32_t)rsp+8u);
                args[1]=guest_u32(rt,(uint32_t)rsp+12u);
                rc = ordinal == 1 && guest_u32(rt,(uint32_t)rsp+16u) ? begin_guest_callback(rt,
                    guest_u32(rt,(uint32_t)rsp+4u),args,2,
                    guest_u32(rt,(uint32_t)rsp+16u),
                    (uint32_t)(ec.VpContext.Rip+ec.VpContext.InstructionLength)) :
                    (ordinal == 1 ? OS2_ERROR_INVALID_PARAMETER : OS2_ERROR_INVALID_FUNCTION);
                if (rc == 0) continue; /* enter callback; do not advance its RIP */
            } else if (module == HC_PMGPI) {
                rc=dispatch_gpi(rt,ordinal,(uint32_t)rsp);
            } else if (module == HC_PMWIN) {
                int entered=0;
                rc=dispatch_pm(rt,ordinal,(uint32_t)rsp,
                    (uint32_t)(ec.VpContext.Rip+ec.VpContext.InstructionLength),&entered);
                if (entered) continue;
            } else if (module == HC_CONSOLE) {
                rc=dispatch_console(rt,ordinal,(uint32_t)rsp);
            } else if (module == HC_SESMGR) {
                fprintf(stderr,"v2: SESMGR.%u not yet supported by WHP CMD phase 1\n",ordinal);
                rc=OS2_ERROR_INVALID_FUNCTION;
            } else if (module == HC_DOSCALLS) {
                rc = dispatch_doscalls(rt, ordinal, (uint32_t)rsp);
            } else if (module == HC_QUECALLS) {
                rc = dispatch_queue(rt, ordinal, (uint32_t)rsp);
            } else if (module == HC_RUNTIME && ordinal == HC_RUNTIME_THREAD_RETURN) {
                struct GuestThread *cur = current_guest_thread(rt);
                if (!cur) {
                    fprintf(stderr, "v2: thread-return hypercall without current thread\n");
                    return 1;
                }
                fprintf(stderr, "v2: runtime: TID %u returned from thread entry\n", cur->tid);
                finish_guest_thread(rt);
                rc = OS2_NO_ERROR;
            } else {
                fprintf(stderr, "v2: unknown hypercall module %08X api=%08X\n", module, api);
                dump_regs(rt, "unknown hypercall");
                return 1;
            }

            if (rt->process_exited) {
                fprintf(stderr, "v2: guest process exited rc=%u\n", rt->process_rc);
                return (int)rt->process_rc;
            }

            /* A dead thread never returns from its hypercall. A blocking call
               does: complete the OUT first, save that completed context, and
               only then restore another runnable thread. */
            if (!rt->current_thread_exited) {
                hr = set_reg64(rt->partition, WHvX64RegisterRax, rc);
                if (FAILED(hr)) {
                    failed("set guest EAX return value", hr);
                    return 1;
                }
                hr = advance_rip(rt, &ec);
                if (FAILED(hr)) {
                    failed("advance EIP after hypercall", hr);
                    return 1;
                }
            }

            if (rt->switch_requested) {
                if (switch_after_hypercall(rt))
                    return 1;
            }
            break;
        }

        case WHvRunVpExitReasonX64Halt:
            dump_regs(rt, "unexpected guest HLT");
            return 1;

        default:
            fprintf(stderr, "v2: unexpected WHP exit reason %u\n", (unsigned)ec.ExitReason);
            dump_regs(rt, "guest stopped unexpectedly");
            fprintf(stderr, "v2: entry was %08X initial ESP=%08X\n", entry, initial_esp);
            return 1;
        }
    }
}

int main(int argc, char **argv)
{
    struct Runtime rt;
    struct LeImage x;
    WHV_CAPABILITY cap;
    UINT32 written = 0, processor_count = 1;
    HRESULT hr;
    uint32_t i, entry, stack_top, esp, env_va, arg_va, pgm_va;
    int vp_created = 0;
    int rc = 1;
    static const uint64_t gdt[3 + MAX_THREADS] = {
        0x0000000000000000ULL,
        0x00CF9B000000FFFFULL,
        0x00CF93000000FFFFULL
    };

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    if(argc==3 && !strcmp(argv[1],"--whp-child")) {
        if(!child_attach(argv[2]))return 31;
        argv[1]=child_boot->data;argc=2;
    }

    if (argc == 3 && strcmp(argv[1], "--check") == 0)
        return check_image(argv[2]);

    if (argc < 2) {
        fprintf(stderr, "usage: whp_os2_v2_hi.exe program.exe [guest arguments ...] | --check image.exe\n");
        return 2;
    }

    memset(&rt, 0, sizeof(rt));
    memset(&x, 0, sizeof(x));
    rt.alloc_next = GUEST_ALLOC_BASE;
    rt.module_next = GUEST_MODULE_BASE;
    rt.stub_next = GUEST_STUB_BASE;
    rt.next_module_handle = 0x00010000u;
    rt.next_tid = 2;
    rt.next_search_id = 0x10000u;
    rt.current_thread = -1;
    if(child_boot)for(i=0;i<3;i++)rt.std_closed[i]=(child_boot->closed_mask>>i)&1;

    x.file = load_file(argv[1], &x.file_size);
    if (!x.file) {
        fprintf(stderr, "v2: cannot read %s\n", argv[1]);
        return 1;
    }

    memset(&cap, 0, sizeof(cap));
    hr = WHvGetCapability(WHvCapabilityCodeHypervisorPresent,
                          &cap, sizeof(cap), &written);
    if (FAILED(hr)) {
        free(x.file);
        return failed("WHvGetCapability", hr);
    }
    if (!cap.HypervisorPresent) {
        fprintf(stderr, "v2: Windows hypervisor is not present.\n");
        free(x.file);
        return 1;
    }

    fprintf(stderr, "WHP OS/2 V2 M3 - common OS/2 personality + LE/LX loader + threads + guest DLLs\n");
    fprintf(stderr, "v2: input %s (%u bytes)\n", argv[1], x.file_size);

    hr = WHvCreatePartition(&rt.partition);
    if (FAILED(hr)) goto whp_fail_create;
    hr = WHvSetPartitionProperty(rt.partition,
        WHvPartitionPropertyCodeProcessorCount,
        &processor_count, sizeof(processor_count));
    if (FAILED(hr)) goto whp_fail_property;
    hr = WHvSetupPartition(rt.partition);
    if (FAILED(hr)) goto whp_fail_setup;

    rt.ram = (uint8_t *)VirtualAlloc(NULL, RAM_SIZE,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!rt.ram) {
        fprintf(stderr, "v2: VirtualAlloc(%u MiB) failed: %lu\n",
                RAM_SIZE / (1024u * 1024u), (unsigned long)GetLastError());
        goto cleanup;
    }
    memset(rt.ram, 0, RAM_SIZE);

    parse_header(&x);
    if ((x.module_flags & MOD_TYPE_MASK) == MOD_TYPE_DLL)
        fatal("main input must be an OS/2 executable, not a DLL");
    parse_objects(&x, &rt, 1);
    parse_import_modules(&x);
    scan_fixups(&x);
    resolve_imports(&rt, &x, base_name(argv[1]));
    emit_runtime_stubs(&rt);
    apply_fixups(&x, rt.ram);
    install_far16(&x, &rt);

    entry = x.objects[x.entry_object - 1u].mapped_addr + x.entry_offset;
    stack_top = x.objects[x.stack_object - 1u].mapped_addr + x.stack_offset;
    if (!guest_range(entry, 1) || !guest_range(stack_top - 20u, 20u))
        fatal("entry/stack lies outside guest RAM");

    build_startup_area(rt.ram, argv[1], argc, argv, &env_va, &arg_va, &pgm_va);

    init_process_info(&rt, env_va, arg_va, x.module_flags);
    guest_put_u32(&rt,GUEST_INFO,GetCurrentProcessId());
    guest_put_u32(&rt,GUEST_INFO+4,child_boot?child_boot->parent_pid:0);

    /* Match the V1 C/386 startup bridge exactly: set stack top, then push
       cmd, env, 0, 0, 0. At LE entry [ESP+12]=env and [ESP+16]=cmd. */
    esp = stack_top;
    esp -= 4; wr32(rt.ram + esp, arg_va);
    esp -= 4; wr32(rt.ram + esp, env_va);
    esp -= 4; wr32(rt.ram + esp, 0);
    esp -= 4; wr32(rt.ram + esp, 0);
    esp -= 4; wr32(rt.ram + esp, 0);

    memcpy(rt.ram + GUEST_GDT, gdt, sizeof(gdt));

    fprintf(stderr, "v2: %s header       = %08X\n", x.is_lx ? "LX" : "LE", x.le);
    fprintf(stderr, "v2: physical pages  = %u\n", x.num_pages);
    fprintf(stderr, "v2: entry           = object %u + %08X -> %08X\n",
           x.entry_object, x.entry_offset, entry);
    fprintf(stderr, "v2: stack           = object %u + %08X -> %08X (initial ESP %08X)\n",
           x.stack_object, x.stack_offset, stack_top, esp);
    for (i = 0; i < x.num_objects; ++i) {
        fprintf(stderr, "v2: object %-2u      = pref %08X guest %08X size=%08X flags=%08X pages=%u @%u\n",
               i + 1u, x.objects[i].addr, x.objects[i].mapped_addr, x.objects[i].size,
               x.objects[i].flags, x.objects[i].mapsize, x.objects[i].mapidx);
    }
    fprintf(stderr, "v2: imports          = %u ordinal + %u named / %u sites\n",
           x.nimports, x.nname_imports, x.external_sites);
    for (i = 0; i < x.nimports; ++i) {
        const char *module_name;
        const struct Os2ApiDescriptor *api;
        module_name = x.modules[x.imports[i].module].name;
        api = os2_api_lookup(module_name, x.imports[i].ordinal);
        if (api != NULL)
            fprintf(stderr,
                    "v2:   %-12s .%-4u %-24s [%-11s] -> %08X (%u site%s)\n",
                    module_name, x.imports[i].ordinal, api->api_name,
                    os2_api_route_name(api->route), x.imports[i].address,
                    x.imports[i].sites,
                    x.imports[i].sites == 1 ? "" : "s");
        else
            fprintf(stderr,
                    "v2:   %-12s .%-4u %-24s [%-11s] -> %08X (%u site%s)\n",
                    module_name, x.imports[i].ordinal, "?", "uncatalogued",
                    x.imports[i].address, x.imports[i].sites,
                    x.imports[i].sites == 1 ? "" : "s");
    }
    for (i = 0; i < x.nname_imports; ++i) {
        char nbuf[256];
        fprintf(stderr, "v2:   %-12s .%-20s -> %08X (%u site%s)\n",
               x.modules[x.name_imports[i].module].name,
               import_name_at(&x, x.name_imports[i].name_offset, nbuf, sizeof(nbuf)),
               x.name_imports[i].address, x.name_imports[i].sites,
               x.name_imports[i].sites == 1 ? "" : "s");
    }
    fprintf(stderr, "v2: internal fixups  = %u records\n", x.internal_records);
    fprintf(stderr, "v2: startup          = ENV=%08X PGM=%08X ARG=%08X\n", env_va, pgm_va, arg_va);

    hr = WHvMapGpaRange(rt.partition, rt.ram, 0, RAM_SIZE,
        (WHV_MAP_GPA_RANGE_FLAGS)(WHvMapGpaRangeFlagRead |
                                  WHvMapGpaRangeFlagWrite |
                                  WHvMapGpaRangeFlagExecute));
    if (FAILED(hr)) {
        failed("WHvMapGpaRange", hr);
        goto cleanup;
    }

    hr = WHvCreateVirtualProcessor(rt.partition, 0, 0);
    if (FAILED(hr)) {
        failed("WHvCreateVirtualProcessor", hr);
        goto cleanup;
    }
    vp_created = 1;

    {
        const WHV_REGISTER_NAME names[] = {
            WHvX64RegisterRip, WHvX64RegisterRsp, WHvX64RegisterRflags,
            WHvX64RegisterRax, WHvX64RegisterRbx, WHvX64RegisterRcx,
            WHvX64RegisterRdx, WHvX64RegisterRsi, WHvX64RegisterRdi,
            WHvX64RegisterRbp,
            WHvX64RegisterCr0, WHvX64RegisterCr3, WHvX64RegisterCr4,
            WHvX64RegisterEfer,
            WHvX64RegisterCs, WHvX64RegisterSs, WHvX64RegisterDs,
            WHvX64RegisterEs, WHvX64RegisterFs, WHvX64RegisterGs,
            WHvX64RegisterGdtr
        };
        WHV_REGISTER_VALUE v[sizeof(names) / sizeof(names[0])];
        size_t k = 0;
        memset(v, 0, sizeof(v));
        v[k++].Reg64 = entry;
        v[k++].Reg64 = esp;
        v[k++].Reg64 = 0x00000002u;
        v[k++].Reg64 = entry;       /* V1 leaves EAX holding entry VA */
        v[k++].Reg64 = 0;
        v[k++].Reg64 = 0;
        v[k++].Reg64 = 0;
        v[k++].Reg64 = 0;
        v[k++].Reg64 = 0;
        v[k++].Reg64 = 0;
        v[k++].Reg64 = 0x00000011u; /* CR0 = PE | ET */
        v[k++].Reg64 = 0;
        v[k++].Reg64 = 0;
        v[k++].Reg64 = 0;
        v[k++].Segment = flat_segment(0x08, 0xC09B);
        v[k++].Segment = flat_segment(0x10, 0xC093);
        v[k++].Segment = flat_segment(0x10, 0xC093);
        v[k++].Segment = flat_segment(0x10, 0xC093);
        v[k++].Segment = flat_segment(0x10, 0xC093);
        v[k++].Segment = flat_segment(0x10, 0xC093);
        v[k].Table.Base = GUEST_GDT;
        v[k].Table.Limit = (UINT16)(sizeof(gdt) - 1u);
        ++k;

        hr = WHvSetVirtualProcessorRegisters(rt.partition, 0, names,
            (UINT32)(sizeof(names) / sizeof(names[0])), v);
        if (FAILED(hr)) {
            failed("WHvSetVirtualProcessorRegisters(initial state)", hr);
            goto cleanup;
        }
    }

    /* TID 1 is the original LE entry context. */
    rt.current_thread = 0;
    rt.threads[0].tid = 1;
    rt.threads[0].state = THREAD_RUNNING;
    rt.threads[0].stack_base = x.objects[x.stack_object - 1u].mapped_addr;
    rt.threads[0].stack_size = x.stack_offset;
    hr = save_thread_context(&rt, &rt.threads[0]);
    if (FAILED(hr)) {
        failed("save initial guest thread context", hr);
        goto cleanup;
    }
    init_thread_info(&rt, 0);
    hr = load_thread_context(&rt, &rt.threads[0]);
    if (FAILED(hr)) {
        failed("install initial TIB/FS", hr);
        goto cleanup;
    }
    fprintf(stderr, "v2: scheduler        = TID 1 active; thread-return stub=%08X\n",
           GUEST_THREAD_EXIT_STUB);

    fprintf(stderr, "---------------- guest begins ----------------\n");
    fflush(stdout);
    if(child_boot)InterlockedExchange(&child_boot->ready,1);
    rc = run_guest(&rt, entry, esp);
    fprintf(stderr, "---------------- guest ended -----------------\n");
    if (rc == 0)
        fprintf(stderr, "PASS: untouched OS/2 LE/LX executed under WHP and exited rc=0\n");
    else
        fprintf(stderr, "V2 DLL/threading attempt stopped/returned rc=%d\n", rc);
    goto cleanup;

whp_fail_create:
    failed("WHvCreatePartition", hr);
    goto cleanup;
whp_fail_property:
    failed("WHvSetPartitionProperty(ProcessorCount)", hr);
    goto cleanup;
whp_fail_setup:
    failed("WHvSetupPartition", hr);
    goto cleanup;

cleanup:
    if(child_boot && rt.process_exited) {child_boot->exit_rc=rt.process_rc;child_boot->normal_exit=1;}
    process_cleanup();
    console_cleanup();
    pm_cleanup(&rt);
    for (i = 0; i < MAX_THREADS; ++i) cancel_guest_callbacks(&rt.threads[i]);
    for (i = 0; i < MAX_SEARCHES; ++i)
        if (rt.searches[i].active) FindClose(rt.searches[i].native);
    for (i = 3; i < MAX_FILES; ++i) {
        if (rt.files[i]) CloseHandle(rt.files[i]);
    }
    if (vp_created)
        WHvDeleteVirtualProcessor(rt.partition, 0);
    if (rt.partition)
        WHvDeletePartition(rt.partition);
    if (rt.ram)
        VirtualFree(rt.ram, 0, MEM_RELEASE);
    for (i = 0; i < MAX_GUEST_MODULES; ++i) {
        if (rt.modules[i].used)
            free_le_image(&rt.modules[i].image);
    }
    free_le_image(&x);
    return rc;
}
