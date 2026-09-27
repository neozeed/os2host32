#include "../common/os2loader.h"
#include "../common/os2image.h"
#include "../common/os2veneer.h"
#include "../common/os2startup.h"
#include "../common/os2sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define OBJ1_BASE     0x01000000u
#define OBJ2_BASE     0x01010000u
#define VENEER_BASE   0x01020000u
#define GATEWAY_BASE  0x00402000u
#define STARTUP_BASE  0x01030000u
#define STARTUP_SIZE  0x00020000u
#define MAX_EXTERNAL  64u
#define MAX_IMPORTS   32u

static const uint32_t supported_ordinals[] = {223u,224u,230u,234u,256u,257u,259u,272u,273u,281u,282u,299u,304u,305u,348u};

static int supported_ordinal(uint32_t ordinal)
{
    size_t i;
    for (i=0;i<sizeof(supported_ordinals)/sizeof(supported_ordinals[0]);++i)
        if (supported_ordinals[i]==ordinal) return 1;
    return 0;
}

static int ascii_equal(const char *a, const char *b)
{
    size_t i = 0;
    while (a[i] && b[i]) {
        char ca = a[i], cb = b[i];
        if (ca >= 'a' && ca <= 'z') ca = (char)(ca - 'a' + 'A');
        if (cb >= 'a' && cb <= 'z') cb = (char)(cb - 'a' + 'A');
        if (ca != cb) return 0;
        ++i;
    }
    return a[i] == b[i];
}

static int write_file(const char *path, const void *data, size_t size)
{
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    if (size && fwrite(data, 1, size, f) != size) { fclose(f); return 0; }
    return fclose(f) == 0;
}

static uint8_t *read_file(const char *path, size_t *size_out)
{
    FILE *f = fopen(path, "rb");
    long n; uint8_t *b;
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) || (n = ftell(f)) < 0 || fseek(f, 0, SEEK_SET)) { fclose(f); return NULL; }
    b = (uint8_t *)malloc((size_t)n ? (size_t)n : 1u);
    if (!b) { fclose(f); return NULL; }
    if (n && fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
    fclose(f); *size_out = (size_t)n; return b;
}

static void hex32(const uint8_t *d, char out[65])
{
    static const char h[] = "0123456789abcdef";
    size_t i;
    for (i=0;i<32u;++i) { out[i*2]=h[d[i]>>4]; out[i*2+1]=h[d[i]&15u]; }
    out[64]=0;
}

