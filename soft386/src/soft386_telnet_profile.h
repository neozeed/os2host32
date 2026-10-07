/* R5C: exact-image adaptation of the native TELNETPM thunk profile.
 * All emitted addresses are guest linear addresses. FS is left emulated.
 * Unknown external authentication callbacks fail instead of entering host code. */
#define TP_STUB_CAP 4096u
#define TP_CALLBACK 0xb47cu
#define TP_DISPATCH 0x1cf14u
struct TpBridge {
    uint8_t *bytes[MAX_OBJECTS];
    uint32_t base[MAX_OBJECTS];
    uint8_t *stub;
    uint32_t stub_base, used;
    uint32_t trace_fn, fs_fn, reject_fn;
    uint32_t imports[MAX_IMPORTS]; /* wrapper addresses */
    uint32_t internal, external, nonflat, fs_sites;
};

static void tp_byte(struct TpBridge *b, uint8_t v)
{
    if (b->used >= TP_STUB_CAP) fatal("TELNETPM bridge buffer exhausted");
    b->stub[b->used++] = v;
}
static void tp_word(struct TpBridge *b, uint32_t v)
{
    tp_byte(b,(uint8_t)v); tp_byte(b,(uint8_t)(v>>8)); tp_byte(b,(uint8_t)(v>>16)); tp_byte(b,(uint8_t)(v>>24));
}
static void tp_seq(struct TpBridge *b, const char *s, uint32_t len)
{
    uint32_t i; for(i=0;i<len;++i) tp_byte(b,(uint8_t)s[i]);
}
static void tp_branch(struct TpBridge *b, uint8_t opcode, uint32_t target)
{
    tp_byte(b,opcode); tp_word(b,target-(b->stub_base+b->used+4));
}
static void tp_patch_jump(uint8_t *code, uint32_t va, uint32_t target, uint32_t length, uint8_t opcode)
{
    memset(code,0x90,length); code[0]=opcode;
    wr32(code+1,target-va-5);
}

static uint32_t tp_dispatch_stub(struct TpBridge *b)
{
    uint32_t entry,j1,j2,j3,stop;
    entry=b->stub_base+b->used;
    tp_seq(b,"\x83\x7c\x24\x04\x01\x0f\x85",7); j1=b->used; tp_word(b,0);
    tp_seq(b,"\x81\x7c\x24\x08",4); tp_word(b,b->base[0]+TP_CALLBACK);
    tp_seq(b,"\x0f\x85",2); j2=b->used; tp_word(b,0);
    tp_seq(b,"\x83\x7c\x24\x0c\x0c\x0f\x85",7); j3=b->used; tp_word(b,0);
    tp_seq(b,"\x55\x8b\xec",3);
    tp_seq(b,"\x0f\xb7\x45\x1e\x50\xff\x75\x1a",8);
    tp_seq(b,"\x0f\xb7\x45\x18\x50\xff\x75\x14",8);
    tp_branch(b,0xe8,b->base[0]+TP_CALLBACK);
    tp_seq(b,"\x83\xc4\x10\x0f\xb7\xc0\xc9\xc3",8);
    stop=b->stub_base+b->used;
    wr32(b->stub+j1,stop-(b->stub_base+j1+4));
    wr32(b->stub+j2,stop-(b->stub_base+j2+4));
    wr32(b->stub+j3,stop-(b->stub_base+j3+4));
    tp_seq(b,"\xfc\x54",2); tp_branch(b,0xe8,b->reject_fn); tp_seq(b,"\x0f\x0b",2);
    return entry;
}


static void install_telnet_profile(struct LeImage *x,struct Runtime *rt)
{
    struct TpBridge b; unsigned i; uint32_t entry;
    static const uint32_t obj[]={0,0,0,0,0,1,1};
    static const uint32_t off[]={0xbc85,0xbcc5,0xb4a4,0xb4b2,0x1cf72,6,0x1d};
    unsigned seen=0;
    if(!tp_match(x)||x->nbridge_fix!=7||x->internal_sites!=7779||x->external_sites!=567)
        fatal("TELNETPM profile inventory mismatch");
    memset(&b,0,sizeof(b));
    for(i=0;i<x->num_objects;i++){b.base[i]=x->objects[i].mapped_addr;b.bytes[i]=rt->ram+b.base[i];}
    for(i=0;i<x->nbridge_fix;i++){
        unsigned j;struct BridgeFix *f=&x->bridge_fix[i];
        for(j=0;j<7;j++)if(f->obj==obj[j]&&f->off==off[j])break;
        if(j==7||(seen&(1u<<j))||f->kind!=TGT_INTERNAL)fatal("TELNETPM unexpected nonflat relocation");
        seen|=1u<<j;f->used=1;
        if(j<2)wr32(b.bytes[0]+f->off-2,b.base[0]+TP_CALLBACK);
    }
    b.stub_base=alloc_guest(rt,TP_STUB_CAP);if(!b.stub_base)fatal("TELNETPM stub allocation failed");
    b.stub=rt->ram+b.stub_base;
    /* Dedicated rejection gate; no host function address enters jar RAM. */
    b.reject_fn=b.stub_base;
    tp_byte(&b,0xb8);tp_word(&b,HC_RUNTIME|3u);tp_byte(&b,0xe7);tp_byte(&b,HOSTCALL_PORT);tp_byte(&b,0xf4);
    entry=tp_dispatch_stub(&b);
    memset(b.bytes[0]+TP_DISPATCH,0xcc,0x1cfaa-TP_DISPATCH);
    tp_patch_jump(b.bytes[0]+TP_DISPATCH,b.base[0]+TP_DISPATCH,entry,5,0xe9);
    memset(b.bytes[0]+TP_CALLBACK,0xcc,0xb4d4-TP_CALLBACK);
    tp_patch_jump(b.bytes[0]+TP_CALLBACK,b.base[0]+TP_CALLBACK,b.base[0]+0xb4d4,5,0xe9);
    memset(b.bytes[0]+0xb5e1,0xcc,0xb5fc-0xb5e1);b.bytes[0][0xb5e1]=0xc3;
    b.bytes[0][0x246f8]=0xc3;b.bytes[0][0x24700]=0xc3;
    memset(b.bytes[1],0xcc,x->objects[1].size);
    if(!rt->quiet)fprintf(stderr,"soft386: TELNETPM-1993 jar profile: 7 nonflat sites; local callback adapted; native FS untouched\n");
}
