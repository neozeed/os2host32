#include "os2image.h"
#include <string.h>

static uint32_t rd32le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void wr32le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

uint32_t os2l_round_extent(uint32_t virtual_size, uint32_t page_size)
{
    if (!page_size || (page_size & (page_size - 1u)) != 0u) return 0;
    if (virtual_size > 0xffffffffu - (page_size - 1u)) return 0;
    return (virtual_size + page_size - 1u) & ~(page_size - 1u);
}

static OS2I_STATUS validate_runtime(const OS2L_PLAN *plan,
                                    const OS2L_RUNTIME_OBJECT *objects,
                                    size_t object_count)
{
    size_t i;
    if (!plan || !objects || object_count != plan->object_count) return OS2I_BAD_ARGUMENT;
    for (i = 0; i < object_count; ++i) {
        uint32_t need = os2l_round_extent(plan->objects[i].virtual_size, plan->page_size);
        if (!need || !objects[i].bytes || objects[i].extent_size < need) return OS2I_RANGE_ERROR;
    }
    return OS2I_OK;
}

OS2I_STATUS os2l_materialize_objects(const uint8_t *file, size_t file_size,
                                     const OS2L_PLAN *plan,
                                     OS2L_RUNTIME_OBJECT *objects,
                                     size_t object_count)
{
    size_t i;
    OS2I_STATUS s = validate_runtime(plan, objects, object_count);
    if (s != OS2I_OK || !file) return s == OS2I_OK ? OS2I_BAD_ARGUMENT : s;

    for (i = 0; i < object_count; ++i) memset(objects[i].bytes, 0, objects[i].extent_size);

    for (i = 0; i < plan->page_count; ++i) {
        const OS2L_PAGE_PLAN *pg = &plan->pages[i];
        const OS2L_OBJECT_PLAN *po;
        OS2L_RUNTIME_OBJECT *ro;
        uint32_t dest_off, room;
        if (!pg->owner_object) continue;
        if (pg->owner_object > object_count) return OS2I_RANGE_ERROR;
        po = &plan->objects[pg->owner_object - 1u];
        ro = &objects[pg->owner_object - 1u];
        if (pg->owner_page > 0xffffffffu / plan->page_size) return OS2I_RANGE_ERROR;
        dest_off = pg->owner_page * plan->page_size;
        if (dest_off >= po->virtual_size) room = 0;
        else {
            room = po->virtual_size - dest_off;
            if (room > plan->page_size) room = plan->page_size;
        }
        if (pg->data_size > room || pg->zero_fill != room - pg->data_size) return OS2I_RANGE_ERROR;
        if ((size_t)pg->data_offset > file_size || pg->data_size > file_size - (size_t)pg->data_offset)
            return OS2I_RANGE_ERROR;
        if (dest_off > ro->extent_size || room > ro->extent_size - dest_off) return OS2I_RANGE_ERROR;
        if (pg->data_size) memcpy(ro->bytes + dest_off, file + pg->data_offset, pg->data_size);
    }
    return OS2I_OK;
}

OS2I_STATUS os2l_verify_materialized_objects(const uint8_t *file, size_t file_size,
                                            const OS2L_PLAN *plan,
                                            const OS2L_RUNTIME_OBJECT *objects,
                                            size_t object_count)
{
    size_t i, j;
    OS2I_STATUS s = validate_runtime(plan, objects, object_count);
    if (s != OS2I_OK || !file) return s == OS2I_OK ? OS2I_BAD_ARGUMENT : s;
    for (i = 0; i < plan->page_count; ++i) {
        const OS2L_PAGE_PLAN *pg = &plan->pages[i];
        const OS2L_OBJECT_PLAN *po;
        const OS2L_RUNTIME_OBJECT *ro;
        uint32_t dest_off, room;
        if (!pg->owner_object) continue;
        po = &plan->objects[pg->owner_object - 1u];
        ro = &objects[pg->owner_object - 1u];
        dest_off = pg->owner_page * plan->page_size;
        room = (dest_off < po->virtual_size) ? po->virtual_size - dest_off : 0;
        if (room > plan->page_size) room = plan->page_size;
        if ((size_t)pg->data_offset > file_size || pg->data_size > file_size - (size_t)pg->data_offset)
            return OS2I_RANGE_ERROR;
        if (pg->data_size) {
            size_t k;
            for (k = 0; k < pg->data_size; ++k)
                if (ro->bytes[dest_off + k] != file[pg->data_offset + k]) return OS2I_VERIFY_MISMATCH;
        }
        for (j = pg->data_size; j < room; ++j)
            if (ro->bytes[dest_off + j] != 0) return OS2I_VERIFY_MISMATCH;
    }
    for (i = 0; i < object_count; ++i) {
        uint32_t vsize = plan->objects[i].virtual_size;
        for (j = vsize; j < objects[i].extent_size; ++j)
            if (objects[i].bytes[j] != 0) return OS2I_VERIFY_MISMATCH;
    }
    return OS2I_OK;
}

