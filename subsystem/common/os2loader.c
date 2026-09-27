/*
 * Host-independent OS/2 LE/LX parser/planner.
 *
 * Extracted/refactored from the proven loader's parse_header, parse_objects,
 * parse_import_modules, source-object ownership, import de-duplication, and
 * scan_fixups logic. Deliberately excluded: platform VM mapping, target RAM
 * access, object copying, fixup application, veneer emission, scheduling and
 * execution.
 */
#include "os2loader.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OS2L_MAX_OBJECTS      32u
#define OS2L_MAX_PAGES        65535u
#define OS2L_MAX_MODULES      32u
#define OS2L_MAX_IMPORTS      512u
#define OS2L_MAX_NAME_IMPORTS 512u

#define SRC_MASK   0x0fu
#define SRC_SEL16  2u
#define SRC_PTR16  3u
#define SRC_PTR32  6u
#define SRC_OFF32  0x07u
#define SRC_REL32  0x08u
#define SRC_LIST   0x20u

#define TGT_MASK     0x03u
#define TGT_INTERNAL 0x00u
#define TGT_EXT_ORD  0x01u
#define TGT_EXT_NAME 0x02u
#define TGT_INT_ENTRY 0x03u
#define TGT_ADDITIVE 0x04u
#define TGT_CHAIN    0x08u
#define TGT_OFF32    0x10u
#define TGT_ADD32    0x20u
#define TGT_OBJ16    0x40u
#define TGT_ORD8     0x80u

#define MOD_TYPE_MASK 0x00038000u
#define MOD_TYPE_DLL  0x00008000u

typedef struct OWNER {
    int valid;
    uint32_t object;      /* 0-based */
    uint32_t object_page;
} OWNER;

typedef struct CTX {
    const uint8_t *file;
    size_t file_size;
    OS2L_PLAN *p;
    OS2L_ERROR *err;

    uint32_t le;
    int is_lx;
    uint32_t last_page_size;
    uint32_t object_table_off;
    uint32_t object_map_off;
    uint32_t fixpage_off;
    uint32_t fixrec_off;
    uint32_t impmod_off;
    uint32_t impproc_off;
    uint32_t data_pages_off;
    uint32_t num_impmods;

    OWNER *owners; /* physical page count + 1 */
    size_t fixup_cap;
} CTX;

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

static int range_ok(const CTX *c, uint32_t off, uint32_t cb)
{
    size_t o = (size_t)off, n = (size_t)cb;
    return o <= c->file_size && n <= c->file_size - o;
}

static int fail(CTX *c, const char *fmt, ...)
{
    va_list ap;
    if (c->err) {
        va_start(ap, fmt);
        (void)vsnprintf(c->err->message, sizeof(c->err->message), fmt, ap);
        va_end(ap);
    }
    return 0;
}

static uint32_t prot_from_flags(uint32_t flags)
{
    uint32_t p = OS2L_PROT_NONE;
    if (flags & OS2L_OBJ_READ) p |= OS2L_PROT_READ;
    if (flags & OS2L_OBJ_WRITE) p |= OS2L_PROT_WRITE;
    if (flags & OS2L_OBJ_EXEC) p |= OS2L_PROT_EXEC;
    return p;
}

static OS2L_SOURCE_TYPE source_type(uint8_t st)
{
    switch (st) {
    case SRC_SEL16: return OS2L_SRC_SEL16;
    case SRC_PTR16: return OS2L_SRC_PTR16;
    case SRC_PTR32: return OS2L_SRC_PTR32;
    case SRC_OFF32: return OS2L_SRC_OFF32;
    case SRC_REL32: return OS2L_SRC_REL32;
    default: return OS2L_SRC_UNSUPPORTED;
    }
}

