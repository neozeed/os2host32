/* Phase 1: offline LX relocation model, NOT a native execution bridge.
 * Included by os2host32.c to reuse its checked fixup scanner on every host.
 * Model addresses/selectors are integers, never installed in the CPU.
 * Buffers are ordinary calloc memory and are never called or made executable.
 */
#define INTAKE_LIMIT 0x08000000UL

struct MixedIntake {
    U8 *bytes[MAX_OBJECTS];
    U32 base[MAX_OBJECTS];
    U32 internal_sites, external_sites, nonflat_sites, alias_sites;
    U32 far_edges;
};

static void intake_put16(U8 *p, U32 value)
{
    p[0] = (U8)value;
    p[1] = (U8)(value >> 8);
}

static void intake_put32(U8 *p, U32 value)
{
    intake_put16(p, value);
    intake_put16(p + 2, value >> 16);
}

static U32 intake_selector(U32 object, int alias)
{
    return 0x100UL + object * 16UL + (alias ? 8UL : 0UL);
}

static void intake_hex(const U8 *p, U32 size)
{
    U32 i;
    for (i = 0; i < size; ++i)
        printf("%s%02X", i ? " " : "", (unsigned)p[i]);
    printf("\n");
}

static U32 intake_page_source(struct LxImage *x, U32 section, U32 offset,
                               U32 length)
{
    U32 scaled;
    if (x->page_shift >= 32 || offset > (0xffffffffUL >> x->page_shift))
        fail("mixed intake: page offset shift overflow");
    scaled = offset << x->page_shift;
    if (section > 0xffffffffUL - scaled ||
        !range_ok(x, section + scaled, length))
        fail("mixed intake: page data outside file");
    return section + scaled;
}

static void intake_expand(struct LxImage *x, U8 *dst, U32 size,
                           U32 src, U32 encoded)
{
    U32 in, out, repeats, length, i;
    in = out = 0;
    while (out < size) {
        if (encoded - in < 4)
            fail("mixed intake: truncated iterated record");
        repeats = rd16(x->file + src + in);
        length = rd16(x->file + src + in + 2);
        in += 4;
        if (!repeats || !length || length > encoded - in ||
            repeats > (size - out) / length)
            fail("mixed intake: invalid iterated expansion");
        for (i = 0; i < repeats; ++i) {
            memcpy(dst + out, x->file + src + in, (size_t)length);
            out += length;
        }
        in += length;
    }
    if (in != encoded)
        fail("mixed intake: trailing iterated data");
}

static void intake_map(struct LxImage *x, struct MixedIntake *m)
{
    U32 i, j, k, total, cursor, map, offset, size, flags, src, dst, room;
    struct LxObject *o;
    total = 0;
    cursor = 0x01000000UL;
    if (x->format != FORMAT_LX)
        fail("mixed intake: phase 1 accepts LX images only; use --scan for LE");
    if (x->page_size != 4096 || x->page_shift >= 32 ||
        x->module_pages > MAX_LE_PAGES ||
        !range_ok(x, x->page_map, x->module_pages * 8UL))
        fail("mixed intake: invalid page geometry/map");
    for (i = 0; i < x->object_count; ++i) {
        o = &x->objects[i];
        if (!o->size || o->size > INTAKE_LIMIT - total)
            fail("mixed intake: object allocation exceeds 128 MiB diagnostic limit");
        total += o->size;
        if (o->page_count) {
            if (!o->page_index || o->page_index > x->module_pages ||
                o->page_count > x->module_pages - o->page_index + 1 ||
                o->page_count > (o->size - 1) / x->page_size + 1)
                fail("mixed intake: invalid object page range");
            for (k = 0; k < i; ++k) {
                struct LxObject *other;
                other = &x->objects[k];
                if (other->page_count &&
                    o->page_index < other->page_index + other->page_count &&
                    other->page_index < o->page_index + o->page_count)
                    fail("mixed intake: overlapping logical page ownership");
            }
        }
        cursor = (cursor + 0xffffUL) & ~0xffffUL;
        m->base[i] = cursor;
        cursor += o->size;
        m->bytes[i] = (U8 *)calloc((size_t)o->size, 1);
        if (!m->bytes[i]) fail("mixed intake: out of memory");
        printf("INTAKE OBJECT %lu model=%08lX size=%08lX default=%u "
               "selector=%04lX alias16=%04lX selector-base=%08lX\n",
               (unsigned long)(i + 1), (unsigned long)m->base[i],
               (unsigned long)o->size, (o->flags & OBJ_BIG_DEFAULT) ? 32 : 16,
               (unsigned long)intake_selector(i, 0),
               (unsigned long)intake_selector(i, 1),
               (unsigned long)((o->flags & OBJ_BIG_DEFAULT) ? 0 : m->base[i]));
        for (j = 0; j < o->page_count; ++j) {
            map = x->page_map + (o->page_index - 1 + j) * 8UL;
            offset = rd32(x->file + map);
            size = rd16(x->file + map + 4);
            flags = rd16(x->file + map + 6);
            dst = j * x->page_size;
            room = o->size - dst;
            if (room > x->page_size) room = x->page_size;
            if (flags == LX_PAGE_VALID) {
                if (size > x->page_size)
                    fail("mixed intake: stored page exceeds logical page");
                src = intake_page_source(x, x->data_pages, offset, size);
                if (size > room) size = room;
                memcpy(m->bytes[i] + dst, x->file + src, (size_t)size);
                if (!(o->flags & OBJ_BIG_DEFAULT) || o->size <= 128)
                    printf("INTAKE PAGE object=%lu page=%lu file=%08lX copied=%lu\n",
                           (unsigned long)(i + 1), (unsigned long)j,
                           (unsigned long)src, (unsigned long)size);
            } else if (flags == LX_PAGE_ITERATED) {
                if (!x->iter_pages) fail("mixed intake: missing iterated section");
                src = intake_page_source(x, x->iter_pages, offset, size);
                intake_expand(x, m->bytes[i] + dst, room, src, size);
            } else if (flags != LX_PAGE_ZERO) {
                fail("mixed intake: unsupported invalid/range/compressed page");
            }
        }
        if (!(o->flags & OBJ_BIG_DEFAULT) || o->size <= 128) {
            printf("INTAKE RAW object=%lu first=%lu bytes: ",
                   (unsigned long)(i + 1),
                   (unsigned long)(o->size < 128 ? o->size : 128));
            intake_hex(m->bytes[i], o->size < 128 ? o->size : 128);
        }
    }
}