OS2I_STATUS os2l_capture_external_sites(const OS2L_PLAN *plan,
                                        const OS2L_RUNTIME_OBJECT *objects,
                                        size_t object_count,
                                        OS2L_EXTERNAL_SNAPSHOT *snapshots,
                                        size_t snapshot_capacity,
                                        size_t *snapshot_count)
{
    size_t i, n = 0;
    OS2I_STATUS s = validate_runtime(plan, objects, object_count);
    if (s != OS2I_OK || !snapshots || !snapshot_count) return s == OS2I_OK ? OS2I_BAD_ARGUMENT : s;
    for (i = 0; i < plan->fixup_count; ++i) {
        const OS2L_FIXUP_SITE *f = &plan->fixups[i];
        const OS2L_RUNTIME_OBJECT *ro;
        if (f->target_kind == OS2L_TARGET_INTERNAL) continue;
        if (f->target_kind != OS2L_TARGET_IMPORT_ORDINAL && f->target_kind != OS2L_TARGET_IMPORT_NAME)
            return OS2I_UNSUPPORTED_FIXUP;
        if (f->source_type != OS2L_SRC_REL32) return OS2I_UNSUPPORTED_FIXUP;
        if (n >= snapshot_capacity || f->source_object == 0 || f->source_object > object_count)
            return OS2I_RANGE_ERROR;
        ro = &objects[f->source_object - 1u];
        if (f->source_offset > ro->extent_size || 4u > ro->extent_size - f->source_offset)
            return OS2I_RANGE_ERROR;
        snapshots[n].fixup_index = (uint32_t)i;
        snapshots[n].source_object = f->source_object;
        snapshots[n].source_offset = f->source_offset;
        snapshots[n].original_value = rd32le(ro->bytes + f->source_offset);
        ++n;
    }
    *snapshot_count = n;
    return OS2I_OK;
}

OS2I_STATUS os2l_apply_internal_fixups(const OS2L_PLAN *plan,
                                       OS2L_RUNTIME_OBJECT *objects,
                                       size_t object_count,
                                       OS2L_FIXUP_STATS *stats)
{
    size_t i;
    OS2I_STATUS s = validate_runtime(plan, objects, object_count);
    if (s != OS2I_OK || !stats) return s == OS2I_OK ? OS2I_BAD_ARGUMENT : s;
    memset(stats, 0, sizeof(*stats));
    stats->internal_planned = plan->internal_fixup_sites;
    stats->external_planned = plan->external_fixup_sites;
    for (i = 0; i < plan->fixup_count; ++i) {
        const OS2L_FIXUP_SITE *f = &plan->fixups[i];
        OS2L_RUNTIME_OBJECT *src;
        const OS2L_RUNTIME_OBJECT *tgt;
        uint32_t value;
        if (f->target_kind != OS2L_TARGET_INTERNAL) continue;
        if (f->source_type != OS2L_SRC_OFF32 || f->additive_present) return OS2I_UNSUPPORTED_FIXUP;
        if (f->source_object == 0 || f->source_object > object_count ||
            f->target_object == 0 || f->target_object > object_count) return OS2I_RANGE_ERROR;
        src = &objects[f->source_object - 1u];
        tgt = &objects[f->target_object - 1u];
        if (f->source_offset > src->extent_size || 4u > src->extent_size - f->source_offset)
            return OS2I_RANGE_ERROR;
        if (tgt->actual_base > 0xffffffffu - f->target_value) return OS2I_RANGE_ERROR;
        value = tgt->actual_base + f->target_value;
        wr32le(src->bytes + f->source_offset, value);
        stats->internal_applied++;
    }
    return stats->internal_applied == plan->internal_fixup_sites ? OS2I_OK : OS2I_VERIFY_MISMATCH;
}

