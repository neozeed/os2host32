/* TELNETPM-1993 exact-image native compatibility profile.
 * No CPU selectors, 16-bit execution, socket emulation or success stubs.
 * Included after mixed_intake.h; all emitters also build on non-Windows hosts.
 */
#define TP_STUB_CAP 32768UL
#define TP_CALLBACK 0x0000b47cUL
#define TP_DISPATCH 0x0001cf14UL

/* The pinned guest CRT compares names case-sensitively. Canonicalize only
 * its ETC input in an owned, bounded startup snapshot; values stay intact.
 * Return -1 for an unterminated block, otherwise the number of renames. */
static int tp_canonical_etc(char *env,U32 size)
{
    U32 pos,len;
    int changed;
    char *end;
    pos=0; changed=0;
    while(pos<size) {
        end=(char *)memchr(env+pos,0,size-pos);
        if(!end) return -1;
        len=(U32)(end-(env+pos));
        if(!len) return changed;
        if(len>=4 && (env[pos]=='e' || env[pos]=='E') &&
           (env[pos+1]=='t' || env[pos+1]=='T') &&
           (env[pos+2]=='c' || env[pos+2]=='C') && env[pos+3]=='=') {
            if(memcmp(env+pos,"ETC",3)) { memcpy(env+pos,"ETC",3); ++changed; }
        }
        pos+=len+1;
    }
    return -1;
}