static int parse_header(CTX *c)
{
    uint32_t h, nobj, npages;
    OS2L_PLAN *p = c->p;

    if (c->file_size < 0x40u) return fail(c, "file is too small for MZ header");
    if (c->file[0] != 'M' || c->file[1] != 'Z') return fail(c, "input is not MZ");
    c->le = rd32(c->file + 0x3c);
    if (!range_ok(c, c->le, 0xc4u)) return fail(c, "LE header is outside file");
    if (c->file[c->le] != 'L' ||
        (c->file[c->le + 1] != 'E' && c->file[c->le + 1] != 'X'))
        return fail(c, "input is not an LE/LX executable");
    c->is_lx = c->file[c->le + 1] == 'X';
    if (c->file[c->le + 2] != 0 || c->file[c->le + 3] != 0)
        return fail(c, "big-endian LE/LX is unsupported");
    if (rd16(c->file + c->le + 8) != 2)
        return fail(c, "only i386 LE/LX images are supported");
    if (rd16(c->file + c->le + 0x0a) != 1)
        return fail(c, "only OS/2 LE/LX images are supported");

    h = c->le;
    p->format = c->is_lx ? OS2L_FORMAT_LX : OS2L_FORMAT_LE;
    p->file_size = (uint32_t)c->file_size;
    p->header_offset = h;
    p->module_flags = rd32(c->file + h + 0x10);
    npages = rd32(c->file + h + 0x14);
    p->physical_page_count = npages;
    p->entry_object = rd32(c->file + h + 0x18);
    p->entry_offset = rd32(c->file + h + 0x1c);
    p->stack_object = rd32(c->file + h + 0x20);
    p->stack_offset = rd32(c->file + h + 0x24);
    p->page_size = rd32(c->file + h + 0x28);
    c->last_page_size = rd32(c->file + h + 0x2c);
    if (c->is_lx && c->last_page_size > 31u)
        return fail(c, "invalid LX page offset shift");
    c->object_table_off = h + rd32(c->file + h + 0x40);
    nobj = rd32(c->file + h + 0x44);
    c->object_map_off = h + rd32(c->file + h + 0x48);
    c->fixpage_off = h + rd32(c->file + h + 0x68);
    c->fixrec_off = h + rd32(c->file + h + 0x6c);
    c->impmod_off = h + rd32(c->file + h + 0x70);
    c->num_impmods = rd32(c->file + h + 0x74);
    c->impproc_off = h + rd32(c->file + h + 0x78);
    c->data_pages_off = rd32(c->file + h + 0x80);

    if (nobj == 0 || nobj > OS2L_MAX_OBJECTS) return fail(c, "unsupported object count");
    if (npages == 0 || npages > OS2L_MAX_PAGES) return fail(c, "unsupported physical page count");
    if (p->page_size == 0 || (p->page_size & (p->page_size - 1u)) != 0)
        return fail(c, "invalid LE/LX page size");
    if (!range_ok(c, c->object_table_off, nobj * 24u)) return fail(c, "bad LE/LX object table");
    if (!range_ok(c, c->fixpage_off, (npages + 1u) * 4u)) return fail(c, "bad LE/LX fixup page table");

    if ((p->module_flags & MOD_TYPE_MASK) != MOD_TYPE_DLL) {
        if (p->entry_object == 0 || p->entry_object > nobj) return fail(c, "bad entry object");
        if (p->stack_object == 0 || p->stack_object > nobj) return fail(c, "bad stack object");
    } else {
        if (p->entry_object > nobj) return fail(c, "bad DLL entry object");
        if (p->stack_object > nobj) return fail(c, "bad DLL stack object");
    }

    p->object_count = nobj;
    p->objects = (OS2L_OBJECT_PLAN *)calloc(p->object_count, sizeof(*p->objects));
    c->owners = (OWNER *)calloc((size_t)npages + 1u, sizeof(*c->owners));
    if (!p->objects || !c->owners) return fail(c, "out of memory allocating plan tables");
    return 1;
}

