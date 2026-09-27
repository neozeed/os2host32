#include "os2loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ORD_NAME { uint32_t ordinal; const char *name; } ORD_NAME;

/* Names are validation/presentation metadata from the proven shared
   os2_api_catalog.inc.  The common parser itself knows only LE/LX module and
   ordinal/name import identities and has no OS2SS protocol-number dependency. */
static const ORD_NAME g_doscalls_names[] = {
    {223u, "DosQueryPathInfo"},
    {224u, "DosQueryHType"},
    {230u, "DosGetDateTime"},
    {234u, "DosExit"},
    {256u, "DosSetFilePtr"},
    {257u, "DosClose"},
    {259u, "DosDelete"},
    {272u, "DosSetFileSize"},
    {273u, "DosOpen"},
    {281u, "DosRead"},
    {282u, "DosWrite"},
    {299u, "DosAllocMem"},
    {304u, "DosFreeMem"},
    {305u, "DosSetMem"},
    {348u, "DosQuerySysInfo"}
};

static const char *doscalls_name(uint32_t ordinal)
{
    size_t i;
    for (i = 0; i < sizeof(g_doscalls_names)/sizeof(g_doscalls_names[0]); ++i)
        if (g_doscalls_names[i].ordinal == ordinal) return g_doscalls_names[i].name;
    return NULL;
}

static uint8_t *load_file(const char *path, size_t *size_out)
{
    FILE *f = fopen(path, "rb");
    long n;
    uint8_t *p;
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f); return NULL;
    }
    p = (uint8_t *)malloc(n ? (size_t)n : 1u);
    if (!p) { fclose(f); return NULL; }
    if (n && fread(p, 1, (size_t)n, f) != (size_t)n) { free(p); fclose(f); return NULL; }
    fclose(f); *size_out = (size_t)n; return p;
}

static void json_string(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    putchar('"');
    while (*p) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\b': fputs("\\b", stdout); break;
        case '\f': fputs("\\f", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default:
            if (*p < 0x20) printf("\\u%04x", (unsigned)*p);
            else putchar(*p);
        }
        ++p;
    }
    putchar('"');
}

static const char *prot_name(uint32_t p)
{
    if (p == (OS2L_PROT_READ|OS2L_PROT_EXEC)) return "RX";
    if (p == (OS2L_PROT_READ|OS2L_PROT_WRITE)) return "RW";
    if (p == OS2L_PROT_READ) return "R";
    if (p == (OS2L_PROT_READ|OS2L_PROT_WRITE|OS2L_PROT_EXEC)) return "RWX";
    if (p == OS2L_PROT_EXEC) return "X";
    if (p == OS2L_PROT_WRITE) return "W";
    return "NONE";
}