/* A full-file SHA-256 pins code, fixups, tables and resources together. */
static U32 tp_ror(U32 v, unsigned n) { return (v >> n) | (v << (32 - n)); }
static void tp_sha256(const U8 *data, U32 size, U8 digest[32])
{
    static const U32 k[64] = {
        0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
        0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
        0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
        0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
        0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
        0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
        0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
        0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U
    };
    U32 h[8], w[64], a,b,c,d,e,f,g,j,t1,t2,block,blocks,i,n,pos;
    U8 buf[64];
    static const U32 initial[8] = {0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,
                                  0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U};
    memcpy(h, initial, sizeof(h));
    blocks = size / 64 + ((size % 64 < 56) ? 1 : 2);
    for (block = 0; block < blocks; ++block) {
        memset(buf, 0, sizeof(buf));
        pos = block * 64;
        if (pos < size) {
            n = size - pos; if (n > 64) n = 64;
            memcpy(buf, data + pos, n);
        }
        if (pos <= size && size - pos < 64) buf[size - pos] = 0x80;
        if (block == blocks - 1) {
            buf[59] = (U8)(size >> 29);
            n = size << 3;
            for (i = 0; i < 4; ++i) buf[63-i] = (U8)(n >> (8*i));
        }
        for (i = 0; i < 16; ++i)
            w[i] = ((U32)buf[i*4]<<24) | ((U32)buf[i*4+1]<<16) |
                   ((U32)buf[i*4+2]<<8) | buf[i*4+3];
        for (i = 16; i < 64; ++i)
            w[i] = w[i-16] + (tp_ror(w[i-15],7)^tp_ror(w[i-15],18)^(w[i-15]>>3)) +
                   w[i-7] + (tp_ror(w[i-2],17)^tp_ror(w[i-2],19)^(w[i-2]>>10));
        a=h[0]; b=h[1]; c=h[2]; d=h[3]; e=h[4]; f=h[5]; g=h[6]; j=h[7];
        for (i=0;i<64;++i) {
            t1=j+(tp_ror(e,6)^tp_ror(e,11)^tp_ror(e,25))+((e&f)^(~e&g))+k[i]+w[i];
            t2=(tp_ror(a,2)^tp_ror(a,13)^tp_ror(a,22))+((a&b)^(a&c)^(b&c));
            j=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=j;
    }
    for (i=0;i<32;++i) digest[i]=(U8)(h[i/4]>>(24-8*(i%4)));
}

static int tp_match(struct LxImage *x)
{
    static const U8 expected[32] = {
        0x16,0xf3,0x4d,0x71,0x2c,0xde,0xfb,0x8b,0x3f,0x4f,0xa9,0x3b,0x79,0x56,0xfc,0x97,
        0x42,0x96,0x97,0x41,0x9d,0xec,0xcd,0x80,0xb6,0x2b,0x8d,0x83,0x8d,0x49,0x60,0xe2};
    U8 hash[32];
    if (x->file_size != 273544UL || x->format != FORMAT_LX) return 0;
    tp_sha256(x->file, x->file_size, hash);
    return memcmp(hash, expected, 32) == 0;
}

struct TpBridge {
    U8 *bytes[MAX_OBJECTS];
    U32 base[MAX_OBJECTS];
    U8 *stub;
    U32 stub_base, used;
    U32 trace_fn, fs_fn, reject_fn;
    U32 imports[MAX_IMPORTS]; /* wrapper addresses */
    U32 internal, external, nonflat, fs_sites;
};

static void tp_byte(struct TpBridge *b, U8 v)
{
    if (b->used >= TP_STUB_CAP) fail("TELNETPM bridge buffer exhausted");
    b->stub[b->used++] = v;
}
static void tp_word(struct TpBridge *b, U32 v)
{
    tp_byte(b,(U8)v); tp_byte(b,(U8)(v>>8)); tp_byte(b,(U8)(v>>16)); tp_byte(b,(U8)(v>>24));
}
static void tp_seq(struct TpBridge *b, const char *s, U32 len)
{
    U32 i; for(i=0;i<len;++i) tp_byte(b,(U8)s[i]);
}
static void tp_branch(struct TpBridge *b, U8 opcode, U32 target)
{
    tp_byte(b,opcode); tp_word(b,target-(b->stub_base+b->used+4));
}
static void tp_patch_jump(U8 *code, U32 va, U32 target, U32 length, U8 opcode)
{
    memset(code,0x90,length); code[0]=opcode;
    intake_put32(code+1,target-va-5);
}

/* Entry wrapper: save flags/GPRs including AL's argument-count convention.
 * Host observer receives (import index, original stack). Tail jump leaves
 * the caller's arguments and return address byte-for-byte unchanged.
 */
static U32 tp_import_stub(struct TpBridge *b, U32 index, U32 target)
{
    U32 entry;
    entry=b->stub_base+b->used;
    tp_seq(b,"\x9c\x60\x83\xec\x6c\xdd\x34\x24\xfc\x8d\x84\x24",12);
    tp_word(b,144); tp_seq(b,"\x50\x68",2);
    tp_word(b,index); tp_branch(b,0xe8,b->trace_fn);
    tp_seq(b,"\x83\xc4\x08\xdd\x24\x24\x83\xc4\x6c\x61\x9d",11);
    if (target) tp_branch(b,0xe9,target);
    else tp_seq(b,"\x0f\x0b",2); /* observer must terminate unresolved call */
    return entry;
}

static U32 tp_bind_import(struct TpBridge *b,U32 index,U32 target,int trace)
{
    return (target && !trace)?target:tp_import_stub(b,index,target);
}

/* PUSHAD layout: EDI,ESI,EBP,savedESP,EBX,EDX,ECX,EAX,EFLAGS,return.
 * The C observer operates on that saved context. No real FS writes occur.
 */
static U32 tp_fs_stub(struct TpBridge *b, U32 kind, U32 resume)
{
    U32 entry;
    entry=b->stub_base+b->used;
    tp_seq(b,"\x9c\x60\x83\xec\x6c\xdd\x34\x24\xfc\x8d\x44\x24\x6c\x50\x68",15);
    tp_word(b,kind);
    tp_branch(b,0xe8,b->fs_fn);
    tp_seq(b,"\x83\xc4\x08\xdd\x24\x24\x83\xc4\x6c\x61\x9d",11);
    if(kind!=0) { tp_seq(b,"\x8d\x64\x24",3); tp_byte(b,kind==2?8:4); }
    tp_branch(b,0xe9,resume);
    return entry;
}

/* The one local far16 cdecl callback has packed widths 4,2,4,2.
 * Dispatcher stack: return, mode, target, byte_count, packed arguments.
 * mode=1 + target=local callback + count=12 is the only accepted invocation.
 * All external authentication callbacks stop before a call is attempted.
 */
static U32 tp_dispatch_stub(struct TpBridge *b)
{
    U32 entry,j1,j2,j3,stop;
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
    intake_put32(b->stub+j1,stop-(b->stub_base+j1+4));
    intake_put32(b->stub+j2,stop-(b->stub_base+j2+4));
    intake_put32(b->stub+j3,stop-(b->stub_base+j3+4));
    tp_seq(b,"\xfc\x54",2); tp_branch(b,0xe8,b->reject_fn); tp_seq(b,"\x0f\x0b",2);
    return entry;
}

static void tp_fixup(struct LxImage *x,U32 obj,U32 off,U8 type,U8 flags,
                     U32 first,U32 target,U32 value,void *ctx)
{
    struct TpBridge *b;
    U32 st,kind,addr,idx;
    U8 *p;
    b=(struct TpBridge *)ctx; st=type&SRC_MASK; kind=flags&TGT_MASK;
    p=b->bytes[obj]+off;
    if(flags&(TGT_ADDITIVE|TGT_CHAIN)) fail("TELNETPM unsupported additive/chain fixup");
    if(kind==TGT_INTERNAL) {
        ++b->internal;
        if(st==SRC_OFF32 && !(type&SRC_ALIAS))
            intake_put32(p,b->base[first-1]+value);
        else if(st==SRC_REL32 && !(type&SRC_ALIAS))
            intake_put32(p,b->base[first-1]+value-b->base[obj]-off-4);
        else {
            ++b->nonflat;
            /* Every remaining site is in an unreachable replaced shim,
             * except the two PUSH immediates which become flat callback tokens. */
            if(obj==0 && (off==0xbc85 || off==0xbcc5) &&
               (type & ~SRC_LIST)==(SRC_SEL16|SRC_ALIAS) && first==1)
                intake_put32(p-2,b->base[0]+TP_CALLBACK);
            else if(!((obj==0 && (off==0xb4a4 || off==0xb4b2 || off==0x1cf72) && st==SRC_SEL16) ||
                      (obj==1 && off==6 && st==SRC_SEL16) ||
                      (obj==1 && off==0x1d && st==SRC_PTR1632)))
                fail("TELNETPM unaccounted nonflat fixup");
        }
    } else if(kind==TGT_EXT_ORD && st==SRC_REL32 && !(type&SRC_ALIAS)) {
        for(idx=0;idx<x->import_count;++idx)
            if(x->imports[idx].module==first-1 && x->imports[idx].ordinal==target) break;
        if(idx==x->import_count) fail("TELNETPM missing import index");
        addr=b->imports[idx];
        if(!addr) fail("TELNETPM external fixup lacks wrapper");
        intake_put32(p,addr-b->base[obj]-off-4); ++b->external;
    } else fail("TELNETPM unaccounted fixup");
}

/* Fixed instruction sites, not a pattern-based patch of arbitrary programs. */
static const U32 tp_fs_push[] = {0x1a6c0,0x1a7c8,0x1a9e0,0x1b6b4,0x1b968,0x1d5c5,
    0x1d824,0x1d944,0x1db54,0x1dc4c,0x1dd44,0x1de80,0x1f394,0x1f498,0x1f8d4,
    0x1fcbc,0x21b88,0x22348,0x22a30,0x23324,0x249ec};
static const U32 tp_fs_esp[] = {0x1a6c7,0x1a7cf,0x1a9e7,0x1b6bb,0x1b96f,0x1d82b,
    0x1d94b,0x1db5b,0x1dc53,0x1dd4b,0x1de87,0x1f39b,0x1f49f,0x1f8db,0x1fcc3,
    0x21b8f,0x2234f,0x22a37,0x2332b,0x249f3};
static const U32 tp_fs_pop[] = {0x1a6ec,0x1a7f8,0x1aa09,0x1b760,0x1b83e,0x1b9aa,
    0x1ba0b,0x1d636,0x1d925,0x1d9dd,0x1db7c,0x1dbb9,0x1dc2c,0x1dc93,0x1dd13,
    0x1dd96,0x1de62,0x1deb4,0x1dedf,0x1def4,0x1df28,0x1e017,0x1e227,0x1e451,
    0x1e4fe,0x1f411,0x1f46a,0x1f4f3,0x1f911,0x1f92c,0x1f95e,0x1f9c4,0x1fae8,
    0x1fcdf,0x1fd28,0x1fdd8,0x1fe07,0x21bc3,0x21bfd,0x22385,0x223bd,0x22a59,
    0x22a7e,0x22ad2,0x22b66,0x22b91,0x22bda,0x22c05,0x22c45,0x22ca2,0x22d55,
    0x23353,0x23382,0x2345b,0x2346c,0x24a5f,0x24aef,0x24b0a,0x24b17};

static void tp_patch_fs(struct TpBridge *b,U32 off,U32 kind)
{
    static const char *const sig[] = {"\x64\xff\x35\0\0\0\0","\x64\x89\x25\0\0\0\0",
        "\x64\x8f\x05\0\0\0\0","\x64\xa3\0\0\0\0"};
    U32 len,entry;
    len=kind==3?6:7;
    if(memcmp(b->bytes[0]+off,sig[kind],len)) fail("TELNETPM FS instruction changed");
    entry=tp_fs_stub(b,kind,b->base[0]+off+len);
    tp_patch_jump(b->bytes[0]+off,b->base[0]+off,entry,len,0xe8);
    ++b->fs_sites;
}

static void tp_install(struct LxImage *x,struct TpBridge *b)
{
    U32 i,entry;
    if(!tp_match(x)) fail("TELNETPM fingerprint mismatch; bridge NOT installed");
    scan_fixups_visit(x,tp_fixup,b);
    if(b->internal!=7779 || b->external!=567 || b->nonflat!=7)
        fail("TELNETPM relocation inventory changed");
    entry=tp_dispatch_stub(b);
    memset(b->bytes[0]+TP_DISPATCH,0xcc,0x1cfaa-TP_DISPATCH);
    tp_patch_jump(b->bytes[0]+TP_DISPATCH,b->base[0]+TP_DISPATCH,entry,5,0xe9);
    /* Bypass both segment/stack marshalling prologue and far-return epilogue.
     * The original 32-bit callback body (including TELNET escaping) is kept. */
    memset(b->bytes[0]+TP_CALLBACK,0xcc,0xb4d4-TP_CALLBACK);
    tp_patch_jump(b->bytes[0]+TP_CALLBACK,b->base[0]+TP_CALLBACK,b->base[0]+0xb4d4,5,0xe9);
    memset(b->bytes[0]+0xb5e1,0xcc,0xb5fc-0xb5e1);
    b->bytes[0][0xb5e1]=0xc3;
    /* The EAX selector conversion helpers now carry flat tokens. */
    b->bytes[0][0x246f8]=0xc3;
    b->bytes[0][0x24700]=0xc3;
    for(i=0;i<sizeof(tp_fs_push)/sizeof(tp_fs_push[0]);++i) tp_patch_fs(b,tp_fs_push[i],0);
    for(i=0;i<sizeof(tp_fs_esp)/sizeof(tp_fs_esp[0]);++i) tp_patch_fs(b,tp_fs_esp[i],1);
    for(i=0;i<sizeof(tp_fs_pop)/sizeof(tp_fs_pop[0]);++i) tp_patch_fs(b,tp_fs_pop[i],2);
    tp_patch_fs(b,0x1d5d9,3);
    if(b->fs_sites!=101) fail("TELNETPM FS inventory changed");
    /* The tiny 16-bit executable object can never be entered by this bridge. */
    memset(b->bytes[1],0xcc,x->objects[1].size);
    if(!g_quiet) printf("TELNETPM BRIDGE: internal=%lu external=%lu nonflat=%lu FS-shadow=%lu stubs=%lu bytes\n",
        (unsigned long)b->internal,(unsigned long)b->external,(unsigned long)b->nonflat,
        (unsigned long)b->fs_sites,(unsigned long)b->used);
}

static void tp_check(struct LxImage *x)
{
    struct MixedIntake m;
    struct TpBridge b;
    U32 i;
    if(!tp_match(x)) fail("TELNETPM fingerprint mismatch; guest was NOT executed");
    memset(&m,0,sizeof(m)); memset(&b,0,sizeof(b));
    intake_map(x,&m); scan_fixups(x);
    for(i=0;i<x->object_count;++i) { b.bytes[i]=m.bytes[i]; b.base[i]=m.base[i]; }
    b.stub=(U8 *)calloc(TP_STUB_CAP,1);
    if(!b.stub) fail("out of memory for TELNETPM model");
    b.stub_base=0x02000000; b.trace_fn=0x03000000; b.fs_fn=0x03000010; b.reject_fn=0x03000020;
    for(i=0;i<x->import_count;++i) b.imports[i]=tp_bind_import(&b,i,0x04000000+i*16,1);
    tp_install(x,&b);
    for(i=0;i<x->object_count;++i) free(m.bytes[i]);
    free(b.stub);
    printf("TELNETPM PHASE2 CHECK PASS: generated native bridge; NO guest execution or DLL loading.\n");
}

#ifdef _WIN32
struct TpThread {
    U32 head;
    U32 tib[6];
    U32 tib2[4];
    U32 pib[7];
};
static DWORD tp_tls = TLS_OUT_OF_INDEXES;
static CRITICAL_SECTION tp_info_lock;
static struct LxImage *tp_image;
static U32 tp_native[MAX_IMPORTS];
static U32 (__cdecl *tp_getinfo)(void **,void **);
static U32 tp_startup_env;
static int tp_trace_enabled;

/* Diagnostics never fabricate ETC.
 * Read through Win32 so a bad guest pointer is reported, not dereferenced.
 * Only ETC is printed; other environment names/values remain private. */
static int tp_peek(U32 address,void *out,U32 bytes)
{
    SIZE_T got;
    return address && ReadProcessMemory(GetCurrentProcess(),
        (void *)(ULONG_PTR)address,out,bytes,&got) && got==bytes;
}
static int tp_etc_entry(U32 address)
{
    char key[4],c;
    U32 i;
    if(!tp_peek(address,key,4)) return 0;
    if((key[0]!='E' && key[0]!='e') || (key[1]!='T' && key[1]!='t') ||
       (key[2]!='C' && key[2]!='c') || key[3]!='=') return 0;
    fprintf(stderr," %c%c%c=\"",key[0],key[1],key[2]);
    for(i=4;i<164;++i) {
        if(!tp_peek(address+i,&c,1)) { fprintf(stderr,"<unreadable>"); break; }
        if(!c) break;
        if((unsigned char)c>=32 && (unsigned char)c<127) fputc(c,stderr);
        else fprintf(stderr,"\\x%02X",(unsigned)(unsigned char)c);
    }
    if(i==164) fprintf(stderr,"<truncated>");
    fprintf(stderr,"\" (%s)",memcmp(key,"ETC=",4)?"case differs":"exact case");
    return 1;
}
static void tp_env_report(const char *label,U32 address)
{
    U32 n,count,found;
    char c;
    int start;
    fprintf(stderr,"TP ENV %s block=%08lX",label,(unsigned long)address);
    count=found=0; start=1;
    for(n=0;n<65536;++n) {
        if(!tp_peek(address+n,&c,1)) { fprintf(stderr," unreadable at +%lu",(unsigned long)n); break; }
        if(start) {
            if(!c) break;
            ++count; found+=(U32)tp_etc_entry(address+n);
        }
        start=(c==0);
    }
    fprintf(stderr," entries=%lu ETC-matches=%lu%s\n",(unsigned long)count,
        (unsigned long)found,n==65536?" scan-limit":"");
}
static void tp_crt_env_report(void)
{
    U32 crt,fields[6],pib,env,entry,i,found;
    U32 data=(U32)(ULONG_PTR)tp_image->objects[4].mapped;
    fprintf(stderr,"TP ENV guest CRT before config:");
    if(!tp_peek(data+0x9c80,&crt,4) || !tp_peek(crt+0x30,fields,sizeof(fields))) {
        fprintf(stderr," unreadable CRT\n"); return;
    }
    /* Verified from this fingerprint's original getenv/environment builder:
     * CRT+30 lock, +3C count, +44 vector, +1E4 PIB pointer. */
    fprintf(stderr," CRT=%08lX lock=%08lX count=%lu vector=%08lX",
        (unsigned long)crt,(unsigned long)fields[0],
        (unsigned long)fields[3],(unsigned long)fields[5]);
    found=0;
    if(fields[3]<=4096) {
        for(i=0;i<fields[3];++i) {
            if(!tp_peek(fields[5]+i*4,&entry,4)) { fprintf(stderr," unreadable vector"); break; }
            found+=(U32)tp_etc_entry(entry);
        }
    } else fprintf(stderr," count exceeds diagnostic limit");
    fprintf(stderr," ETC-matches=%lu\n",(unsigned long)found);
    if(tp_peek(crt+0x1e4,&pib,4) && tp_peek(pib+0x10,&env,4))
        tp_env_report("guest PIB at config",env);
}

static struct TpThread *tp_thread(void)
{
    struct TpThread *t;
    t=(struct TpThread *)TlsGetValue(tp_tls);
    if(!t) {
        t=(struct TpThread *)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*t));
        if(!t) fail("TELNETPM cannot allocate thread state");
        t->head=0xffffffffUL; t->tib[0]=t->head;
        if(!TlsSetValue(tp_tls,t)) fail("TELNETPM cannot set thread state");
    }
    return t;
}