/* Evidence from fixup-attached instruction encodings, not a whole-program
 * disassembler or reachability proof. Only far JMP encodings at exact
 * selector/pointer operands are recognized. Other selector sites stay raw.
 */
static void intake_edge(struct LxImage *x, struct MixedIntake *m,
                         U32 obj, U32 off, U8 type, U32 target, U32 target_off)
{
    U8 *b;
    U32 st, start, dest, bits;
    int found;
    b = m->bytes[obj];
    st = type & SRC_MASK;
    bits = (x->objects[obj].flags & OBJ_BIG_DEFAULT) ? 32 : 16;
    found = 0;
    start = dest = 0;
    if (st == SRC_SEL16 && bits == 32 && off >= 4 &&
        b[off - 4] == 0x66 && b[off - 3] == 0xea) {
        start = off - 4;
        dest = rd16(b + off - 2);
        found = 1;
    } else if (st == SRC_SEL16 && bits == 16 && off >= 6 &&
               b[off - 6] == 0x66 && b[off - 5] == 0xea) {
        start = off - 6;
        dest = rd32(b + off - 4);
        found = 1;
    } else if (st == SRC_PTR1632 && bits == 16 && off >= 2 &&
               b[off - 2] == 0x66 && b[off - 1] == 0xea) {
        start = off - 2;
        dest = target_off;
        found = 1;
    }
    if (found) {
        if (dest >= x->objects[target - 1].size)
            fail("mixed intake: far jump target outside object");
        ++m->far_edges;
        printf("INTAKE FAR-JMP obj=%lu+%08lX (%lu-bit default) -> "
               "obj=%lu+%08lX (%u-bit %s); static edge, NOT executed\n",
               (unsigned long)(obj + 1), (unsigned long)start,
               (unsigned long)bits, (unsigned long)target,
               (unsigned long)dest,
               (type & SRC_ALIAS) ? 16 :
                   ((x->objects[target - 1].flags & OBJ_BIG_DEFAULT) ? 32 : 16),
               (type & SRC_ALIAS) ? "alias" : "default");
    }
}