static int parse_objects_and_pages(CTX *c)
{
    OS2L_PLAN *p = c->p;
    uint32_t i, j, off, last_map = 0;

    for (i = 0; i < p->object_count; ++i) {
        OS2L_OBJECT_PLAN *o = &p->objects[i];
        off = c->object_table_off + i * 24u;
        o->number = i + 1u;
        o->virtual_size = rd32(c->file + off + 0);
        o->preferred_va = rd32(c->file + off + 4);
        o->flags = rd32(c->file + off + 8);
        o->protection = prot_from_flags(o->flags);
        o->page_map_index = rd32(c->file + off + 12);
        o->page_count = rd32(c->file + off + 16);

        if (!(o->flags & OS2L_OBJ_BIG) && (!(o->flags & OS2L_OBJ_EXEC) || o->virtual_size > 65536u))
            return fail(c, "unsupported non-big object %u", i + 1u);
        if (o->virtual_size && o->preferred_va > 0xffffffffu - o->virtual_size)
            return fail(c, "object %u preferred range overflows", i + 1u);
        if (o->page_count) {
            uint32_t max_pages = o->virtual_size / p->page_size + (o->virtual_size % p->page_size != 0);
            if (o->page_map_index == 0 || o->page_map_index > OS2L_MAX_PAGES ||
                o->page_count > OS2L_MAX_PAGES - (o->page_map_index - 1u) ||
                o->page_count > max_pages)
                return fail(c, "object %u has invalid page-map index", i + 1u);
            if (o->page_map_index - 1u + o->page_count > last_map)
                last_map = o->page_map_index - 1u + o->page_count;
        }
    }

    if ((p->module_flags & MOD_TYPE_MASK) != MOD_TYPE_DLL) {
        OS2L_OBJECT_PLAN *eo = &p->objects[p->entry_object - 1u];
        OS2L_OBJECT_PLAN *so = &p->objects[p->stack_object - 1u];
        if (!(eo->flags & OS2L_OBJ_BIG) || !(so->flags & OS2L_OBJ_BIG))
            return fail(c, "16-bit entry/stack unsupported");
        if (p->entry_offset >= eo->virtual_size || p->stack_offset < 20u || p->stack_offset > so->virtual_size)
            return fail(c, "entry/stack offset lies outside object");
        if (eo->preferred_va > 0xffffffffu - p->entry_offset || so->preferred_va > 0xffffffffu - p->stack_offset)
            return fail(c, "entry/stack VA overflows");
        p->entry_va = eo->preferred_va + p->entry_offset;
        p->stack_top = so->preferred_va + p->stack_offset;
    } else {
        if (p->entry_object) p->entry_va = p->objects[p->entry_object - 1u].preferred_va + p->entry_offset;
        if (p->stack_object) p->stack_top = p->objects[p->stack_object - 1u].preferred_va + p->stack_offset;
    }

    if (last_map == 0 || last_map > OS2L_MAX_PAGES) return fail(c, "unsupported logical page-map size");
    if (c->is_lx && last_map != p->physical_page_count) return fail(c, "LX object map does not cover module pages");
    p->logical_page_count = last_map;
    p->page_count = last_map;
    if (!range_ok(c, c->object_map_off, last_map * (c->is_lx ? 8u : 4u)))
        return fail(c, "bad LE/LX object page map");
    p->pages = (OS2L_PAGE_PLAN *)calloc(p->page_count, sizeof(*p->pages));
    if (!p->pages) return fail(c, "out of memory allocating page plan");

    for (i = 0; i < p->page_count; ++i) {
        const uint8_t *m = c->file + c->object_map_off + i * (c->is_lx ? 8u : 4u);
        OS2L_PAGE_PLAN *pg = &p->pages[i];
        uint32_t phys;
        uint16_t flags;
        pg->logical_page = i + 1u;
        if (c->is_lx) {
            uint64_t data_off;
            phys = i + 1u;
            flags = rd16(m + 6);
            pg->data_size = rd16(m + 4);
            if (pg->data_size > p->page_size) return fail(c, "LX page data exceeds page size");
            if (flags == 0 && pg->data_size) {
                data_off = (uint64_t)c->data_pages_off + ((uint64_t)rd32(m) << c->last_page_size);
                if (data_off > 0xffffffffu || !range_ok(c, (uint32_t)data_off, pg->data_size))
                    return fail(c, "LX page data extends past EOF");
                pg->data_offset = (uint32_t)data_off;
            }
        } else {
            phys = ((uint32_t)m[0] << 16) | ((uint32_t)m[1] << 8) | (uint32_t)m[2];
            flags = m[3];
        }
        pg->physical_page = phys;
        pg->flags = flags;
        if (flags == 0) {
            if (phys == 0 || phys > p->physical_page_count) return fail(c, "bad LE/LX page number");
        } else if (flags != 3) {
            return fail(c, "only enumerated/zero LE/LX pages are supported by extracted proven parser");
        }
    }

    for (i = 0; i < p->object_count; ++i) {
        OS2L_OBJECT_PLAN *o = &p->objects[i];
        for (j = 0; j < o->page_count; ++j) {
            uint32_t idx = o->page_map_index - 1u + j;
            OS2L_PAGE_PLAN *pg = &p->pages[idx];
            uint32_t room, actual, src;
            if (!c->is_lx && pg->flags == 3) {
                pg->owner_object = i + 1u;
                pg->owner_page = j;
                room = (j * p->page_size < o->virtual_size) ? o->virtual_size - j * p->page_size : 0;
                if (room > p->page_size) room = p->page_size;
                pg->zero_fill = room;
                continue;
            }
            if (pg->physical_page == 0 || pg->physical_page > p->physical_page_count)
                return fail(c, "page owner references invalid physical page");
            if (c->owners[pg->physical_page].valid)
                return fail(c, "physical/module page has multiple owners");
            c->owners[pg->physical_page].valid = 1;
            c->owners[pg->physical_page].object = i;
            c->owners[pg->physical_page].object_page = j;
            pg->owner_object = i + 1u;
            pg->owner_page = j;

            if (pg->flags == 3) {
                room = (j * p->page_size < o->virtual_size) ? o->virtual_size - j * p->page_size : 0;
                if (room > p->page_size) room = p->page_size;
                pg->zero_fill = room;
                continue;
            }
            src = c->is_lx ? pg->data_offset : c->data_pages_off + (pg->physical_page - 1u) * p->page_size;
            room = p->page_size;
            if (j * p->page_size >= o->virtual_size) room = 0;
            else if (room > o->virtual_size - j * p->page_size) room = o->virtual_size - j * p->page_size;
            actual = room;
            if (c->is_lx && actual > pg->data_size) actual = pg->data_size;
            if (!c->is_lx && pg->physical_page == p->physical_page_count && actual > c->last_page_size)
                actual = c->last_page_size;
            if (!range_ok(c, src, actual)) return fail(c, "LE/LX page data extends past EOF");
            pg->data_offset = src;
            pg->data_size = actual;
            pg->zero_fill = room - actual;
        }
    }
    return 1;
}