static void print_json(const OS2L_PLAN *p)
{
    size_t i;
    printf("{\n");
    printf("  \"format\": \"%s\",\n", os2l_format_name(p->format));
    printf("  \"file_size\": %u,\n", p->file_size);
    printf("  \"header_offset\": %u,\n", p->header_offset);
    printf("  \"module_flags\": %u,\n", p->module_flags);
    printf("  \"page_size\": %u,\n", p->page_size);
    printf("  \"physical_pages\": %u,\n", p->physical_page_count);
    printf("  \"logical_pages\": %u,\n", p->logical_page_count);
    printf("  \"object_count\": %zu,\n", p->object_count);
    printf("  \"objects\": [\n");
    for (i = 0; i < p->object_count; ++i) {
        const OS2L_OBJECT_PLAN *o = &p->objects[i];
        printf("    {\"number\":%u,\"base\":%u,\"virtual_size\":%u,\"flags\":%u,\"protection\":\"%s\",\"page_map_index\":%u,\"page_count\":%u}%s\n",
               o->number,o->preferred_va,o->virtual_size,o->flags,prot_name(o->protection),o->page_map_index,o->page_count,
               i+1u==p->object_count?"":",");
    }
    printf("  ],\n");
    printf("  \"entry\": {\"object\":%u,\"offset\":%u,\"linear_va\":%u},\n", p->entry_object,p->entry_offset,p->entry_va);
    printf("  \"stack\": {\"object\":%u,\"offset\":%u,\"top\":%u},\n", p->stack_object,p->stack_offset,p->stack_top);

    printf("  \"pages\": [\n");
    for (i = 0; i < p->page_count; ++i) {
        const OS2L_PAGE_PLAN *q=&p->pages[i];
        printf("    {\"logical_page\":%u,\"physical_page\":%u,\"flags\":%u,\"data_offset\":%u,\"data_size\":%u,\"owner_object\":%u,\"owner_page\":%u,\"zero_fill\":%u}%s\n",
               q->logical_page,q->physical_page,q->flags,q->data_offset,q->data_size,q->owner_object,q->owner_page,q->zero_fill,
               i+1u==p->page_count?"":",");
    }
    printf("  ],\n");

    printf("  \"import_modules\": [");
    for (i = 0; i < p->module_count; ++i) {
        if (i) putchar(',');
        json_string(p->modules[i].name);
    }
    printf("],\n");
    printf("  \"ordinal_import_count\": %zu,\n", p->ordinal_import_count);
    printf("  \"named_import_count\": %zu,\n", p->named_import_count);
    printf("  \"ordinal_imports\": [\n");
    for (i = 0; i < p->ordinal_import_count; ++i) {
        const OS2L_ORDINAL_IMPORT *q=&p->ordinal_imports[i];
        const char *mn = (q->module_index && q->module_index <= p->module_count) ? p->modules[q->module_index-1u].name : "";
        const char *nm = (strcmp(mn,"DOSCALLS")==0) ? doscalls_name(q->ordinal) : NULL;
        printf("    {\"module_index\":%u,\"module\":",q->module_index); json_string(mn);
        printf(",\"ordinal\":%u,\"name\":",q->ordinal); if(nm)json_string(nm);else fputs("null",stdout);
        printf(",\"site_count\":%u}%s\n",q->site_count,i+1u==p->ordinal_import_count?"":",");
    }
    printf("  ],\n");
    printf("  \"named_imports\": [\n");
    for (i = 0; i < p->named_import_count; ++i) {
        const OS2L_NAMED_IMPORT *q=&p->named_imports[i];
        const char *mn = (q->module_index && q->module_index <= p->module_count) ? p->modules[q->module_index-1u].name : "";
        printf("    {\"module_index\":%u,\"module\":",q->module_index); json_string(mn);
        printf(",\"name_offset\":%u,\"name\":",q->name_offset);json_string(q->name);
        printf(",\"site_count\":%u}%s\n",q->site_count,i+1u==p->named_import_count?"":",");
    }
    printf("  ],\n");

    printf("  \"fixup_counts\": {\"internal_records\":%u,\"internal_sites\":%u,\"external_sites\":%u,\"planned_sites\":%zu,\"unsupported_records\":%u},\n",
           p->internal_fixup_records,p->internal_fixup_sites,p->external_fixup_sites,p->fixup_count,p->unsupported_fixup_records);
    printf("  \"fixups\": [\n");
    for (i = 0; i < p->fixup_count; ++i) {
        const OS2L_FIXUP_SITE *q=&p->fixups[i];
        printf("    {\"record_index\":%u,\"physical_page\":%u,\"source_object\":%u,\"source_page\":%u,\"source_offset\":%u,\"source_type\":\"%s\",\"target_kind\":\"%s\",\"target_object\":%u,\"target_module\":%u,\"target_value\":%u,\"additive_present\":%u,\"additive_value\":%u,\"source_list\":%u}%s\n",
               q->record_index,q->physical_page,q->source_object,q->source_page,q->source_offset,
               os2l_source_type_name(q->source_type),os2l_target_kind_name(q->target_kind),q->target_object,q->target_module,q->target_value,
               q->additive_present,q->additive_value,q->source_list,i+1u==p->fixup_count?"":",");
    }
    printf("  ]\n}\n");
}

static void print_summary(const OS2L_PLAN *p)
{
    size_t i;
    printf("OS/2 LE/LX host-independent load plan\n");
    printf("format=%s file_size=%u header=0x%08X pages=%u objects=%zu\n",os2l_format_name(p->format),p->file_size,p->header_offset,p->physical_page_count,p->object_count);
    for(i=0;i<p->object_count;++i)printf("object %u base=0x%08X size=0x%08X flags=0x%08X protection=%s pages=%u @%u\n",p->objects[i].number,p->objects[i].preferred_va,p->objects[i].virtual_size,p->objects[i].flags,prot_name(p->objects[i].protection),p->objects[i].page_count,p->objects[i].page_map_index);
    printf("entry object=%u offset=0x%08X linear=0x%08X\n",p->entry_object,p->entry_offset,p->entry_va);
    printf("stack object=%u offset=0x%08X top=0x%08X\n",p->stack_object,p->stack_offset,p->stack_top);
    printf("fixups internal_records=%u internal_sites=%u external_sites=%u planned_sites=%zu\n",p->internal_fixup_records,p->internal_fixup_sites,p->external_fixup_sites,p->fixup_count);
    printf("imports ordinal=%zu named=%zu\n",p->ordinal_import_count,p->named_import_count);
    for(i=0;i<p->ordinal_import_count;++i){const OS2L_ORDINAL_IMPORT *q=&p->ordinal_imports[i];const char *mn=p->modules[q->module_index-1u].name;const char *nm=strcmp(mn,"DOSCALLS")==0?doscalls_name(q->ordinal):NULL;printf("  %s.%u%s%s sites=%u\n",mn,q->ordinal,nm?" ":"",nm?nm:"",q->site_count);}
    printf("note: C/386 initial ESP/startup frame is NOT built by this parser/planner.\n");
}

int main(int argc, char **argv)
{
    int summary = 0;
    const char *path;
    uint8_t *data;
    size_t size;
    OS2L_PLAN plan;
    OS2L_ERROR err;
    if (argc == 3 && strcmp(argv[1],"--summary")==0) { summary=1; path=argv[2]; }
    else if (argc == 2) path=argv[1];
    else { fprintf(stderr,"usage: leplan [--summary] image.exe\n"); return 2; }
    data=load_file(path,&size); if(!data){fprintf(stderr,"leplan: cannot read input\n");return 2;}
    if(!os2l_plan_image(data,size,&plan,&err)){fprintf(stderr,"leplan: %s\n",err.message[0]?err.message:"parse failed");free(data);return 1;}
    if(summary)print_summary(&plan);else print_json(&plan);
    os2l_free_plan(&plan);free(data);return 0;
}