static void intake_fixup(struct LxImage *x, U32 obj, U32 off, U8 type,
                          U8 flags, U32 first, U32 target, U32 target_off,
                          void *context)
{
    struct MixedIntake *m;
    U8 *p;
    U32 st, kind, value, selector, base, begin, length;
    int diagnostic;
    m = (struct MixedIntake *)context;
    st = type & SRC_MASK;
    kind = flags & TGT_MASK;
    p = m->bytes[obj] + off;
    if (flags & (TGT_ADDITIVE | TGT_CHAIN))
        fail("mixed intake: additive/chained model not implemented");
    if (kind == TGT_EXT_ORD || kind == TGT_EXT_NAME) {
        if (st != SRC_REL32 || (type & SRC_ALIAS))
            fail("mixed intake: external fixup requires an unsupported non-flat model");
        ++m->external_sites;
        /* No LoadLibrary, fake success exports, or made-up callable pointers. */
        if (x->objects[obj].size <= 128 || !(x->objects[obj].flags & OBJ_BIG_DEFAULT) ||
            (kind == TGT_EXT_ORD && (target == 110 || target == 111 ||
                                   target == 425 || target == 426)))
            printf("INTAKE DEFERRED obj=%lu+%08lX %s.%lu (unresolved, unchanged)\n",
                   (unsigned long)(obj + 1), (unsigned long)off,
                   x->modules[first - 1].name, (unsigned long)target);
        return;
    }
    if (kind != TGT_INTERNAL)
        fail("mixed intake: entry-table fixup model not implemented");
    diagnostic = st != SRC_OFF32 || (type & SRC_ALIAS) ||
                 !(x->objects[first - 1].flags & OBJ_BIG_DEFAULT) ||
                 x->objects[obj].size <= 128;
    if (st != SRC_OFF32 && st != SRC_REL32) ++m->nonflat_sites;
    if (type & SRC_ALIAS) ++m->alias_sites;
    if (diagnostic) {
        printf("INTAKE FIXUP obj=%lu+%08lX %s%s -> obj=%lu+%08lX\n",
               (unsigned long)(obj + 1), (unsigned long)off, source_type_name(st),
               (type & SRC_ALIAS) ? "+ALIAS" : "",
               (unsigned long)first, (unsigned long)target_off);
        begin = off > 8 ? off - 8 : 0;
        length = x->objects[obj].size - begin;
        if (length > 24) length = 24;
        printf("INTAKE CONTEXT obj=%lu+%08lX: ",
               (unsigned long)(obj + 1), (unsigned long)begin);
        intake_hex(m->bytes[obj] + begin, length);
        intake_edge(x, m, obj, off, type, first, target_off);
    }
    base = m->base[first - 1];
    selector = intake_selector(first - 1, (type & SRC_ALIAS) != 0);
    /* Default 32-bit selectors have flat base zero. 16-bit/default or alias
     * selectors are object-relative. OFF32+ALIAS is intentionally unsupported. */
    value = target_off;
    if (!(type & SRC_ALIAS) && (x->objects[first - 1].flags & OBJ_BIG_DEFAULT))
        value = (U32)(base + target_off);
    if (st == SRC_SEL16) {
        intake_put16(p, selector);
        if (rd16(p) != selector) fail("mixed intake: selector verification failed");
    } else if (st == SRC_PTR1632) {
        if (target_off >= x->objects[first - 1].size)
            fail("mixed intake: far pointer target outside object");
        intake_put32(p, value);
        intake_put16(p + 4, selector);
        if (rd32(p) != value || rd16(p + 4) != selector)
            fail("mixed intake: far pointer verification failed");
    } else if ((st == SRC_OFF32 || st == SRC_REL32) && !(type & SRC_ALIAS)) {
        value = (U32)(base + target_off);
        if (st == SRC_REL32)
            value = (U32)(value - (m->base[obj] + off + 4UL));
        intake_put32(p, value);
        if (rd32(p) != value) fail("mixed intake: offset verification failed");
    } else {
        fail("mixed intake: unsupported internal relocation model");
    }
    ++m->internal_sites;
    if (diagnostic) {
        printf("INTAKE PATCH model bytes: ");
        intake_hex(p, source_width(st));
    }
    /* Check that this record did not corrupt a neighbouring byte is left to
     * the independent test oracle; source bounds are checked by the scanner. */
}

static void mixed_intake(struct LxImage *x)
{
    struct MixedIntake m;
    U32 i, j, hash;
    memset(&m, 0, sizeof(m));
    printf("MIXED INTAKE phase 1: offline model; NO guest execution, NO DLL loading.\n"
           "Model bases/selectors are diagnostic tokens, NOT native CPU selectors.\n"
           "Alias selector bases equal object model bases; default32 bases are zero.\n");
    intake_map(x, &m);
    scan_fixups_visit(x, intake_fixup, &m);
    for (i = 0; i < x->object_count; ++i) {
        /* Reproducible FNV-1a fingerprint for an independent relocation oracle. */
        hash = 2166136261U;
        for (j = 0; j < x->objects[i].size; ++j)
            hash = (U32)((hash ^ m.bytes[i][j]) * 16777619U);
        printf("INTAKE MODEL object=%lu fnv1a=%08lX\n",
               (unsigned long)(i + 1), (unsigned long)hash);
        free(m.bytes[i]);
    }
    printf("INTAKE PASS internal-sites=%lu external-deferred=%lu "
           "nonflat-sites=%lu alias-sites=%lu far-jump-edges=%lu\n",
           (unsigned long)m.internal_sites, (unsigned long)m.external_sites,
           (unsigned long)m.nonflat_sites, (unsigned long)m.alias_sites,
           (unsigned long)m.far_edges);
    printf("Static edges do NOT prove runtime reachability. Native --run gate unchanged.\n"
           "Guest was NOT executed; no socket/UI shims installed.\n");
}