static int parse_import_modules(CTX *c)
{
    OS2L_PLAN *p = c->p;
    uint32_t i, pos;
    if (c->num_impmods > OS2L_MAX_MODULES) return fail(c, "unsupported import-module count");
    p->module_count = c->num_impmods;
    if (!p->module_count) return 1;
    p->modules = (OS2L_IMPORT_MODULE *)calloc(p->module_count, sizeof(*p->modules));
    if (!p->modules) return fail(c, "out of memory allocating import modules");
    pos = c->impmod_off;
    for (i = 0; i < p->module_count; ++i) {
        uint8_t n;
        if (!range_ok(c, pos, 1)) return fail(c, "bad import module table");
        n = c->file[pos++];
        if (n == 0 || n >= OS2L_MODULE_NAME_MAX || !range_ok(c, pos, n))
            return fail(c, "bad import module name");
        memcpy(p->modules[i].name, c->file + pos, n);
        p->modules[i].name[n] = '\0';
        pos += n;
    }
    return 1;
}

static int source_object_offset(CTX *c, uint32_t physical, int16_t source,
                                uint32_t *obj_out, uint32_t *page_out, uint32_t *off_out)
{
    OWNER *own;
    OS2L_OBJECT_PLAN *o;
    int32_t v;
    if (physical == 0 || physical > c->p->physical_page_count)
        return fail(c, "fixup references invalid physical page");
    own = &c->owners[physical];
    if (!own->valid) return fail(c, "fixup physical page has no object owner");
    o = &c->p->objects[own->object];
    v = (int32_t)(own->object_page * c->p->page_size) + (int32_t)source;
    if (v < 0 || (uint32_t)v + 4u > o->virtual_size)
        return fail(c, "fixup source lies outside object");
    *obj_out = own->object + 1u;
    *page_out = own->object_page;
    *off_out = (uint32_t)v;
    return 1;
}

static int skip_objmod(CTX *c, uint32_t *pos, uint8_t flags, uint32_t end, uint32_t *value)
{
    if (flags & TGT_OBJ16) {
        if (*pos > end || end - *pos < 2) return fail(c, "truncated target object/module");
        *value = rd16(c->file + *pos); *pos += 2;
    } else {
        if (*pos >= end) return fail(c, "truncated target object/module");
        *value = c->file[(*pos)++];
    }
    return 1;
}