OS2I_STATUS os2l_verify_internal_fixups(const OS2L_PLAN *plan,
                                        const OS2L_RUNTIME_OBJECT *objects,
                                        size_t object_count,
                                        OS2L_FIXUP_STATS *stats)
{
    size_t i;
    OS2I_STATUS s = validate_runtime(plan, objects, object_count);
    if (s != OS2I_OK || !stats) return s == OS2I_OK ? OS2I_BAD_ARGUMENT : s;
    stats->internal_verified = 0;
    stats->internal_mismatches = 0;
    for (i = 0; i < plan->fixup_count; ++i) {
        const OS2L_FIXUP_SITE *f = &plan->fixups[i];
        const OS2L_RUNTIME_OBJECT *src, *tgt;
        uint32_t expected, actual;
        if (f->target_kind != OS2L_TARGET_INTERNAL) continue;
        if (f->source_type != OS2L_SRC_OFF32 || f->additive_present) return OS2I_UNSUPPORTED_FIXUP;
        if (f->source_object == 0 || f->source_object > object_count ||
            f->target_object == 0 || f->target_object > object_count) return OS2I_RANGE_ERROR;
        src = &objects[f->source_object - 1u];
        tgt = &objects[f->target_object - 1u];
        if (f->source_offset > src->extent_size || 4u > src->extent_size - f->source_offset ||
            tgt->actual_base > 0xffffffffu - f->target_value) return OS2I_RANGE_ERROR;
        expected = tgt->actual_base + f->target_value;
        actual = rd32le(src->bytes + f->source_offset);
        if (actual != expected) stats->internal_mismatches++;
        else stats->internal_verified++;
    }
    return stats->internal_mismatches == 0 && stats->internal_verified == plan->internal_fixup_sites
        ? OS2I_OK : OS2I_VERIFY_MISMATCH;
}

OS2I_STATUS os2l_verify_external_sites(const OS2L_PLAN *plan,
                                       const OS2L_RUNTIME_OBJECT *objects,
                                       size_t object_count,
                                       const OS2L_EXTERNAL_SNAPSHOT *snapshots,
                                       size_t snapshot_count,
                                       OS2L_FIXUP_STATS *stats)
{
    size_t i;
    OS2I_STATUS s = validate_runtime(plan, objects, object_count);
    if (s != OS2I_OK || !snapshots || !stats) return s == OS2I_OK ? OS2I_BAD_ARGUMENT : s;
    if (snapshot_count != plan->external_fixup_sites) return OS2I_VERIFY_MISMATCH;
    stats->external_resolved = 0;
    for (i = 0; i < snapshot_count; ++i) {
        const OS2L_EXTERNAL_SNAPSHOT *x = &snapshots[i];
        const OS2L_RUNTIME_OBJECT *ro;
        if (x->source_object == 0 || x->source_object > object_count) return OS2I_RANGE_ERROR;
        ro = &objects[x->source_object - 1u];
        if (x->source_offset > ro->extent_size || 4u > ro->extent_size - x->source_offset)
            return OS2I_RANGE_ERROR;
        if (rd32le(ro->bytes + x->source_offset) != x->original_value)
            return OS2I_VERIFY_MISMATCH;
    }
    return OS2I_OK;
}


static const OS2L_IMPORT_RESOLUTION *find_resolution(const OS2L_IMPORT_RESOLUTION *resolutions,
                                                     size_t resolution_count,
                                                     uint32_t module_index,
                                                     uint32_t ordinal)
{
    size_t i;
    for (i = 0; i < resolution_count; ++i)
        if (resolutions[i].module_index == module_index && resolutions[i].ordinal == ordinal)
            return &resolutions[i];
    return NULL;
}

