/*
 * le2pe386.c - experimental OS/2 LE -> Win32 PE32 translator
 *
 * First target: 32-bit Microsoft C/386 + LINK386 LE executables.
 * Milestone 11: carries multiple LE import modules through to PE32
 * (first targets: DOSCALLS, PMWIN and PMGPI for Sarien), while retaining
 * LE zero-fill support and the Win32 -> OS/2 startup bridge.
 * Deliberately written in conservative ANSI C (C89) so it can be built
 * with old Microsoft C compilers as well as modern GCC/Clang.
 *
 * Current deliberately narrow assumptions:
 *   - LE, little endian, i386, OS/2
 *   - 32-bit objects only
 *   - enumerated pages plus zero-filled pages (LE page-map flag 3)
 *   - iterated/range/compressed pages are not supported yet
 *   - internal OFF32 fixups
 *   - external ORDINAL, SELFREL32 fixups
 *   - external ordinal imports from multiple modules
 *   - fixed PE image base (no PE base relocation section yet)
 *
 * This is a compatibility experiment, not yet a general LE loader.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "os2_api_catalog.h"

#if UINT_MAX == 0xffffffffU
typedef unsigned int U32;
typedef signed int S32;
#elif ULONG_MAX == 0xffffffffUL
typedef unsigned long U32;
typedef signed long S32;
#else
#error Need a 32-bit integer type
#endif

typedef unsigned short U16;
typedef signed short S16;
typedef unsigned char U8;

#define IMAGE_BASE          0x00400000UL
#define FILE_ALIGN          0x00000200UL
#define SECTION_ALIGN       0x00001000UL
#define BOOT_RVA            0x00001000UL
#define PE_OFFSET           0x00000080UL
#define N_SECTIONS          4

#define SRC_MASK            0x0f
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

#define OBJ_READ            0x0001UL
#define OBJ_WRITE           0x0002UL
#define OBJ_EXEC            0x0004UL
#define OBJ_BIG             0x2000UL

#define PE_SCN_CODE         0x00000020UL
#define PE_SCN_INIT_DATA    0x00000040UL
#define PE_SCN_UNINIT_DATA  0x00000080UL
#define PE_SCN_EXECUTE      0x20000000UL
#define PE_SCN_READ         0x40000000UL
#define PE_SCN_WRITE        0x80000000UL

#define MAX_OBJECTS         32
#define MAX_PAGES           65535
#define MAX_IMPORTS         512
#define MAX_MODULES         32

struct LeObject {
    U32 size;
    U32 addr;
    U32 flags;
    U32 mapidx;
    U32 mapsize;
    U32 reserved;
    U32 pe_rva;
    U8 *data;
    U32 init_size;
};

struct LePage {
    U32 physical;
    U8 flags;
};

struct PhysOwner {
    int valid;
    U32 object;
    U32 object_page;
};

struct ImportModule {
    char name[32];
    U32 nimports;
    U32 first_import;
    U32 int_off;
    U32 iat_off;
    U32 name_off;
};

struct ImportOrd {
    U32 module;
    U32 ordinal;
    U32 stub_rva;
    U32 iat_rva;
};

struct Context {
    U8 *in;
    U32 in_size;
    U32 le;

    U32 num_pages;        /* physical file-backed LE pages */
    U32 num_map_pages;    /* logical entries in object page map */
    U32 entry_object;
    U32 entry_offset;
    U32 stack_object;
    U32 stack_offset;
    U32 page_size;
    U32 last_page_size;
    U32 object_table_off;
    U32 num_objects;
    U32 object_map_off;
    U32 fixpage_off;
    U32 fixrec_off;
    U32 impmod_off;
    U32 num_impmods;
    U32 impproc_off;
    U32 data_pages_off;

    struct LeObject objects[MAX_OBJECTS];
    struct LePage *pages;
    struct PhysOwner *owner;

    struct ImportModule modules[MAX_MODULES];
    struct ImportOrd imports[MAX_IMPORTS];
    U32 nimports;

    U32 internal_fixups;
    U32 external_fixups;
};

static U16 rd16(const U8 *p)
{
    return (U16)((U16)p[0] | ((U16)p[1] << 8));
}

static S16 rds16(const U8 *p)
{
    return (S16)rd16(p);
}

static U32 rd32(const U8 *p)
{
    return (U32)p[0] |
           ((U32)p[1] << 8) |
           ((U32)p[2] << 16) |
           ((U32)p[3] << 24);
}

static void wr16(U8 *p, U16 v)
{
    p[0] = (U8)(v & 0xff);
    p[1] = (U8)((v >> 8) & 0xff);
}

static void wr32(U8 *p, U32 v)
{
    p[0] = (U8)(v & 0xff);
    p[1] = (U8)((v >> 8) & 0xff);
    p[2] = (U8)((v >> 16) & 0xff);
    p[3] = (U8)((v >> 24) & 0xff);
}

static U32 align_up(U32 v, U32 a)
{
    return (v + a - 1) & ~(a - 1);
}

static int in_range(struct Context *c, U32 off, U32 cb)
{
    if (off > c->in_size)
        return 0;
    if (cb > c->in_size - off)
        return 0;
    return 1;
}

static void die(const char *s)
{
    fprintf(stderr, "le2pe386: %s\n", s);
    exit(1);
}