static int find_or_add_ord(CTX *c, uint32_t module1, uint32_t ordinal, size_t *idx_out)
{
    OS2L_PLAN *p = c->p;
    size_t i;
    if (module1 == 0 || module1 > p->module_count) return fail(c, "external fixup references invalid module");
    for (i = 0; i < p->ordinal_import_count; ++i)
        if (p->ordinal_imports[i].module_index == module1 && p->ordinal_imports[i].ordinal == ordinal) {
            *idx_out = i; return 1;
        }
    if (p->ordinal_import_count >= OS2L_MAX_IMPORTS) return fail(c, "too many imported ordinals");
    {
        size_t n = p->ordinal_import_count + 1u;
        void *q = realloc(p->ordinal_imports, n * sizeof(*p->ordinal_imports));
        if (!q) return fail(c, "out of memory growing ordinal imports");
        p->ordinal_imports = (OS2L_ORDINAL_IMPORT *)q;
        memset(&p->ordinal_imports[n - 1u], 0, sizeof(p->ordinal_imports[n - 1u]));
        p->ordinal_imports[n - 1u].module_index = module1;
        p->ordinal_imports[n - 1u].ordinal = ordinal;
        p->ordinal_import_count = n;
        *idx_out = n - 1u;
    }
    return 1;
}

static int import_name_at(CTX *c, uint32_t off, char *buf, size_t cap)
{
    uint32_t pos, n;
    if (!cap) return 0;
    buf[0] = '\0';
    if (c->impproc_off > 0xffffffffu - off) return fail(c, "import name offset overflows");
    pos = c->impproc_off + off;
    if (!range_ok(c, pos, 1)) return fail(c, "bad import name offset");
    n = c->file[pos++];
    if (n >= cap || !range_ok(c, pos, n)) return fail(c, "bad import name");
    memcpy(buf, c->file + pos, n); buf[n] = '\0';
    return 1;
}

static int find_or_add_name(CTX *c, uint32_t module1, uint32_t name_offset, size_t *idx_out)
{
    OS2L_PLAN *p = c->p;
    size_t i;
    if (module1 == 0 || module1 > p->module_count) return fail(c, "external named fixup references invalid module");
    for (i = 0; i < p->named_import_count; ++i)
        if (p->named_imports[i].module_index == module1 && p->named_imports[i].name_offset == name_offset) {
            *idx_out = i; return 1;
        }
    if (p->named_import_count >= OS2L_MAX_NAME_IMPORTS) return fail(c, "too many named imports");
    {
        size_t n = p->named_import_count + 1u;
        void *q = realloc(p->named_imports, n * sizeof(*p->named_imports));
        if (!q) return fail(c, "out of memory growing named imports");
        p->named_imports = (OS2L_NAMED_IMPORT *)q;
        memset(&p->named_imports[n - 1u], 0, sizeof(p->named_imports[n - 1u]));
        p->named_imports[n - 1u].module_index = module1;
        p->named_imports[n - 1u].name_offset = name_offset;
        if (!import_name_at(c, name_offset, p->named_imports[n - 1u].name, sizeof(p->named_imports[n - 1u].name))) return 0;
        p->named_import_count = n;
        *idx_out = n - 1u;
    }
    return 1;
}

static int append_fixup(CTX *c, const OS2L_FIXUP_SITE *site)
{
    OS2L_PLAN *p = c->p;
    if (p->fixup_count == c->fixup_cap) {
        size_t cap = c->fixup_cap ? c->fixup_cap * 2u : 256u;
        void *q = realloc(p->fixups, cap * sizeof(*p->fixups));
        if (!q) return fail(c, "out of memory growing fixup plan");
        p->fixups = (OS2L_FIXUP_SITE *)q;
        c->fixup_cap = cap;
    }
    p->fixups[p->fixup_count++] = *site;
    return 1;
}