static void __cdecl tp_fs_observe(U32 kind,U32 *saved)
{
    struct TpThread *t;
    t=tp_thread();
    if(kind==0) saved[9]=t->head;       /* replace bridge return slot with PUSH value */
    else if(kind==1) t->head=(U32)(ULONG_PTR)(saved+10); /* guest ESP before CALL */
    else if(kind==2) t->head=saved[10]; /* original POP source */
    else if(kind==3) t->head=saved[7];  /* original EAX */
    else fail("TELNETPM invalid FS operation");
    t->tib[0]=t->head;
}

/* Keep API-based and inline exception registrations in the same thread-local
 * chain. This tracks registrations only; OS/2 exception dispatch is absent.
 * In particular, a guest record is NEVER linked into native Win32 SEH.
 */
static U32 __cdecl tp_set_exception(U32 *rec)
{
    struct TpThread *t;
    if(!rec) return O2_ERROR_INVALID_PARAMETER;
    t=tp_thread(); rec[0]=t->head;
    t->head=(U32)(ULONG_PTR)rec; t->tib[0]=t->head;
    return 0;
}
static U32 __cdecl tp_unset_exception(U32 *rec)
{
    struct TpThread *t;
    U32 *link,guard;
    if(!rec) return O2_ERROR_INVALID_PARAMETER;
    t=tp_thread(); link=&t->head; guard=0;
    while(*link && *link!=0xffffffffUL && guard++<256) {
        if(*link==(U32)(ULONG_PTR)rec) {
            *link=rec[0]; t->tib[0]=t->head; return 0;
        }
        link=(U32 *)(ULONG_PTR)*link;
    }
    return O2_ERROR_INVALID_PARAMETER;
}
static U32 __cdecl tp_get_info(void **ptib,void **ppib)
{
    struct TpThread *t;
    void *source,*pib;
    U32 rc;
    if(!ptib || !ppib) return O2_ERROR_INVALID_PARAMETER;
    if(!tp_getinfo) fail("TELNETPM DOSCALLS.312 required for TIB presentation");
    t=tp_thread(); source=pib=0;
    /* The existing backend presents shared scratch TIB structures. Copy
     * under a lock, keeping this probe's returned TIB and chain per-thread. */
    EnterCriticalSection(&tp_info_lock);
    rc=tp_getinfo(&source,&pib);
    if(!rc) {
        memcpy(t->tib,source,sizeof(t->tib));
        memcpy(t->tib2,(void *)(ULONG_PTR)t->tib[3],sizeof(t->tib2));
        memcpy(t->pib,pib,sizeof(t->pib));
        if(tp_trace_enabled) {
            U32 fields[7];
            if(tp_peek((U32)(ULONG_PTR)pib,fields,sizeof(fields))) {
                fprintf(stderr,"TP ENV DOSCALLS.312 PIB=%08lX cmd=%08lX env=%08lX\n",
                    (unsigned long)(ULONG_PTR)pib,(unsigned long)fields[3],(unsigned long)fields[4]);
                tp_env_report("backend PIB",fields[4]);
            } else fprintf(stderr,"TP ENV DOSCALLS.312 unreadable PIB\n");
        }
    }
    LeaveCriticalSection(&tp_info_lock);
    if(rc) return rc;
    if(!tp_startup_env) fail("TELNETPM startup environment was not initialized");
    t->pib[4]=tp_startup_env;
    if(tp_trace_enabled) tp_env_report("guest canonical PIB",t->pib[4]);
    t->tib[0]=t->head; t->tib[3]=(U32)(ULONG_PTR)t->tib2;
    *ptib=t->tib; *ppib=t->pib;
    return 0;
}