OS2I_STATUS os2l_apply_external_ordinal_fixups(const OS2L_PLAN *plan,
                                                OS2L_RUNTIME_OBJECT *objects,
                                                size_t object_count,
                                                const OS2L_IMPORT_RESOLUTION *resolutions,
                                                size_t resolution_count,
                                                OS2L_EXTERNAL_STATS *stats)
{
    size_t i;
    OS2I_STATUS s = validate_runtime(plan, objects, object_count);
    if (s != OS2I_OK || !resolutions || !stats) return s == OS2I_OK ? OS2I_BAD_ARGUMENT : s;
    memset(stats, 0, sizeof(*stats));
    stats->planned = plan->external_fixup_sites;
    for (i = 0; i < plan->fixup_count; ++i) {
        const OS2L_FIXUP_SITE *f = &plan->fixups[i];
        OS2L_RUNTIME_OBJECT *src;
        const OS2L_IMPORT_RESOLUTION *r;
        uint32_t site_va, next_va, displacement;
        int64_t delta;
        if (f->target_kind == OS2L_TARGET_INTERNAL) continue;
        if (f->target_kind != OS2L_TARGET_IMPORT_ORDINAL || f->source_type != OS2L_SRC_REL32 || f->additive_present)
            return OS2I_UNSUPPORTED_FIXUP;
        if (f->source_object == 0 || f->source_object > object_count) return OS2I_RANGE_ERROR;
        src = &objects[f->source_object - 1u];
        if (f->source_offset > src->extent_size || 4u > src->extent_size - f->source_offset)
            return OS2I_RANGE_ERROR;
        r = find_resolution(resolutions, resolution_count, f->target_module, f->target_value);
        if (!r || r->address == 0) return OS2I_RANGE_ERROR;
        if (src->actual_base > UINT32_MAX - f->source_offset) return OS2I_RANGE_ERROR;
        site_va = src->actual_base + f->source_offset;
        if (site_va > UINT32_MAX - 4u) return OS2I_RANGE_ERROR;
        next_va = site_va + 4u;
        delta = (int64_t)(uint64_t)r->address - (int64_t)(uint64_t)next_va;
        if (delta < INT32_MIN || delta > INT32_MAX) return OS2I_RANGE_ERROR;
        displacement = (uint32_t)(int32_t)delta;
        wr32le(src->bytes + f->source_offset, displacement);
        stats->resolved++;
    }
    return stats->resolved == stats->planned ? OS2I_OK : OS2I_VERIFY_MISMATCH;
}

OS2I_STATUS os2l_verify_external_ordinal_fixups(const OS2L_PLAN *plan,
                                                 const OS2L_RUNTIME_OBJECT *objects,
                                                 size_t object_count,
                                                 const OS2L_IMPORT_RESOLUTION *resolutions,
                                                 size_t resolution_count,
                                                 OS2L_EXTERNAL_STATS *stats)
{
    size_t i;
    OS2I_STATUS s = validate_runtime(plan, objects, object_count);
    if (s != OS2I_OK || !resolutions || !stats) return s == OS2I_OK ? OS2I_BAD_ARGUMENT : s;
    stats->verified = 0;
    stats->mismatches = 0;
    for (i = 0; i < plan->fixup_count; ++i) {
        const OS2L_FIXUP_SITE *f = &plan->fixups[i];
        const OS2L_RUNTIME_OBJECT *src;
        const OS2L_IMPORT_RESOLUTION *r;
        uint32_t site_va, next_va, actual_target;
        int32_t displacement;
        if (f->target_kind == OS2L_TARGET_INTERNAL) continue;
        if (f->target_kind != OS2L_TARGET_IMPORT_ORDINAL || f->source_type != OS2L_SRC_REL32 || f->additive_present)
            return OS2I_UNSUPPORTED_FIXUP;
        if (f->source_object == 0 || f->source_object > object_count) return OS2I_RANGE_ERROR;
        src = &objects[f->source_object - 1u];
        if (f->source_offset > src->extent_size || 4u > src->extent_size - f->source_offset)
            return OS2I_RANGE_ERROR;
        r = find_resolution(resolutions, resolution_count, f->target_module, f->target_value);
        if (!r || r->address == 0) return OS2I_RANGE_ERROR;
        if (src->actual_base > UINT32_MAX - f->source_offset) return OS2I_RANGE_ERROR;
        site_va = src->actual_base + f->source_offset;
        if (site_va > UINT32_MAX - 4u) return OS2I_RANGE_ERROR;
        next_va = site_va + 4u;
        displacement = (int32_t)rd32le(src->bytes + f->source_offset);
        actual_target = (uint32_t)((uint64_t)next_va + (int64_t)displacement);
        if (actual_target != r->address) stats->mismatches++;
        else stats->verified++;
    }
    return stats->mismatches == 0 && stats->verified == stats->planned ? OS2I_OK : OS2I_VERIFY_MISMATCH;
}

uint32_t os2l_fnv1a32(const uint8_t *data, size_t size)
{
    size_t i;
    uint32_t h = 2166136261u;
    if (!data && size) return 0;
    for (i = 0; i < size; ++i) {
        h ^= data[i];
        h *= 16777619u;
    }
    return h;
}

const char *os2i_status_name(OS2I_STATUS status)
{
    switch (status) {
    case OS2I_OK: return "OK";
    case OS2I_BAD_ARGUMENT: return "BAD_ARGUMENT";
    case OS2I_RANGE_ERROR: return "RANGE_ERROR";
    case OS2I_UNSUPPORTED_FIXUP: return "UNSUPPORTED_FIXUP";
    case OS2I_VERIFY_MISMATCH: return "VERIFY_MISMATCH";
    default: return "UNKNOWN";
    }
}
