#ifndef OS2IMAGE_H
#define OS2IMAGE_H

#include "os2loader.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum OS2I_STATUS {
    OS2I_OK = 0,
    OS2I_BAD_ARGUMENT,
    OS2I_RANGE_ERROR,
    OS2I_UNSUPPORTED_FIXUP,
    OS2I_VERIFY_MISMATCH
} OS2I_STATUS;

typedef struct OS2L_RUNTIME_OBJECT {
    uint8_t *bytes;
    uint32_t actual_base;
    uint32_t extent_size;
} OS2L_RUNTIME_OBJECT;

typedef struct OS2L_FIXUP_STATS {
    uint32_t internal_planned;
    uint32_t internal_applied;
    uint32_t internal_verified;
    uint32_t internal_mismatches;
    uint32_t external_planned;
    uint32_t external_resolved;
} OS2L_FIXUP_STATS;


typedef struct OS2L_IMPORT_RESOLUTION {
    uint32_t module_index;
    uint32_t ordinal;
    uint32_t address;
} OS2L_IMPORT_RESOLUTION;

typedef struct OS2L_EXTERNAL_STATS {
    uint32_t planned;
    uint32_t resolved;
    uint32_t verified;
    uint32_t mismatches;
} OS2L_EXTERNAL_STATS;

typedef struct OS2L_EXTERNAL_SNAPSHOT {
    uint32_t fixup_index;
    uint32_t source_object;
    uint32_t source_offset;
    uint32_t original_value;
} OS2L_EXTERNAL_SNAPSHOT;

uint32_t os2l_round_extent(uint32_t virtual_size, uint32_t page_size);
OS2I_STATUS os2l_materialize_objects(const uint8_t *file, size_t file_size,
                                     const OS2L_PLAN *plan,
                                     OS2L_RUNTIME_OBJECT *objects,
                                     size_t object_count);
OS2I_STATUS os2l_verify_materialized_objects(const uint8_t *file, size_t file_size,
                                            const OS2L_PLAN *plan,
                                            const OS2L_RUNTIME_OBJECT *objects,
                                            size_t object_count);
OS2I_STATUS os2l_capture_external_sites(const OS2L_PLAN *plan,
                                        const OS2L_RUNTIME_OBJECT *objects,
                                        size_t object_count,
                                        OS2L_EXTERNAL_SNAPSHOT *snapshots,
                                        size_t snapshot_capacity,
                                        size_t *snapshot_count);
OS2I_STATUS os2l_apply_internal_fixups(const OS2L_PLAN *plan,
                                       OS2L_RUNTIME_OBJECT *objects,
                                       size_t object_count,
                                       OS2L_FIXUP_STATS *stats);
OS2I_STATUS os2l_verify_internal_fixups(const OS2L_PLAN *plan,
                                        const OS2L_RUNTIME_OBJECT *objects,
                                        size_t object_count,
                                        OS2L_FIXUP_STATS *stats);
OS2I_STATUS os2l_verify_external_sites(const OS2L_PLAN *plan,
                                       const OS2L_RUNTIME_OBJECT *objects,
                                       size_t object_count,
                                       const OS2L_EXTERNAL_SNAPSHOT *snapshots,
                                       size_t snapshot_count,
                                       OS2L_FIXUP_STATS *stats);

OS2I_STATUS os2l_apply_external_ordinal_fixups(const OS2L_PLAN *plan,
                                                OS2L_RUNTIME_OBJECT *objects,
                                                size_t object_count,
                                                const OS2L_IMPORT_RESOLUTION *resolutions,
                                                size_t resolution_count,
                                                OS2L_EXTERNAL_STATS *stats);
OS2I_STATUS os2l_verify_external_ordinal_fixups(const OS2L_PLAN *plan,
                                                 const OS2L_RUNTIME_OBJECT *objects,
                                                 size_t object_count,
                                                 const OS2L_IMPORT_RESOLUTION *resolutions,
                                                 size_t resolution_count,
                                                 OS2L_EXTERNAL_STATS *stats);

uint32_t os2l_fnv1a32(const uint8_t *data, size_t size);
const char *os2i_status_name(OS2I_STATUS status);

#ifdef __cplusplus
}
#endif
#endif
