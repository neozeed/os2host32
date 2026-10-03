/* Test-only access to the production emitters. No historical bytes embedded. */
#define main os2host_original_main
#include "../../loader/os2host32.c"
#undef main

static void output32(FILE *f,U32 value)
{
    U8 bytes[4]; intake_put32(bytes,value);
    if(fwrite(bytes,1,4,f)!=4) fail("test snapshot write failed");
}
int main(int argc,char **argv)
{
    struct LxImage x;
    struct MixedIntake m;
    struct TpBridge b;
    U32 i;
    FILE *f;
    if(argc!=3 && !(argc==4 && strcmp(argv[3],"--direct")==0)) return 2;
    memset(&x,0,sizeof(x)); load_file(&x,argv[1]);
    if(strcmp(argv[2],"--etc-env")==0) {
        int changed=tp_canonical_etc((char *)x.file,x.file_size);
        if(changed<0) { free(x.file); return 3; }
        if(fwrite(x.file,1,x.file_size,stdout)!=x.file_size) return 4;
        free(x.file); return 0;
    }
    if(strcmp(argv[2],"--hash")==0) {
        U8 hash[32]; tp_sha256(x.file,x.file_size,hash);
        for(i=0;i<32;++i) printf("%02x",(unsigned)hash[i]);
        printf("\n"); free(x.file); return 0;
    }
    parse_image(&x);
    if(!tp_match(&x)) fail("TELNETPM fingerprint mismatch");
    memset(&m,0,sizeof(m)); memset(&b,0,sizeof(b));
    intake_map(&x,&m); scan_fixups(&x);
    for(i=0;i<x.object_count;++i) { b.bytes[i]=m.bytes[i]; b.base[i]=m.base[i]; }
    b.stub=(U8 *)calloc(TP_STUB_CAP,1); if(!b.stub) fail("test allocation failed");
    b.stub_base=0x02000000; b.trace_fn=0x03000000; b.fs_fn=0x03000010; b.reject_fn=0x03000020;
    for(i=0;i<x.import_count;++i)
        b.imports[i]=tp_bind_import(&b,i,(argc==4 && i==0)?0:0x04000000+i*16,argc==3);
    tp_install(&x,&b);
    f=fopen(argv[2],"wb"); if(!f) fail("cannot create test snapshot");
    output32(f,x.object_count);
    for(i=0;i<x.object_count;++i) {
        output32(f,b.base[i]); output32(f,x.objects[i].size);
        if(fwrite(b.bytes[i],1,x.objects[i].size,f)!=x.objects[i].size) fail("snapshot write failed");
    }
    output32(f,b.stub_base); output32(f,b.used);
    if(fwrite(b.stub,1,b.used,f)!=b.used) fail("snapshot write failed");
    output32(f,x.import_count);
    for(i=0;i<x.import_count;++i) {
        output32(f,b.imports[i]); output32(f,x.imports[i].ordinal);
        if(fwrite(x.modules[x.imports[i].module].name,1,64,f)!=64) fail("snapshot write failed");
    }
    if(fclose(f)) fail("snapshot close failed");
    for(i=0;i<x.object_count;++i) free(m.bytes[i]);
    free(b.stub); free(x.file); return 0;
}