int main(int argc, char **argv)
{
    uint8_t *file = NULL, *veneer = NULL, *startup = NULL;
    size_t file_size = 0, ext_count = 0, i;
    OS2L_PLAN plan; OS2L_ERROR err;
    OS2L_RUNTIME_OBJECT ro[2];
    OS2L_EXTERNAL_SNAPSHOT ext[MAX_EXTERNAL];
    OS2L_FIXUP_STATS internal;
    OS2L_EXTERNAL_STATS external;
    OS2L_IMPORT_RESOLUTION resolutions[MAX_IMPORTS];
    OS2C386_STARTUP_INFO si;
    OS2I_STATUS is;
    uint32_t entry, stack_top;
    char p1[1024], p2[1024], pv[1024], ps[1024];
    uint8_t d1[32], d2[32], dv[32], ds[32]; char h1[65],h2[65],hv[65],hs[65];
    static const char env[] = "\0"; /* two NULs including implicit terminator */

    if (argc != 3) { fprintf(stderr, "usage: %s hi.exe outdir\n", argv[0]); return 2; }
    file = read_file(argv[1], &file_size);
    if (!file) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    memset(&plan,0,sizeof(plan)); memset(&err,0,sizeof(err)); memset(ro,0,sizeof(ro));
    memset(resolutions,0,sizeof(resolutions)); memset(&si,0,sizeof(si));
    if (!os2l_plan_image(file,file_size,&plan,&err)) { fprintf(stderr,"parse failed: %s\n",err.message); return 1; }
    if (file_size == 0u || plan.format != OS2L_FORMAT_LE || plan.object_count != 2u ||
        plan.named_import_count != 0u || plan.unsupported_fixup_records != 0u ||
        plan.external_fixup_sites > MAX_EXTERNAL || plan.ordinal_import_count == 0u ||
        plan.ordinal_import_count > MAX_IMPORTS) {
        fprintf(stderr,"unsupported execution shape\n"); return 1;
    }
    for(i=0;i<plan.fixup_count;++i) {
        if(plan.fixups[i].target_kind==OS2L_TARGET_INTERNAL) {
            if(plan.fixups[i].source_type!=OS2L_SRC_OFF32) { fprintf(stderr,"unsupported internal fixup\n"); return 1; }
        } else if(plan.fixups[i].target_kind==OS2L_TARGET_IMPORT_ORDINAL) {
            if(plan.fixups[i].source_type!=OS2L_SRC_REL32 || !supported_ordinal(plan.fixups[i].target_value)) {
                fprintf(stderr,"unsupported import ordinal/fixup %u\n",plan.fixups[i].target_value); return 1;
            }
        } else { fprintf(stderr,"unsupported fixup target\n"); return 1; }
    }
    ro[0].actual_base=OBJ1_BASE; ro[0].extent_size=os2l_round_extent(plan.objects[0].virtual_size,plan.page_size);
    ro[1].actual_base=OBJ2_BASE; ro[1].extent_size=os2l_round_extent(plan.objects[1].virtual_size,plan.page_size);
    ro[0].bytes=(uint8_t*)calloc(1,ro[0].extent_size); ro[1].bytes=(uint8_t*)calloc(1,ro[1].extent_size);
    veneer=(uint8_t*)calloc(1,4096u); startup=(uint8_t*)calloc(1,STARTUP_SIZE);
    if (!ro[0].bytes || !ro[1].bytes || !veneer || !startup) { fprintf(stderr,"alloc failed\n"); return 1; }
    is=os2l_materialize_objects(file,file_size,&plan,ro,2u); if(is!=OS2I_OK){fprintf(stderr,"materialize %s\n",os2i_status_name(is));return 1;}
    is=os2l_verify_materialized_objects(file,file_size,&plan,ro,2u); if(is!=OS2I_OK){fprintf(stderr,"materialize verify %s\n",os2i_status_name(is));return 1;}
    is=os2l_capture_external_sites(&plan,ro,2u,ext,MAX_EXTERNAL,&ext_count); if(is!=OS2I_OK||ext_count!=plan.external_fixup_sites){fprintf(stderr,"capture external failed\n");return 1;}
    is=os2l_apply_internal_fixups(&plan,ro,2u,&internal); if(is!=OS2I_OK){fprintf(stderr,"internal apply %s\n",os2i_status_name(is));return 1;}
    is=os2l_verify_internal_fixups(&plan,ro,2u,&internal); if(is!=OS2I_OK){fprintf(stderr,"internal verify %s\n",os2i_status_name(is));return 1;}

    for(i=0;i<plan.ordinal_import_count;++i){
        uint32_t module=plan.ordinal_imports[i].module_index, ord=plan.ordinal_imports[i].ordinal;
        uint32_t va=VENEER_BASE+(uint32_t)i*OS2X86_VENEER_STRIDE;
        if(!supported_ordinal(ord) || module==0 || module>plan.module_count || !ascii_equal(plan.modules[module-1u].name,"DOSCALLS")) { fprintf(stderr,"bad import module/ordinal\n");return 1; }
        if(!os2x86_emit_ordinal_veneer(veneer+i*OS2X86_VENEER_STRIDE,4096u-i*OS2X86_VENEER_STRIDE,va,GATEWAY_BASE,ord) ||
           !os2x86_verify_ordinal_veneer(veneer+i*OS2X86_VENEER_STRIDE,4096u-i*OS2X86_VENEER_STRIDE,va,GATEWAY_BASE,ord)) { fprintf(stderr,"veneer failed %u\n",ord);return 1; }
        resolutions[i].module_index=module; resolutions[i].ordinal=ord; resolutions[i].address=va;
    }
    is=os2l_apply_external_ordinal_fixups(&plan,ro,2u,resolutions,plan.ordinal_import_count,&external); if(is!=OS2I_OK){fprintf(stderr,"external apply %s\n",os2i_status_name(is));return 1;}
    is=os2l_verify_external_ordinal_fixups(&plan,ro,2u,resolutions,plan.ordinal_import_count,&external); if(is!=OS2I_OK){fprintf(stderr,"external verify %s\n",os2i_status_name(is));return 1;}

    entry=ro[plan.entry_object-1u].actual_base+plan.entry_offset;
    stack_top=ro[plan.stack_object-1u].actual_base+plan.stack_offset;
    if(!os2c386_build_startup_area(startup,STARTUP_SIZE,STARTUP_BASE,env,sizeof(env),"C:\\OS2\\HI.EXE","hi.exe",&si) ||
       !os2c386_build_initial_stack(ro[plan.stack_object-1u].bytes,ro[plan.stack_object-1u].extent_size,ro[plan.stack_object-1u].actual_base,stack_top,&si) ||
       !os2c386_verify_initial_stack(ro[plan.stack_object-1u].bytes,ro[plan.stack_object-1u].extent_size,ro[plan.stack_object-1u].actual_base,&si)) {
        fprintf(stderr,"startup frame failed\n"); return 1;
    }
    if(si.initial_esp + 20u != si.stack_top) { fprintf(stderr,"invalid startup stack esp=%08x top=%08x\n",si.initial_esp,si.stack_top); return 1; }

    snprintf(p1,sizeof(p1),"%s/LE4X-HOST-OBJECT1.bin",argv[2]);
    snprintf(p2,sizeof(p2),"%s/LE4X-HOST-OBJECT2.bin",argv[2]);
    snprintf(pv,sizeof(pv),"%s/LE4X-HOST-VENEERS.bin",argv[2]);
    snprintf(ps,sizeof(ps),"%s/LE4X-HOST-STARTUP.bin",argv[2]);
    if(!write_file(p1,ro[0].bytes,ro[0].extent_size)||!write_file(p2,ro[1].bytes,ro[1].extent_size)||
       !write_file(pv,veneer,4096u)||!write_file(ps,startup,STARTUP_SIZE)){fprintf(stderr,"write failed\n");return 1;}
    os2_sha256(ro[0].bytes,ro[0].extent_size,d1); os2_sha256(ro[1].bytes,ro[1].extent_size,d2);
    os2_sha256(veneer,4096u,dv); os2_sha256(startup,STARTUP_SIZE,ds); hex32(d1,h1);hex32(d2,h2);hex32(dv,hv);hex32(ds,hs);

    printf("LE4IO host construction oracle PASS\n");
    printf("object1 preferred=%08x actual=%08x extent=%08x sha256=%s\n",plan.objects[0].preferred_va,ro[0].actual_base,ro[0].extent_size,h1);
    printf("object2 preferred=%08x actual=%08x extent=%08x sha256=%s\n",plan.objects[1].preferred_va,ro[1].actual_base,ro[1].extent_size,h2);
    printf("internal planned=%u applied=%u verified=%u mismatches=%u\n",internal.internal_planned,internal.internal_applied,internal.internal_verified,internal.internal_mismatches);
    printf("external planned=%u resolved=%u verified=%u mismatches=%u\n",external.planned,external.resolved,external.verified,external.mismatches);
    printf("veneer_base=%08x gateway=%08x sha256=%s\n",VENEER_BASE,GATEWAY_BASE,hv);
    for(i=0;i<plan.ordinal_import_count;++i) printf("DOSCALLS.%u=%08x\n",resolutions[i].ordinal,resolutions[i].address);
    printf("startup_base=%08x env=%08x pgm=%08x arg=%08x bytes=%u sha256=%s\n",STARTUP_BASE,si.env_va,si.pgm_va,si.arg_va,si.startup_bytes,hs);
    printf("entry=%08x stack_top=%08x initial_esp=%08x\n",entry,stack_top,si.initial_esp);
    printf("execution_performed=0\n");

    free(ro[0].bytes); free(ro[1].bytes); free(veneer); free(startup); os2l_free_plan(&plan); free(file); return 0;
}
