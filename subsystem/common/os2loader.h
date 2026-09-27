#ifndef OS2LOADER_H
#define OS2LOADER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OS2L_MODULE_NAME_MAX 32u
#define OS2L_PROC_NAME_MAX   256u

/* LE/LX bits preserved from the proven WHP parser. */
#define OS2L_OBJ_READ  0x0001u
#define OS2L_OBJ_WRITE 0x0002u
#define OS2L_OBJ_EXEC  0x0004u
#define OS2L_OBJ_BIG   0x2000u

typedef enum OS2L_FORMAT {
    OS2L_FORMAT_UNKNOWN = 0,
    OS2L_FORMAT_LE,
    OS2L_FORMAT_LX
} OS2L_FORMAT;

typedef enum OS2L_PROTECTION {
    OS2L_PROT_NONE = 0,
    OS2L_PROT_READ = 1u << 0,
    OS2L_PROT_WRITE = 1u << 1,
    OS2L_PROT_EXEC = 1u << 2
} OS2L_PROTECTION;

typedef enum OS2L_SOURCE_TYPE {
    OS2L_SRC_UNSUPPORTED = 0,
    OS2L_SRC_SEL16,
    OS2L_SRC_PTR16,
    OS2L_SRC_PTR32,
    OS2L_SRC_OFF32,
    OS2L_SRC_REL32
} OS2L_SOURCE_TYPE;

typedef enum OS2L_TARGET_KIND {
    OS2L_TARGET_INTERNAL = 0,
    OS2L_TARGET_IMPORT_ORDINAL,
    OS2L_TARGET_IMPORT_NAME,
    OS2L_TARGET_INTERNAL_ENTRY,
    OS2L_TARGET_UNSUPPORTED
} OS2L_TARGET_KIND;

typedef struct OS2L_OBJECT_PLAN {
    uint32_t number;          /* 1-based LE/LX object number */
    uint32_t preferred_va;
    uint32_t virtual_size;
    uint32_t flags;
    uint32_t protection;      /* OS2L_PROT_* intent derived from flags */
    uint32_t page_map_index;  /* 1-based LE/LX page-map index */
    uint32_t page_count;
} OS2L_OBJECT_PLAN;

typedef struct OS2L_PAGE_PLAN {
    uint32_t logical_page;    /* 1-based logical page-map entry */
    uint32_t physical_page;   /* LE physical page or LX module page */
    uint32_t flags;
    uint32_t data_offset;     /* file offset for enumerated data */
    uint32_t data_size;       /* bytes available from file for this page */
    uint32_t owner_object;    /* 1-based, 0 when no owner */
    uint32_t owner_page;      /* 0-based page within object */
    uint32_t zero_fill;       /* bytes in virtual page not backed by file */
} OS2L_PAGE_PLAN;

typedef struct OS2L_IMPORT_MODULE {
    char name[OS2L_MODULE_NAME_MAX];
} OS2L_IMPORT_MODULE;

typedef struct OS2L_ORDINAL_IMPORT {
    uint32_t module_index;    /* 1-based */
    uint32_t ordinal;
    uint32_t site_count;
} OS2L_ORDINAL_IMPORT;

typedef struct OS2L_NAMED_IMPORT {
    uint32_t module_index;    /* 1-based */
    uint32_t name_offset;
    uint32_t site_count;
    char name[OS2L_PROC_NAME_MAX];
} OS2L_NAMED_IMPORT;

typedef struct OS2L_FIXUP_SITE {
    uint32_t record_index;    /* 0-based decoded record number */
    uint32_t physical_page;   /* 1-based fixup page */
    uint32_t source_object;   /* 1-based */
    uint32_t source_page;     /* 0-based within object */
    uint32_t source_offset;   /* byte offset within source object */
    OS2L_SOURCE_TYPE source_type;
    OS2L_TARGET_KIND target_kind;
    uint32_t target_object;   /* internal: 1-based; otherwise 0 */
    uint32_t target_module;   /* import: 1-based; otherwise 0 */
    uint32_t target_value;    /* object offset, ordinal or name offset */
    uint32_t additive_present;
    uint32_t additive_value;
    uint32_t source_list;
} OS2L_FIXUP_SITE;

typedef struct OS2L_PLAN {
    OS2L_FORMAT format;
    uint32_t file_size;
    uint32_t header_offset;
    uint32_t module_flags;
    uint32_t page_size;
    uint32_t physical_page_count;
    uint32_t logical_page_count;

    uint32_t entry_object;
    uint32_t entry_offset;
    uint32_t entry_va;
    uint32_t stack_object;
    uint32_t stack_offset;
    uint32_t stack_top;

    OS2L_OBJECT_PLAN *objects;
    size_t object_count;
    OS2L_PAGE_PLAN *pages;
    size_t page_count;
    OS2L_IMPORT_MODULE *modules;
    size_t module_count;
    OS2L_ORDINAL_IMPORT *ordinal_imports;
    size_t ordinal_import_count;
    OS2L_NAMED_IMPORT *named_imports;
    size_t named_import_count;
    OS2L_FIXUP_SITE *fixups;
    size_t fixup_count;

    uint32_t internal_fixup_records;
    uint32_t internal_fixup_sites;
    uint32_t external_fixup_sites;
    uint32_t unsupported_fixup_records;
} OS2L_PLAN;

typedef struct OS2L_ERROR {
    char message[192];
} OS2L_ERROR;

/* Parse/decode only. No virtual-memory reservation, mapping, fixup writes,
   import veneers, thread manipulation, or execution occur here. */
int os2l_plan_image(const uint8_t *file, size_t file_size,
                    OS2L_PLAN *plan, OS2L_ERROR *error);
void os2l_free_plan(OS2L_PLAN *plan);

const char *os2l_format_name(OS2L_FORMAT format);
const char *os2l_source_type_name(OS2L_SOURCE_TYPE type);
const char *os2l_target_kind_name(OS2L_TARGET_KIND kind);

#ifdef __cplusplus
}
#endif
#endif