static void tp_location(U32 pc)
{
    U32 i,base;
    for(i=0;i<tp_image->object_count;++i) {
        base=(U32)(ULONG_PTR)tp_image->objects[i].mapped;
        if(pc>=base && pc-base<tp_image->objects[i].size) {
            fprintf(stderr,"object %lu+%08lX",(unsigned long)(i+1),(unsigned long)(pc-base));
            return;
        }
    }
    fprintf(stderr,"native %08lX",(unsigned long)pc);
}
static void __cdecl tp_import_observe(U32 index,U32 *stack)
{
    struct ImportOrd *im;
    if(index>=tp_image->import_count) fail("TELNETPM invalid import trace index");
    if(tp_native[index] && !tp_trace_enabled) return;
    im=&tp_image->imports[index];
    fprintf(stderr,"TP CALL %s.%lu from ",tp_image->modules[im->module].name,(unsigned long)im->ordinal);
    tp_location(stack[0]); fprintf(stderr," ESP=%08lX%s\n",(unsigned long)(ULONG_PTR)stack,
        tp_native[index]?"":" UNIMPLEMENTED");
    if(stack[0]==(U32)(ULONG_PTR)tp_image->objects[0].mapped+0x3b71 &&
       im->ordinal==331 && ascii_ieq(tp_image->modules[im->module].name,"DOSCALLS"))
        tp_crt_env_report();
    fflush(stderr);
    if(!tp_native[index]) {
        fprintf(stderr,"TELNETPM STOP: reached unavailable import; no result was fabricated.\n");
        fflush(stderr); ExitProcess(4);
    }
}
static void __cdecl tp_thunk_reject(U32 *stack)
{
    fprintf(stderr,"TELNETPM STOP: unsupported authentication thunk mode=%lu target=%08lX frame=%lu from ",
        (unsigned long)stack[1],(unsigned long)stack[2],(unsigned long)stack[3]);
    tp_location(stack[0]); fprintf(stderr,"\nNo 16-bit target was executed.\n");
    fflush(stderr); ExitProcess(5);
}
static void __cdecl tp_returned(U32 rc)
{
    if(tp_trace_enabled) fprintf(stderr,"TELNETPM: process entry returned %lu\n",(unsigned long)rc);
    fflush(stderr); ExitProcess(rc);
}