static int scan_fixups(CTX *c)
{
    OS2L_PLAN *pl = c->p;
    uint32_t physical, record_index = 0;

    for (physical = 1; physical <= pl->physical_page_count; ++physical) {
        uint32_t start = rd32(c->file + c->fixpage_off + (physical - 1u) * 4u);
        uint32_t endoff = rd32(c->file + c->fixpage_off + physical * 4u);
        uint32_t pos = c->fixrec_off + start;
        uint32_t end = c->fixrec_off + endoff;
        if (pos > end || end > c->impmod_off) return fail(c, "bad LE/LX fixup-record range");

        while (pos < end) {
            uint8_t type, flags, st, kind;
            uint32_t count, target_pos, first = 0, target = 0, additive = 0, additive_value = 0;
            int16_t nonlist_source = 0;
            int is_list;
            uint32_t i;
            OS2L_SOURCE_TYPE stype;
            OS2L_TARGET_KIND tkind;
            size_t import_idx = 0;

            if (end - pos < 2) return fail(c, "truncated LE/LX fixup record");
            type = c->file[pos++]; flags = c->file[pos++];
            st = type & SRC_MASK; kind = flags & TGT_MASK; is_list = (type & SRC_LIST) != 0;
            stype = source_type(st);
            if (stype == OS2L_SRC_UNSUPPORTED) return fail(c, "unsupported source fixup type 0x%02x", st);
            if ((type & 0x10u) && st != SRC_SEL16 && st != SRC_PTR16) return fail(c, "unsupported alias fixup");
            if (flags & TGT_CHAIN) return fail(c, "chained LE/LX fixups are unsupported");

            if (is_list) {
                if (pos >= end) return fail(c, "truncated source-list count");
                count = c->file[pos++];
            } else {
                uint32_t o, pg, of;
                count = 1;
                if (end - pos < 2) return fail(c, "truncated fixup source");
                nonlist_source = rds16(c->file + pos);
                if (!source_object_offset(c, physical, nonlist_source, &o, &pg, &of)) return 0;
                pos += 2;
            }

            target_pos = pos;
            if (!skip_objmod(c, &target_pos, flags, end, &first)) return 0;
            if (kind == TGT_INTERNAL) {
                tkind = OS2L_TARGET_INTERNAL;
                if (st != SRC_OFF32 && st != SRC_SEL16 && st != SRC_PTR32) return fail(c, "unsupported internal fixup kind");
                if (first == 0 || first > pl->object_count) return fail(c, "internal fixup has invalid object");
                if (st == SRC_SEL16) target = 0;
                else if (flags & TGT_OFF32) {
                    if (end - target_pos < 4) return fail(c, "truncated internal target");
                    target = rd32(c->file + target_pos); target_pos += 4;
                } else {
                    if (end - target_pos < 2) return fail(c, "truncated internal target");
                    target = rd16(c->file + target_pos); target_pos += 2;
                }
                if (flags & TGT_ADDITIVE) return fail(c, "additive internal fixup unsupported");
                pl->internal_fixup_records++;
                pl->internal_fixup_sites += count;
            } else if (kind == TGT_EXT_ORD) {
                tkind = OS2L_TARGET_IMPORT_ORDINAL;
                if (st != SRC_REL32 && st != SRC_PTR16) return fail(c, "unsupported ordinal fixup kind");
                if (st == SRC_PTR16 && (flags & TGT_ADDITIVE)) return fail(c, "additive far16 import unsupported");
                if (first == 0 || first > pl->module_count) return fail(c, "external fixup has invalid module");
                if (flags & TGT_ORD8) {
                    if (target_pos >= end) return fail(c, "truncated ordinal");
                    target = c->file[target_pos++];
                } else if (flags & TGT_OFF32) {
                    if (end - target_pos < 4) return fail(c, "truncated ordinal");
                    target = rd32(c->file + target_pos); target_pos += 4;
                } else {
                    if (end - target_pos < 2) return fail(c, "truncated ordinal");
                    target = rd16(c->file + target_pos); target_pos += 2;
                }
                if (!find_or_add_ord(c, first, target, &import_idx)) return 0;
                pl->ordinal_imports[import_idx].site_count += count;
                pl->external_fixup_sites += count;
            } else if (kind == TGT_EXT_NAME) {
                tkind = OS2L_TARGET_IMPORT_NAME;
                if (st != SRC_REL32) return fail(c, "external named fixup is not REL32");
                if (first == 0 || first > pl->module_count) return fail(c, "external named fixup has invalid module");
                if (flags & TGT_OFF32) {
                    if (end - target_pos < 4) return fail(c, "truncated name offset");
                    target = rd32(c->file + target_pos); target_pos += 4;
                } else {
                    if (end - target_pos < 2) return fail(c, "truncated name offset");
                    target = rd16(c->file + target_pos); target_pos += 2;
                }
                if (!find_or_add_name(c, first, target, &import_idx)) return 0;
                pl->named_imports[import_idx].site_count += count;
                pl->external_fixup_sites += count;
            } else if (kind == TGT_INT_ENTRY) {
                pl->unsupported_fixup_records++;
                return fail(c, "entry-table target fixup unsupported by proven parser");
            } else {
                pl->unsupported_fixup_records++;
                return fail(c, "unknown target fixup kind");
            }

            if (flags & TGT_ADDITIVE) {
                uint32_t n = (flags & TGT_ADD32) ? 4u : 2u;
                if (end - target_pos < n) return fail(c, "truncated additive field");
                additive = 1;
                additive_value = (n == 4u) ? rd32(c->file + target_pos) : rd16(c->file + target_pos);
                target_pos += n;
            }

            pos = target_pos;
            for (i = 0; i < count; ++i) {
                int16_t src;
                OS2L_FIXUP_SITE s;
                uint32_t obj, objpage, of;
                memset(&s, 0, sizeof(s));
                if (is_list) {
                    if (end - pos < 2) return fail(c, "fixup source list extends past record area");
                    src = rds16(c->file + pos); pos += 2;
                } else src = nonlist_source;
                if (!source_object_offset(c, physical, src, &obj, &objpage, &of)) return 0;
                s.record_index = record_index;
                s.physical_page = physical;
                s.source_object = obj;
                s.source_page = objpage;
                s.source_offset = of;
                s.source_type = stype;
                s.target_kind = tkind;
                s.target_object = (tkind == OS2L_TARGET_INTERNAL) ? first : 0;
                s.target_module = (tkind == OS2L_TARGET_IMPORT_ORDINAL || tkind == OS2L_TARGET_IMPORT_NAME) ? first : 0;
                s.target_value = target;
                s.additive_present = additive;
                s.additive_value = additive_value;
                s.source_list = is_list ? 1u : 0u;
                if (!append_fixup(c, &s)) return 0;
            }
            if (pos > end) return fail(c, "fixup record extends past record area");
            record_index++;
        }
        if (pos != end) return fail(c, "fixup page did not end on record boundary");
    }
    return 1;
}