static U8 *load_file(const char *name, U32 *size_out)
{
    FILE *f;
    long n;
    U8 *p;

    f = fopen(name, "rb");
    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    n = ftell(f);
    if (n < 0) {
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    p = (U8 *)malloc((size_t)n);
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
    *size_out = (U32)n;
    return p;
}

static int write_file(const char *name, const U8 *p, U32 n)
{
    FILE *f;
    f = fopen(name, "wb");
    if (!f)
        return 0;
    if (n != 0 && fwrite(p, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        return 0;
    }
    if (fclose(f) != 0)
        return 0;
    return 1;
}

static void parse_header(struct Context *c)
{
    U32 h;

    if (c->in_size < 0x40)
        die("file is too small for an MZ header");
    if (c->in[0] != 'M' || c->in[1] != 'Z')
        die("input is not MZ");

    c->le = rd32(c->in + 0x3c);
    if (!in_range(c, c->le, 0xc4))
        die("LE header is outside file");
    if (c->in[c->le] != 'L' || c->in[c->le + 1] != 'E')
        die("input is not an LE executable");
    if (c->in[c->le + 2] != 0 || c->in[c->le + 3] != 0)
        die("big-endian LE is not supported");
    if (rd16(c->in + c->le + 0x08) != 2)
        die("only i386 LE executables are supported");
    if (rd16(c->in + c->le + 0x0a) != 1)
        die("only OS/2 LE executables are supported");

    h = c->le;
    c->num_pages       = rd32(c->in + h + 0x14);
    c->entry_object    = rd32(c->in + h + 0x18);
    c->entry_offset    = rd32(c->in + h + 0x1c);
    c->stack_object    = rd32(c->in + h + 0x20);
    c->stack_offset    = rd32(c->in + h + 0x24);
    c->page_size       = rd32(c->in + h + 0x28);
    c->last_page_size  = rd32(c->in + h + 0x2c);
    c->object_table_off= h + rd32(c->in + h + 0x40);
    c->num_objects     = rd32(c->in + h + 0x44);
    c->object_map_off  = h + rd32(c->in + h + 0x48);
    c->fixpage_off     = h + rd32(c->in + h + 0x68);
    c->fixrec_off      = h + rd32(c->in + h + 0x6c);
    c->impmod_off      = h + rd32(c->in + h + 0x70);
    c->num_impmods     = rd32(c->in + h + 0x74);
    c->impproc_off     = h + rd32(c->in + h + 0x78);

    /* LE page data offset is an absolute file offset, unlike most tables. */
    c->data_pages_off  = rd32(c->in + h + 0x80);

    if (c->num_objects == 0 || c->num_objects > MAX_OBJECTS)
        die("unsupported object count");
    if (c->num_pages == 0 || c->num_pages > MAX_PAGES)
        die("unsupported page count");
    if (c->page_size == 0 || (c->page_size & (c->page_size - 1)) != 0)
        die("invalid LE page size");
    if (!in_range(c, c->object_table_off, c->num_objects * 24))
        die("bad LE object table");
    if (!in_range(c, c->fixpage_off, (c->num_pages + 1) * 4))
        die("bad LE fixup page table");
}

static void parse_objects(struct Context *c)
{
    U32 i;
    U32 j;
    U32 off;
    U32 phys;
    U32 src;
    U32 room;
    U32 actual;
    U32 last_map;
    U8 *m;
    U8 pflags;
    struct LeObject *o;

    /*
     * e32_mpages is the number of physical data pages in the file.  It is
     * NOT necessarily the number of entries in the object page map.  Large
     * BSS/stack objects may contain logical page-map entries marked ZEROED
     * (flag 3), which consume no physical data page.  LINK386 uses exactly
     * this representation for sufficiently large uninitialised objects.
     *
     * Read the object descriptors first and derive the number of logical
     * page-map entries from their map ranges.
     */
    last_map = 0;
    for (i = 0; i < c->num_objects; ++i) {
        off = c->object_table_off + i * 24;
        o = &c->objects[i];
        o->size     = rd32(c->in + off + 0);
        o->addr     = rd32(c->in + off + 4);
        o->flags    = rd32(c->in + off + 8);
        o->mapidx   = rd32(c->in + off + 12);
        o->mapsize  = rd32(c->in + off + 16);
        o->reserved = rd32(c->in + off + 20);

        if ((o->flags & OBJ_BIG) == 0)
            die("16-bit LE objects are not supported yet");

        if (o->mapsize != 0) {
            if (o->mapidx == 0)
                die("object has invalid page-map index");
            if (o->mapidx - 1 > 0xffffffffUL - o->mapsize)
                die("object page-map range overflows");
            if (o->mapidx - 1 + o->mapsize > last_map)
                last_map = o->mapidx - 1 + o->mapsize;
        }
    }

    if (last_map == 0 || last_map > MAX_PAGES)
        die("unsupported logical LE page-map size");
    c->num_map_pages = last_map;
    if (!in_range(c, c->object_map_off, c->num_map_pages * 4))
        die("bad LE object page map");

    c->pages = (struct LePage *)calloc((size_t)c->num_map_pages,
                                        sizeof(struct LePage));
    c->owner = (struct PhysOwner *)calloc((size_t)c->num_pages + 1,
                                           sizeof(struct PhysOwner));
    if (!c->pages || !c->owner)
        die("out of memory");

    for (i = 0; i < c->num_map_pages; ++i) {
        m = c->in + c->object_map_off + i * 4;
        /* LE page number is a 24-bit big-order quantity even in LE files. */
        phys = ((U32)m[0] << 16) | ((U32)m[1] << 8) | (U32)m[2];
        pflags = m[3];
        c->pages[i].physical = phys;
        c->pages[i].flags = pflags;

        if (pflags == 0) {
            if (phys == 0 || phys > c->num_pages)
                die("bad LE physical page number");
        } else if (pflags == 3) {
            /*
             * ZEROED logical page.  The 24-bit page-number field is not a
             * physical file-page reference and is deliberately ignored.
             */
        } else {
            die("prototype supports only enumerated and zero-filled LE pages");
        }
    }

    for (i = 0; i < c->num_objects; ++i) {
        o = &c->objects[i];
        if (o->mapsize != 0 &&
            (o->mapidx == 0 || o->mapidx - 1 + o->mapsize > c->num_map_pages))
            die("object has invalid page-map range");

        o->data = (U8 *)calloc(1, (size_t)o->size);
        if (!o->data)
            die("out of memory allocating object");
        o->init_size = 0;

        for (j = 0; j < o->mapsize; ++j) {
            struct LePage *lp;
            lp = &c->pages[o->mapidx - 1 + j];

            if (lp->flags == 3) {
                /* calloc() already supplied the zero-filled logical page. */
                continue;
            }

            phys = lp->physical;
            if (c->owner[phys].valid)
                die("physical LE page belongs to more than one object");
            c->owner[phys].valid = 1;
            c->owner[phys].object = i;
            c->owner[phys].object_page = j;

            src = c->data_pages_off + (phys - 1) * c->page_size;
            room = c->page_size;
            if (j * c->page_size >= o->size)
                room = 0;
            else if (room > o->size - j * c->page_size)
                room = o->size - j * c->page_size;
            actual = room;
            if (phys == c->num_pages && actual > c->last_page_size)
                actual = c->last_page_size;
            if (!in_range(c, src, actual))
                die("LE page data extends past EOF");
            if (actual != 0)
                memcpy(o->data + j * c->page_size, c->in + src, (size_t)actual);
            if (j * c->page_size + actual > o->init_size)
                o->init_size = j * c->page_size + actual;
        }
    }
}

static void parse_import_modules(struct Context *c)
{
    U32 i;
    U32 p;
    U8 n;

    if (c->num_impmods == 0 || c->num_impmods > MAX_MODULES)
        die("unsupported number of imported modules");

    p = c->impmod_off;
    for (i = 0; i < c->num_impmods; ++i) {
        if (!in_range(c, p, 1))
            die("bad import module table");
        n = c->in[p++];
        if (n == 0 || n >= sizeof(c->modules[i].name) || !in_range(c, p, n))
            die("bad import module name");
        memcpy(c->modules[i].name, c->in + p, n);
        c->modules[i].name[n] = 0;
        c->modules[i].nimports = 0;
        c->modules[i].first_import = 0;
        c->modules[i].int_off = 0;
        c->modules[i].iat_off = 0;
        c->modules[i].name_off = 0;
        p += n;
    }
}

static U32 find_or_add_import(struct Context *c, U32 module, U32 ordinal)
{
    U32 i;
    if (module >= c->num_impmods)
        die("external fixup references invalid module");
    for (i = 0; i < c->nimports; ++i) {
        if (c->imports[i].module == module && c->imports[i].ordinal == ordinal)
            return i;
    }
    if (c->nimports >= MAX_IMPORTS)
        die("too many imported ordinals");
    c->imports[c->nimports].module = module;
    c->imports[c->nimports].ordinal = ordinal;
    c->imports[c->nimports].stub_rva = 0;
    c->imports[c->nimports].iat_rva = 0;
    ++c->nimports;
    return c->nimports - 1;
}

static int import_cmp(const void *a, const void *b)
{
    const struct ImportOrd *ia;
    const struct ImportOrd *ib;
    ia = (const struct ImportOrd *)a;
    ib = (const struct ImportOrd *)b;
    if (ia->module < ib->module)
        return -1;
    if (ia->module > ib->module)
        return 1;
    if (ia->ordinal < ib->ordinal)
        return -1;
    if (ia->ordinal > ib->ordinal)
        return 1;
    return 0;
}

static void summarize_imports(struct Context *c)
{
    U32 i;
    U32 m;

    for (m = 0; m < c->num_impmods; ++m) {
        c->modules[m].nimports = 0;
        c->modules[m].first_import = c->nimports;
    }
    for (i = 0; i < c->nimports; ++i) {
        m = c->imports[i].module;
        if (c->modules[m].nimports == 0)
            c->modules[m].first_import = i;
        ++c->modules[m].nimports;
    }
}

static U32 import_index(struct Context *c, U32 module, U32 ordinal)
{
    U32 i;
    for (i = 0; i < c->nimports; ++i) {
        if (c->imports[i].module == module && c->imports[i].ordinal == ordinal)
            return i;
    }
    die("internal error: missing imported ordinal");
    return 0;
}

static U32 source_object_offset(struct Context *c, U32 physical_page, S16 src)
{
    struct PhysOwner *po;
    struct LeObject *o;
    S32 v;

    if (physical_page == 0 || physical_page > c->num_pages)
        die("fixup references invalid physical page");
    po = &c->owner[physical_page];
    if (!po->valid)
        die("fixup physical page has no object owner");
    o = &c->objects[po->object];
    v = (S32)(po->object_page * c->page_size) + (S32)src;
    if (v < 0 || (U32)v + 4 > o->size)
        die("fixup source lies outside object");
    return (U32)v;
}

static U32 skip_objmod(const U8 *r, U32 *pos, U8 flags)
{
    U32 x;
    if (flags & TGT_OBJ16) {
        x = rd16(r + *pos);
        *pos += 2;
    } else {
        x = r[*pos];
        *pos += 1;
    }
    return x;
}

/* First pass: discover external ordinals and validate the supported subset. */
static void scan_fixups(struct Context *c)
{
    U32 physical;
    U32 start;
    U32 end;
    U32 p;
    U32 q;
    U32 first;
    U32 target;
    U32 count;
    U32 i;
    U8 type;
    U8 flags;
    U8 target_kind;

    for (physical = 1; physical <= c->num_pages; ++physical) {
        start = rd32(c->in + c->fixpage_off + (physical - 1) * 4);
        end   = rd32(c->in + c->fixpage_off + physical * 4);
        p = c->fixrec_off + start;
        end = c->fixrec_off + end;
        if (p > end || end > c->impmod_off)
            die("bad fixup-record range");

        while (p < end) {
            type = c->in[p++];
            flags = c->in[p++];
            if ((type & SRC_MASK) != SRC_OFF32 && (type & SRC_MASK) != SRC_REL32)
                die("unsupported LE source-fixup type");
            if (type & SRC_LIST) {
                count = c->in[p++];
            } else {
                count = 1;
                p += 2;
            }
            q = p;
            first = skip_objmod(c->in, &q, flags);
            target_kind = flags & TGT_MASK;

            if (flags & TGT_CHAIN)
                die("chained LE fixups are not supported yet");

            if (target_kind == TGT_INTERNAL) {
                if ((type & SRC_MASK) != SRC_OFF32)
                    die("prototype expects internal fixups to be OFF32");
                if (flags & TGT_OFF32)
                    q += 4;
                else
                    q += 2;
                if (first == 0 || first > c->num_objects)
                    die("internal fixup has bad target object");
                ++c->internal_fixups;
            } else if (target_kind == TGT_EXT_ORD) {
                if ((type & SRC_MASK) != SRC_REL32)
                    die("prototype expects external ordinal fixups to be REL32");
                if (first == 0 || first > c->num_impmods)
                    die("external fixup references invalid import module");
                if (flags & TGT_ORD8) {
                    target = c->in[q++];
                } else if (flags & TGT_OFF32) {
                    target = rd32(c->in + q);
                    q += 4;
                } else {
                    target = rd16(c->in + q);
                    q += 2;
                }
                find_or_add_import(c, first - 1, target);
                if (flags & TGT_ADDITIVE)
                    q += (flags & TGT_ADD32) ? 4 : 2;
                c->external_fixups += count;
            } else if (target_kind == TGT_EXT_NAME) {
                die("import-by-name LE fixups are not supported yet");
            } else if (target_kind == TGT_INT_ENTRY) {
                die("entry-table LE fixups are not supported yet");
            } else {
                die("unknown LE fixup target kind");
            }

            p = q;
            if (type & SRC_LIST) {
                if (p + count * 2 > end)
                    die("fixup source list extends past page records");
                for (i = 0; i < count; ++i) {
                    (void)source_object_offset(c, physical, rds16(c->in + p));
                    p += 2;
                }
            }
            if (p > end)
                die("fixup record extends past page record area");
        }
        if (p != end)
            die("fixup page did not end on a record boundary");
    }

    qsort(c->imports, (size_t)c->nimports, sizeof(c->imports[0]), import_cmp);
    summarize_imports(c);
}

/* Second pass: apply LE relocations to reconstructed object bytes. */
static void apply_fixups(struct Context *c)
{
    U32 physical;
    U32 start;
    U32 endoff;
    U32 p;
    U32 end;
    U32 q;
    U32 first;
    U32 target;
    U32 target_off;
    U32 count;
    U32 i;
    U32 source_off;
    U32 src_va;
    U32 dst_va;
    U32 idx;
    U8 type;
    U8 flags;
    U8 target_kind;
    S16 source;
    struct PhysOwner *po;
    struct LeObject *src_obj;
    struct LeObject *dst_obj;

    for (physical = 1; physical <= c->num_pages; ++physical) {
        start = rd32(c->in + c->fixpage_off + (physical - 1) * 4);
        endoff = rd32(c->in + c->fixpage_off + physical * 4);
        p = c->fixrec_off + start;
        end = c->fixrec_off + endoff;

        while (p < end) {
            type = c->in[p++];
            flags = c->in[p++];
            if (type & SRC_LIST) {
                count = c->in[p++];
                source = 0;
            } else {
                count = 1;
                source = rds16(c->in + p);
                p += 2;
            }
            q = p;
            first = skip_objmod(c->in, &q, flags);
            target_kind = flags & TGT_MASK;
            target = 0;
            target_off = 0;

            if (target_kind == TGT_INTERNAL) {
                if (flags & TGT_OFF32) {
                    target_off = rd32(c->in + q);
                    q += 4;
                } else {
                    target_off = rd16(c->in + q);
                    q += 2;
                }
                target = first;
            } else if (target_kind == TGT_EXT_ORD) {
                if (flags & TGT_ORD8) {
                    target = c->in[q++];
                } else if (flags & TGT_OFF32) {
                    target = rd32(c->in + q);
                    q += 4;
                } else {
                    target = rd16(c->in + q);
                    q += 2;
                }
                if (flags & TGT_ADDITIVE)
                    q += (flags & TGT_ADD32) ? 4 : 2;
            } else {
                die("unsupported fixup slipped through validation");
            }

            p = q;
            for (i = 0; i < count; ++i) {
                if (type & SRC_LIST) {
                    source = rds16(c->in + p);
                    p += 2;
                }

                source_off = source_object_offset(c, physical, source);
                po = &c->owner[physical];
                src_obj = &c->objects[po->object];
                src_va = IMAGE_BASE + src_obj->pe_rva + source_off;

                if (target_kind == TGT_INTERNAL) {
                    dst_obj = &c->objects[target - 1];
                    if (target_off >= dst_obj->size)
                        die("internal fixup target lies outside object");
                    dst_va = IMAGE_BASE + dst_obj->pe_rva + target_off;
                    wr32(src_obj->data + source_off, dst_va);
                } else {
                    idx = import_index(c, first - 1, target);
                    dst_va = IMAGE_BASE + c->imports[idx].stub_rva;
                    wr32(src_obj->data + source_off, dst_va - (src_va + 4));
                }
            }
        }
    }
}

static void write_section_header(U8 *p, const char *name, U32 vsize, U32 rva,
                                 U32 rawsize, U32 rawoff, U32 characteristics)
{
    size_t n;
    memset(p, 0, 40);
    n = strlen(name);
    if (n > 8)
        n = 8;
    memcpy(p, name, n);
    wr32(p + 8, vsize);
    wr32(p + 12, rva);
    wr32(p + 16, rawsize);
    wr32(p + 20, rawoff);
    wr32(p + 36, characteristics);
}

static const char *base_name(const char *p)
{
    const char *q;
    const char *last;
    last = p;
    q = p;
    while (*q) {
        if (*q == '/' || *q == '\\')
            last = q + 1;
        ++q;
    }
    return last;
}

static void patch_rel32(U8 *code, U32 disp_off, U32 target_off)
{
    S32 d;
    d = (S32)target_off - (S32)(disp_off + 4);
    wr32(code + disp_off, (U32)d);
}

static void build_pe(struct Context *c, const char *out_name)
{
    U32 headers_size;
    U32 boot_raw;
    U32 boot_raw_size;
    U32 text_raw;
    U32 text_raw_size;
    U32 data_raw;
    U32 data_raw_size;
    U32 idata_raw;
    U32 idata_raw_size;
    U32 file_size;
    U32 idata_size;
    U32 desc_off;
    U32 kern_int_off;
    U32 kern_iat_off;
    U32 kern_name_off;
    U32 gcla_name_off;
    U32 gesa_name_off;
    U32 env_off;
    U32 cmd_block_off;
    U32 cmdptr_off;
    U32 iat_dir_off;
    U32 iat_dir_size;
    U32 used_mods;
    U32 desc_index;
    U32 cursor;
    U32 m;
    U32 local_index;
    U32 i;
    U32 p;
    U32 thunk_off;
    U32 entry_va;
    U32 stack_va;
    U32 text_vsize;
    U32 data_vsize;
    U32 size_image;
    U32 opt;
    U32 coff;
    U32 sec;
    U32 text_obj;
    U32 data_obj;
    U32 text_rva;
    U32 data_rva;
    U32 idata_rva;
    U32 gcla_iat_va;
    U32 gesa_iat_va;
    U32 cmdptr_va;
    U32 argbuf_va;
    U32 jne_unquoted;
    U32 jz_quoted_done;
    U32 jne_quoted_loop;
    U32 jmp_skip_ws;
    U32 jz_unquoted_done;
    U32 je_unquoted_space;
    U32 je_unquoted_tab;
    U32 je_ws_space;
    U32 jne_ws_done;
    U32 jmp_ws_loop;
    U32 jz_copy_done;
    U32 jne_copy_loop;
    U32 quoted_loop;
    U32 unquoted_loop;
    U32 skip_ws;
    U32 ws_inc;
    U32 copy_start;
    U32 copy_loop;
    U32 copy_done;
    U8 *out;
    U8 *boot;
    const char *bn;
    const char *api_name;
    size_t bnlen;
    struct LeObject *to;
    struct LeObject *dobj;
    char dllname[40];

#define CMD_BUF_SIZE 1024UL

    text_obj = 0xffffffffUL;
    data_obj = 0xffffffffUL;
    for (i = 0; i < c->num_objects; ++i) {
        if ((c->objects[i].flags & OBJ_EXEC) && text_obj == 0xffffffffUL)
            text_obj = i;
        if ((c->objects[i].flags & OBJ_WRITE) && !(c->objects[i].flags & OBJ_EXEC) &&
            data_obj == 0xffffffffUL)
            data_obj = i;
    }
    if (text_obj == 0xffffffffUL || data_obj == 0xffffffffUL)
        die("could not identify code/data objects");
    if (c->entry_object - 1 != text_obj)
        die("entry object is not the executable object");

    to = &c->objects[text_obj];
    dobj = &c->objects[data_obj];
    text_vsize = to->size;
    data_vsize = dobj->size;

    bn = base_name(out_name);
    bnlen = strlen(bn);
    if (bnlen > 120)
        bnlen = 120;

    used_mods = 0;
    for (m = 0; m < c->num_impmods; ++m) {
        if (c->modules[m].nimports != 0)
            ++used_mods;
    }

    /* Leave enough room for the startup adapter and one six-byte absolute
       jump thunk per unique imported ordinal. */
    boot_raw_size = align_up(384UL + c->nimports * 6UL, FILE_ALIGN);
    if (boot_raw_size < FILE_ALIGN)
        boot_raw_size = FILE_ALIGN;

    text_rva = align_up(BOOT_RVA + boot_raw_size, SECTION_ALIGN);
    data_rva = align_up(text_rva + align_up(text_vsize, SECTION_ALIGN), SECTION_ALIGN);
    idata_rva = align_up(data_rva + align_up(data_vsize, SECTION_ALIGN), SECTION_ALIGN);
    to->pe_rva = text_rva;
    dobj->pe_rva = data_rva;

    headers_size = align_up(PE_OFFSET + 4 + 20 + 224 + N_SECTIONS * 40, FILE_ALIGN);
    boot_raw = headers_size;
    text_raw = boot_raw + boot_raw_size;
    text_raw_size = align_up(to->init_size, FILE_ALIGN);
    data_raw = text_raw + text_raw_size;
    data_raw_size = align_up(dobj->init_size, FILE_ALIGN);

    /* Build one PE import descriptor for every LE module that is actually
       referenced, plus KERNEL32 for the synthetic process-startup bridge. */
    desc_off = 0;
    cursor = (used_mods + 2) * 20; /* used LE modules + KERNEL32 + NULL */

    for (m = 0; m < c->num_impmods; ++m) {
        if (c->modules[m].nimports == 0)
            continue;
        c->modules[m].int_off = cursor;
        cursor += (c->modules[m].nimports + 1) * 4;
    }
    kern_int_off = cursor;
    cursor += 12; /* two name imports plus terminator */

    iat_dir_off = 0xffffffffUL;
    for (m = 0; m < c->num_impmods; ++m) {
        if (c->modules[m].nimports == 0)
            continue;
        c->modules[m].iat_off = cursor;
        if (iat_dir_off == 0xffffffffUL)
            iat_dir_off = cursor;
        cursor += (c->modules[m].nimports + 1) * 4;
    }
    kern_iat_off = cursor;
    if (iat_dir_off == 0xffffffffUL)
        iat_dir_off = kern_iat_off;
    cursor += 12;
    iat_dir_size = cursor - iat_dir_off;

    for (m = 0; m < c->num_impmods; ++m) {
        if (c->modules[m].nimports == 0)
            continue;
        c->modules[m].name_off = cursor;
        cursor += (U32)strlen(c->modules[m].name) + 5; /* .dll + NUL */
    }
    kern_name_off = cursor;
    cursor += 13;
    gcla_name_off = align_up(cursor, 2);
    cursor = gcla_name_off + 2 + 16;
    gesa_name_off = align_up(cursor, 2);
    cursor = gesa_name_off + 2 + 23;
    env_off = align_up(cursor, 4);
    cmd_block_off = env_off;
    cmdptr_off = cmd_block_off + 1 + (U32)bnlen + 1;
    idata_size = cmdptr_off + CMD_BUF_SIZE;

    idata_raw_size = align_up(idata_size, FILE_ALIGN);
    idata_raw = data_raw + data_raw_size;
    file_size = idata_raw + idata_raw_size;
    size_image = align_up(idata_rva + idata_size, SECTION_ALIGN);

    out = (U8 *)calloc(1, (size_t)file_size);
    if (!out)
        die("out of memory building PE");

    out[0] = 'M'; out[1] = 'Z';
    wr16(out + 2, 0x0090);
    wr16(out + 4, 0x0003);
    wr16(out + 8, 0x0004);
    wr16(out + 0x18, 0x0040);
    wr32(out + 0x3c, PE_OFFSET);
    memcpy(out + 0x40, "This program requires Win32.\r\n$", 31);

    p = PE_OFFSET;
    out[p++] = 'P'; out[p++] = 'E'; out[p++] = 0; out[p++] = 0;
    coff = p;
    wr16(out + coff + 0, 0x014c);
    wr16(out + coff + 2, N_SECTIONS);
    wr32(out + coff + 4, 0);
    wr16(out + coff + 16, 224);
    wr16(out + coff + 18, 0x0103);

    opt = coff + 20;
    wr16(out + opt + 0, 0x010b);
    out[opt + 2] = 6;
    out[opt + 3] = 0;
    wr32(out + opt + 4, boot_raw_size + text_raw_size);
    wr32(out + opt + 8, data_raw_size + idata_raw_size);
    wr32(out + opt + 12, data_vsize > dobj->init_size ? data_vsize - dobj->init_size : 0);
    wr32(out + opt + 16, BOOT_RVA);
    wr32(out + opt + 20, BOOT_RVA);
    wr32(out + opt + 24, data_rva);
    wr32(out + opt + 28, IMAGE_BASE);
    wr32(out + opt + 32, SECTION_ALIGN);
    wr32(out + opt + 36, FILE_ALIGN);
    wr16(out + opt + 40, 4);
    wr16(out + opt + 48, 4);
    wr32(out + opt + 56, size_image);
    wr32(out + opt + 60, headers_size);
    wr16(out + opt + 68, 3);
    wr16(out + opt + 70, 0);
    wr32(out + opt + 72, 0x00100000UL);
    wr32(out + opt + 76, 0x00001000UL);
    wr32(out + opt + 80, 0x00100000UL);
    wr32(out + opt + 84, 0x00001000UL);
    wr32(out + opt + 92, 16);
    wr32(out + opt + 96 + 8, idata_rva + desc_off);
    wr32(out + opt + 96 + 12, idata_size);
    wr32(out + opt + 96 + 8 * 12, idata_rva + iat_dir_off);
    wr32(out + opt + 96 + 8 * 12 + 4, iat_dir_size);

    sec = opt + 224;
    write_section_header(out + sec + 0 * 40, ".boot", boot_raw_size, BOOT_RVA,
                         boot_raw_size, boot_raw,
                         PE_SCN_CODE | PE_SCN_EXECUTE | PE_SCN_READ);
    write_section_header(out + sec + 1 * 40, ".text", text_vsize, text_rva,
                         text_raw_size, text_raw,
                         PE_SCN_CODE | PE_SCN_EXECUTE | PE_SCN_READ);
    write_section_header(out + sec + 2 * 40, ".data", data_vsize, data_rva,
                         data_raw_size, data_raw,
                         PE_SCN_INIT_DATA | PE_SCN_READ | PE_SCN_WRITE);
    write_section_header(out + sec + 3 * 40, ".idata", idata_size, idata_rva,
                         idata_raw_size, idata_raw,
                         PE_SCN_INIT_DATA | PE_SCN_READ | PE_SCN_WRITE);

    desc_index = 0;
    for (m = 0; m < c->num_impmods; ++m) {
        U32 d;
        if (c->modules[m].nimports == 0)
            continue;
        d = desc_off + desc_index * 20;
        wr32(out + idata_raw + d + 0, idata_rva + c->modules[m].int_off);
        wr32(out + idata_raw + d + 12, idata_rva + c->modules[m].name_off);
        wr32(out + idata_raw + d + 16, idata_rva + c->modules[m].iat_off);
        ++desc_index;

        sprintf(dllname, "%s.dll", c->modules[m].name);
        memcpy(out + idata_raw + c->modules[m].name_off,
               dllname, strlen(dllname) + 1);
    }

    /* Synthetic KERNEL32 descriptor follows the translated LE modules. */
    wr32(out + idata_raw + desc_off + desc_index * 20 + 0, idata_rva + kern_int_off);
    wr32(out + idata_raw + desc_off + desc_index * 20 + 12, idata_rva + kern_name_off);
    wr32(out + idata_raw + desc_off + desc_index * 20 + 16, idata_rva + kern_iat_off);

    memcpy(out + idata_raw + kern_name_off, "KERNEL32.dll", 13);
    wr16(out + idata_raw + gcla_name_off, 0);
    memcpy(out + idata_raw + gcla_name_off + 2, "GetCommandLineA", 16);
    wr16(out + idata_raw + gesa_name_off, 0);
    memcpy(out + idata_raw + gesa_name_off + 2, "GetEnvironmentStringsA", 23);

    /* Populate each translated module's INT/IAT with ordinal imports. */
    for (i = 0; i < c->nimports; ++i) {
        m = c->imports[i].module;
        local_index = i - c->modules[m].first_import;
        c->imports[i].iat_rva = idata_rva + c->modules[m].iat_off + local_index * 4;
        wr32(out + idata_raw + c->modules[m].int_off + local_index * 4,
             0x80000000UL | c->imports[i].ordinal);
        wr32(out + idata_raw + c->modules[m].iat_off + local_index * 4,
             0x80000000UL | c->imports[i].ordinal);
    }

    wr32(out + idata_raw + kern_int_off + 0, idata_rva + gcla_name_off);
    wr32(out + idata_raw + kern_int_off + 4, idata_rva + gesa_name_off);
    wr32(out + idata_raw + kern_iat_off + 0, idata_rva + gcla_name_off);
    wr32(out + idata_raw + kern_iat_off + 4, idata_rva + gesa_name_off);

    out[idata_raw + cmd_block_off] = 0;
    memcpy(out + idata_raw + cmd_block_off + 1, bn, bnlen);
    out[idata_raw + cmd_block_off + 1 + bnlen] = 0;
    out[idata_raw + cmdptr_off] = 0;

    gcla_iat_va = IMAGE_BASE + idata_rva + kern_iat_off;
    gesa_iat_va = IMAGE_BASE + idata_rva + kern_iat_off + 4;
    cmdptr_va = IMAGE_BASE + idata_rva + cmd_block_off + 1;
    argbuf_va = IMAGE_BASE + idata_rva + cmdptr_off;

    boot = out + boot_raw;
    p = 0;

    boot[p++] = 0xff; boot[p++] = 0x15; wr32(boot + p, gcla_iat_va); p += 4;
    boot[p++] = 0x89; boot[p++] = 0xc6;
    boot[p++] = 0x80; boot[p++] = 0x3e; boot[p++] = 0x22;
    boot[p++] = 0x0f; boot[p++] = 0x85; jne_unquoted = p; p += 4;

    boot[p++] = 0x46;
    quoted_loop = p;
    boot[p++] = 0x8a; boot[p++] = 0x06;
    boot[p++] = 0x84; boot[p++] = 0xc0;
    boot[p++] = 0x0f; boot[p++] = 0x84; jz_quoted_done = p; p += 4;
    boot[p++] = 0x46;
    boot[p++] = 0x3c; boot[p++] = 0x22;
    boot[p++] = 0x0f; boot[p++] = 0x85; jne_quoted_loop = p; p += 4;
    boot[p++] = 0xe9; jmp_skip_ws = p; p += 4;

    unquoted_loop = p;
    boot[p++] = 0x8a; boot[p++] = 0x06;
    boot[p++] = 0x84; boot[p++] = 0xc0;
    boot[p++] = 0x0f; boot[p++] = 0x84; jz_unquoted_done = p; p += 4;
    boot[p++] = 0x3c; boot[p++] = 0x20;
    boot[p++] = 0x0f; boot[p++] = 0x84; je_unquoted_space = p; p += 4;
    boot[p++] = 0x3c; boot[p++] = 0x09;
    boot[p++] = 0x0f; boot[p++] = 0x84; je_unquoted_tab = p; p += 4;
    boot[p++] = 0x46;
    boot[p++] = 0xe9; { U32 d = p; p += 4; patch_rel32(boot, d, unquoted_loop); }

    skip_ws = p;
    boot[p++] = 0x8a; boot[p++] = 0x06;
    boot[p++] = 0x3c; boot[p++] = 0x20;
    boot[p++] = 0x0f; boot[p++] = 0x84; je_ws_space = p; p += 4;
    boot[p++] = 0x3c; boot[p++] = 0x09;
    boot[p++] = 0x0f; boot[p++] = 0x85; jne_ws_done = p; p += 4;
    ws_inc = p;
    boot[p++] = 0x46;
    boot[p++] = 0xe9; jmp_ws_loop = p; p += 4;

    copy_start = p;
    boot[p++] = 0xbf; wr32(boot + p, argbuf_va); p += 4;
    boot[p++] = 0xb9; wr32(boot + p, CMD_BUF_SIZE - 1); p += 4;
    copy_loop = p;
    boot[p++] = 0x8a; boot[p++] = 0x06;
    boot[p++] = 0x88; boot[p++] = 0x07;
    boot[p++] = 0x84; boot[p++] = 0xc0;
    boot[p++] = 0x0f; boot[p++] = 0x84; jz_copy_done = p; p += 4;
    boot[p++] = 0x46;
    boot[p++] = 0x47;
    boot[p++] = 0x49;
    boot[p++] = 0x0f; boot[p++] = 0x85; jne_copy_loop = p; p += 4;
    boot[p++] = 0xc6; boot[p++] = 0x07; boot[p++] = 0x00;
    copy_done = p;

    patch_rel32(boot, jne_unquoted, unquoted_loop);
    patch_rel32(boot, jz_quoted_done, copy_start);
    patch_rel32(boot, jne_quoted_loop, quoted_loop);
    patch_rel32(boot, jmp_skip_ws, skip_ws);
    patch_rel32(boot, jz_unquoted_done, copy_start);
    patch_rel32(boot, je_unquoted_space, skip_ws);
    patch_rel32(boot, je_unquoted_tab, skip_ws);
    patch_rel32(boot, je_ws_space, ws_inc);
    patch_rel32(boot, jne_ws_done, copy_start);
    patch_rel32(boot, jmp_ws_loop, skip_ws);
    patch_rel32(boot, jz_copy_done, copy_done);
    patch_rel32(boot, jne_copy_loop, copy_loop);

    boot[p++] = 0xff; boot[p++] = 0x15; wr32(boot + p, gesa_iat_va); p += 4;
    boot[p++] = 0x89; boot[p++] = 0xc2;

    entry_va = IMAGE_BASE + c->objects[c->entry_object - 1].pe_rva + c->entry_offset;
    if (c->stack_object == 0 || c->stack_object > c->num_objects)
        die("invalid LE stack object");
    if (c->stack_offset > c->objects[c->stack_object - 1].size)
        die("LE stack offset lies outside stack object");
    stack_va = IMAGE_BASE + c->objects[c->stack_object - 1].pe_rva + c->stack_offset;

    boot[p++] = 0xbc; wr32(boot + p, stack_va); p += 4;
    boot[p++] = 0x68; wr32(boot + p, cmdptr_va); p += 4;
    boot[p++] = 0x52;
    boot[p++] = 0x6a; boot[p++] = 0x00;
    boot[p++] = 0x6a; boot[p++] = 0x00;
    boot[p++] = 0x6a; boot[p++] = 0x00;
    boot[p++] = 0x31; boot[p++] = 0xdb;
    boot[p++] = 0x31; boot[p++] = 0xc9;
    boot[p++] = 0x31; boot[p++] = 0xd2;
    boot[p++] = 0x31; boot[p++] = 0xf6;
    boot[p++] = 0x31; boot[p++] = 0xff;
    boot[p++] = 0x31; boot[p++] = 0xed;
    boot[p++] = 0xb8; wr32(boot + p, entry_va); p += 4;
    boot[p++] = 0xff; boot[p++] = 0xe0;

    if (p > boot_raw_size)
        die(".boot startup code overflow");

    p = align_up(p, 4);
    thunk_off = p;
    for (i = 0; i < c->nimports; ++i) {
        if (p + 6 > boot_raw_size)
            die(".boot import thunk overflow");
        c->imports[i].stub_rva = BOOT_RVA + p;
        boot[p + 0] = 0xff;
        boot[p + 1] = 0x25;
        wr32(boot + p + 2, IMAGE_BASE + c->imports[i].iat_rva);
        p += 6;
    }

    apply_fixups(c);

    memcpy(out + text_raw, to->data, (size_t)to->init_size);
    memcpy(out + data_raw, dobj->data, (size_t)dobj->init_size);

    if (!write_file(out_name, out, file_size))
        die("could not write output file");

    printf("LE2PE386: wrote %s\n", out_name);
    printf("  image base      %08lx\n", (unsigned long)IMAGE_BASE);
    printf("  original entry  obj %lu + %08lx -> VA %08lx\n",
           (unsigned long)c->entry_object,
           (unsigned long)c->entry_offset,
           (unsigned long)entry_va);
    printf("  translated stack obj %lu + %08lx -> VA %08lx\n",
           (unsigned long)c->stack_object,
           (unsigned long)c->stack_offset,
           (unsigned long)stack_va);
    printf("  PE RVAs         text %08lx data %08lx idata %08lx\n",
           (unsigned long)text_rva, (unsigned long)data_rva, (unsigned long)idata_rva);
    printf("  objects         %lu\n", (unsigned long)c->num_objects);
    printf("  LE pages        %lu physical, %lu logical map entries\n",
           (unsigned long)c->num_pages,
           (unsigned long)c->num_map_pages);
    printf("  internal fixups %lu records\n", (unsigned long)c->internal_fixups);
    printf("  external calls  %lu sites\n", (unsigned long)c->external_fixups);
    printf("  imported modules:\n");
    for (m = 0; m < c->num_impmods; ++m) {
        if (c->modules[m].nimports == 0)
            continue;
        printf("    %s:", c->modules[m].name);
        for (i = c->modules[m].first_import;
             i < c->modules[m].first_import + c->modules[m].nimports; ++i) {
            api_name = os2_api_name(c->modules[m].name,
                                    (uint32_t)c->imports[i].ordinal);
            if (api_name != NULL)
                printf(" %lu(%s)", (unsigned long)c->imports[i].ordinal,
                       api_name);
            else
                printf(" %lu", (unsigned long)c->imports[i].ordinal);
        }
        printf("\n");
    }
    printf("  Win32 bootstrap imports: KERNEL32.GetCommandLineA, GetEnvironmentStringsA\n");
    printf("  environment     Win32 ANSI block -> original C/386 CRT\n");
    printf("  argv bridge     enabled (OS/2 program-name\\0argument-tail block)\n");

    (void)thunk_off;
    free(out);
#undef CMD_BUF_SIZE
}

static void cleanup(struct Context *c)
{
    U32 i;
    for (i = 0; i < c->num_objects; ++i)
        free(c->objects[i].data);
    free(c->pages);
    free(c->owner);
    free(c->in);
}

int main(int argc, char **argv)
{
    struct Context c;

    if (argc != 3) {
        fprintf(stderr, "usage: le2pe386 input-le.exe output-pe.exe\n");
        return 2;
    }

    memset(&c, 0, sizeof(c));
    c.in = load_file(argv[1], &c.in_size);
    if (!c.in) {
        fprintf(stderr, "le2pe386: cannot read %s\n", argv[1]);
        return 1;
    }

    parse_header(&c);
    parse_objects(&c);
    parse_import_modules(&c);
    scan_fixups(&c);
    build_pe(&c, argv[2]);
    cleanup(&c);
    return 0;
}