static U32 tp_resolve(struct LxImage *x,U32 index)
{
    U32 m,ord,addr;
    char dll[80];
    FARPROC proc;
    m=x->imports[index].module; ord=x->imports[index].ordinal;
    /* Bind only native PE personality DLLs. SO32DLL/TCP32DLL now provide
     * explicit IBM-ABI-to-Winsock adapters; missing imports still trap. */
    if(x->modules[m].kind==IMPORT_KIND_NONE) {
        sprintf(dll,"%s.dll",x->modules[m].name);
        x->modules[m].handle=LoadLibraryA(dll);
        x->modules[m].kind=x->modules[m].handle?IMPORT_KIND_HOST:IMPORT_KIND_TRAP;
        if(tp_trace_enabled) printf("TP MODULE %s: %s\n",dll,x->modules[m].handle?"native DLL loaded":"unavailable; calls will stop");
    }
    proc=x->modules[m].handle?GetProcAddress(x->modules[m].handle,(LPCSTR)(ULONG_PTR)ord):0;
    addr=proc?(U32)(ULONG_PTR)proc:0;
    if(ascii_ieq(x->modules[m].name,"DOSCALLS")) {
        if(ord==354) addr=(U32)(ULONG_PTR)tp_set_exception;
        if(ord==355) addr=(U32)(ULONG_PTR)tp_unset_exception;
        if(ord==312 && proc) {
            tp_getinfo=(U32 (__cdecl *)(void **,void **))proc;
            addr=(U32)(ULONG_PTR)tp_get_info;
        }
    }
    return addr;
}