int os2l_plan_image(const uint8_t *file, size_t file_size, OS2L_PLAN *plan, OS2L_ERROR *error)
{
    CTX c;
    if (!file || !plan) return 0;
    memset(plan, 0, sizeof(*plan));
    if (error) error->message[0] = '\0';
    if (file_size > 0xffffffffu) {
        if (error) (void)snprintf(error->message, sizeof(error->message), "input exceeds 32-bit LE/LX file size");
        return 0;
    }
    memset(&c, 0, sizeof(c));
    c.file = file; c.file_size = file_size; c.p = plan; c.err = error;
    if (!parse_header(&c) || !parse_objects_and_pages(&c) || !parse_import_modules(&c) || !scan_fixups(&c)) {
        free(c.owners);
        os2l_free_plan(plan);
        return 0;
    }
    free(c.owners);
    return 1;
}

void os2l_free_plan(OS2L_PLAN *p)
{
    if (!p) return;
    free(p->objects); free(p->pages); free(p->modules); free(p->ordinal_imports);
    free(p->named_imports); free(p->fixups);
    memset(p, 0, sizeof(*p));
}

const char *os2l_format_name(OS2L_FORMAT f)
{
    return f == OS2L_FORMAT_LE ? "LE" : f == OS2L_FORMAT_LX ? "LX" : "UNKNOWN";
}

const char *os2l_source_type_name(OS2L_SOURCE_TYPE t)
{
    switch (t) {
    case OS2L_SRC_SEL16: return "SEL16";
    case OS2L_SRC_PTR16: return "PTR16";
    case OS2L_SRC_PTR32: return "PTR32";
    case OS2L_SRC_OFF32: return "OFF32";
    case OS2L_SRC_REL32: return "REL32";
    default: return "UNSUPPORTED";
    }
}

const char *os2l_target_kind_name(OS2L_TARGET_KIND k)
{
    switch (k) {
    case OS2L_TARGET_INTERNAL: return "internal";
    case OS2L_TARGET_IMPORT_ORDINAL: return "import_ordinal";
    case OS2L_TARGET_IMPORT_NAME: return "import_name";
    case OS2L_TARGET_INTERNAL_ENTRY: return "internal_entry";
    default: return "unsupported";
    }
}