static void tp_enter(struct LxImage *x,const char *name,const char *arg0_override,
                     int argc,char **argv,int first_arg)
{
    struct TpBridge b;
    U32 env,cmd,pgm,ret,entry,stack;
    void (__cdecl *start)(void);
    memset(&b,0,sizeof(b));
    (void)build_os2_startup_area(name,arg0_override,argc,argv,first_arg,&env,&cmd,&pgm);
    if(tp_trace_enabled) {
        char value[256];
        DWORD len;
        SetLastError(0);
        len=GetEnvironmentVariableA("ETC",value,sizeof(value));
        fprintf(stderr,"TP ENV Win32 ETC length=%lu error=%lu\n",
            (unsigned long)len,(unsigned long)GetLastError());
    }
    if(tp_trace_enabled) tp_env_report("entry snapshot",env);
    {
        int changed;
        /* pgm immediately follows the owned environment in this allocation.
         * Bound the walk by that layout, not a scan past the last terminator. */
        changed=tp_canonical_etc((char *)(ULONG_PTR)env,pgm-env);
        if(changed<0) fail("TELNETPM startup environment is unterminated");
        tp_startup_env=env;
        if(tp_trace_enabled) {
            fprintf(stderr,"TP ENV canonicalized ETC names=%d; values preserved\n",changed);
            tp_env_report("canonical entry snapshot",env);
        }
    }
    b.stub=(U8 *)VirtualAlloc(0,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if(!b.stub) fail("TELNETPM cannot allocate entry stub");
    b.stub_base=(U32)(ULONG_PTR)b.stub;
    ret=b.stub_base;
    tp_seq(&b,"\xfc\x50",2); tp_branch(&b,0xe8,(U32)(ULONG_PTR)tp_returned); tp_seq(&b,"\x0f\x0b",2);
    entry=b.stub_base+b.used;
    stack=(U32)(ULONG_PTR)x->objects[4].mapped+x->stack_esp;
    tp_byte(&b,0xbc); tp_word(&b,stack);
    tp_byte(&b,0x68); tp_word(&b,cmd);
    tp_byte(&b,0x68); tp_word(&b,env);
    tp_seq(&b,"\x6a\x00\x6a\x01\x68",5); tp_word(&b,ret);
    tp_seq(&b,"\x31\xdb\x31\xc9\x31\xd2\x31\xf6\x31\xff\x31\xed\xfc",13);
    tp_branch(&b,0xe9,(U32)(ULONG_PTR)x->objects[0].mapped+x->entry_eip);
    {
        DWORD oldp;
        if(!VirtualProtect(b.stub,4096,PAGE_EXECUTE_READ,&oldp)) fail("TELNETPM entry protection failed");
    }
    FlushInstructionCache(GetCurrentProcess(),b.stub,b.used);
    SetUnhandledExceptionFilter(guest_exception_filter);
    if(!g_quiet) printf("TELNETPM: entering original entry; native socket adapters enabled; native FS preserved.\n");
    fflush(stdout); fflush(stderr);
    start=(void (__cdecl *)(void))(ULONG_PTR)entry; start();
    fail("TELNETPM entry unexpectedly returned to host");
}

static void tp_run(struct LxImage *x,const char *name,const char *arg0_override,
                   int argc,char **argv,int first_arg,int trace)
{
    struct TpBridge b;
    U32 i,missing;
    DWORD oldp;
    if(!tp_match(x)) fail("TELNETPM fingerprint mismatch; guest was NOT executed");
    tp_trace_enabled=trace;
    if(!g_quiet) printf("TELNETPM-1993: exact SHA-256 matched; native compatibility profile%s.\n",
                        trace?" (trace enabled)":"");
    scan_fixups(x); map_objects(x);
    memset(&b,0,sizeof(b)); tp_image=x;
    tp_tls=TlsAlloc();
    if(tp_tls==TLS_OUT_OF_INDEXES) fail("TELNETPM TLS allocation failed");
    InitializeCriticalSection(&tp_info_lock);
    g_main_guest_stack_low=(U32)(ULONG_PTR)x->objects[4].mapped;
    g_main_guest_stack_high=g_main_guest_stack_low+x->stack_esp;
    for(i=0;i<x->object_count;++i) { b.bytes[i]=x->objects[i].mapped; b.base[i]=(U32)(ULONG_PTR)b.bytes[i]; }
    b.stub=(U8 *)VirtualAlloc(0,TP_STUB_CAP,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if(!b.stub) fail("TELNETPM bridge allocation failed");
    b.stub_base=(U32)(ULONG_PTR)b.stub;
    b.trace_fn=(U32)(ULONG_PTR)tp_import_observe;
    b.fs_fn=(U32)(ULONG_PTR)tp_fs_observe;
    b.reject_fn=(U32)(ULONG_PTR)tp_thunk_reject;
    missing=0;
    for(i=0;i<x->import_count;++i) {
        tp_native[i]=tp_resolve(x,i);
        if(!tp_native[i]) ++missing;
        /* Normal launches call resolved APIs directly. Missing APIs retain
         * the diagnostic trap; --telnetpm-probe wraps every call as before. */
        b.imports[i]=tp_bind_import(&b,i,tp_native[i],trace);
        if(trace) printf("TP IMPORT %s.%lu: %s\n",x->modules[x->imports[i].module].name,
            (unsigned long)x->imports[i].ordinal,tp_native[i]?"native":"stop on call");
    }
    tp_install(x,&b);
    /* Resource publication is useful when the user's PMWIN is installed.
     * If it is absent, the first PMWIN call will stop through its import stub. */
    for(i=0;i<x->import_module_count;++i)
        if(ascii_ieq(x->modules[i].name,"PMWIN") && x->modules[i].handle) publish_main_pm_resources(x);
    protect_objects(x);
    if(!VirtualProtect(b.bytes[1],x->objects[1].size,PAGE_NOACCESS,&oldp) ||
       !VirtualProtect(b.stub,TP_STUB_CAP,PAGE_EXECUTE_READ,&oldp)) fail("TELNETPM bridge protection failed");
    FlushInstructionCache(GetCurrentProcess(),0,0);
    if(!g_quiet) printf("TELNETPM: %lu imports deferred to fail-stop traps.\n",(unsigned long)missing);
    tp_enter(x,name,arg0_override,argc,argv,first_arg);
}
#endif
