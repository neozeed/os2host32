/* Included by the vessel: LE/LX module identity and addresses never leave RAM.
 * The export decoder follows the existing native/WHP loader contracts. */
static uint32_t system_module_id(const char *name)
{
    static const char *names[]={"DOSCALLS","","VIOCALLS","KBDCALLS","SESMGR","NLS","","PMWIN","PMGPI","PMCTLS","MSG","PMSHAPI","HELPMGR","PMWP","QUECALLS","","SO32DLL","TCP32DLL"};
    char n[256]; size_t len; unsigned i;
    if(strlen(name)>=sizeof(n))return 0;
    strcpy(n,base_name(name));len=strlen(n);
    if(len>4&&!_stricmp(n+len-4,".DLL"))n[len-4]=0;
    for(i=0;i<sizeof(names)/sizeof(names[0]);i++)if(names[i][0]&&!_stricmp(n,names[i]))return (i+1u)<<24;
    return 0;
}

static int module_equal(const char *a,const char *b)
{
    char x[256],y[256];size_t n;
    a=base_name(a);b=base_name(b);
    if(strlen(a)>=sizeof(x)||strlen(b)>=sizeof(y))return 0;
    strcpy(x,a);strcpy(y,b);
    n=strlen(x);if(n>4&&!_stricmp(x+n-4,".DLL"))x[n-4]=0;
    n=strlen(y);if(n>4&&!_stricmp(y+n-4,".DLL"))y[n-4]=0;
    return !_stricmp(x,y);
}

static struct GuestModule *find_guest_module(struct Runtime *rt,const char *name)
{
    unsigned i;for(i=0;i<MAX_MODULES;i++)
        if(rt->modules[i]&&module_equal(rt->modules[i]->name,name))return rt->modules[i];
    return NULL;
}
static struct GuestModule *module_handle(struct Runtime *rt,uint32_t h)
{
    unsigned i;for(i=0;i<MAX_MODULES;i++)if(rt->modules[i]&&rt->modules[i]->handle==h)return rt->modules[i];
    return NULL;
}

static int module_path_try(const char *dir,size_t n,const char *leaf,char *out)
{
    FILE *f; size_t i;
    if(n+strlen(leaf)+2u>=4096)return 0;
    memcpy(out,dir,n);if(n&&out[n-1]!='/'&&out[n-1]!='\\')out[n++]='/';strcpy(out+n,leaf);
#ifndef _WIN32
    for(i=0;out[i];i++)if(out[i]=='\\')out[i]='/';
#else
    (void)i;
#endif
    f=fopen(out,"rb");if(f){fclose(f);return 1;}
#ifndef _WIN32
    /* OS/2 names are case insensitive; try the usual all-lowercase leaf too. */
    for(i=n;out[i];i++)if(out[i]>='A'&&out[i]<='Z')out[i]=(char)(out[i]+'a'-'A');
    f=fopen(out,"rb");if(f){fclose(f);return 1;}
#endif
    return 0;
}
static int locate_guest_module(struct Runtime *rt,const char *name,char *out)
{
    char leaf[4096];const char *p,*q,*base;size_t n;
    if(strlen(name)>sizeof(leaf)-5)return 0;
    strcpy(leaf,name);n=strlen(leaf);
    if(!strchr(base_name(leaf),'.'))strcat(leaf,".DLL");
    if(strchr(name,'/')||strchr(name,'\\')||strchr(name,':'))return module_path_try("",0,leaf,out);
    base=base_name(rt->main_path);
    if(module_path_try(rt->main_path,(size_t)(base-rt->main_path),leaf,out))return 1;
    if(module_path_try("",0,leaf,out))return 1;
    p=getenv("OS2LIBPATH");if(!p)return 0;
    do {q=strchr(p,';');n=q?(size_t)(q-p):strlen(p);
        if(module_path_try(p,n,leaf,out))return 1;
        if(!q)break;p=q+1;
    }while(*p);
    return 0;
}

static uint32_t export_ordinal(struct LeImage *x,uint32_t wanted)
{
    uint32_t p=x->entry_table_off,ord=1,i,obj,off;uint8_t count,type,flags;
    if(!wanted||p<=x->le)return 0;
    for(;;){
        if(!file_range(x,p,1))fatal("truncated DLL entry table");
        count=x->file[p++];if(!count)return 0;
        if(!file_range(x,p,1))fatal("truncated DLL entry bundle");
        type=x->file[p++];if(type&0x80)fatal("typed DLL entry bundle unsupported");
        if(!type){ord+=count;continue;}
        if(type>3)fatal("DLL forwarder/call gate unsupported");
        if(!file_range(x,p,2))fatal("truncated DLL entry object");obj=rd16(x->file+p);p+=2;
        for(i=0;i<count;i++,ord++){
            uint32_t size=type==1?3u:5u;
            if(!file_range(x,p,size))fatal("truncated DLL export");
            flags=x->file[p];off=type==3?rd32(x->file+p+1):rd16(x->file+p+1);p+=size;
            if(ord!=wanted)continue;
            if(!(flags&1u))return 0;
            if(type!=3||!obj||obj>x->num_objects)fatal("DLL export is not a flat32 object");
            if(off>=x->objects[obj-1].size)fatal("DLL export outside object");
            return x->objects[obj-1].mapped_addr+off;
        }
    }
}
static uint32_t name_ordinal(struct LeImage *x,uint32_t p,uint32_t end,const char *name)
{
    if(!p||p>=end||end>x->file_size)return 0;
    while(p<end){uint32_t n=x->file[p++],ord;if(!n)break;
        if(n+2u>end-p)fatal("truncated DLL export name");
        ord=rd16(x->file+p+n);
        if(strlen(name)==n){char found[256];memcpy(found,x->file+p,n);found[n]=0;if(!_stricmp(found,name))return ord;}
        p+=n+2u;
    }return 0;
}
static uint32_t export_name(struct LeImage *x,const char *name)
{
    uint32_t ord=name_ordinal(x,x->resident_name_off,x->entry_table_off,name);
    if(!ord&&x->nonresident_name_len){
        if(!file_range(x,x->nonresident_name_off,x->nonresident_name_len))fatal("bad nonresident names");
        ord=name_ordinal(x,x->nonresident_name_off,x->nonresident_name_off+x->nonresident_name_len,name);
    }return export_ordinal(x,ord);
}

static void module_resources(struct Runtime *rt,struct LeImage *x,uint32_t handle)
{
    uint32_t p=x->le+rd32(x->file+x->le+0x50),n=rd32(x->file+x->le+0x54),i;
    if(n>S386_PM_RESOURCES||!file_range(x,p,n*14u))fatal("invalid resource table");
    for(i=0;i<n;i++,p+=14){uint32_t obj=rd16(x->file+p+8),off=rd32(x->file+p+10),cb=rd32(x->file+p+4);
        if(!obj||obj>x->num_objects||off>x->objects[obj-1].size||cb>x->objects[obj-1].size-off)
            fatal("resource outside module object");
        if(cb&&!soft386_pm_add_resource(&rt->pm,handle==1?0:handle,rd16(x->file+p),rd16(x->file+p+2),
            rt->ram+x->objects[obj-1].mapped_addr+off,cb))fatal("cannot copy module resource");
    }
}

static struct GuestModule *load_guest_module(struct Runtime *,const char *);
static void resolve_imports(struct Runtime *rt,struct LeImage *x)
{
    uint32_t i,id,addr;struct GuestModule *g;
    /* Load dependency graph first, including modules with no referenced entry. */
    for(i=0;i<x->num_impmods;i++)if(!system_module_id(x->modules[i].name)){
        g=load_guest_module(rt,x->modules[i].name);
        if(!g){fprintf(stderr,"soft386: missing guest DLL %s\n",x->modules[i].name);fatal("DLL dependency not found");}
        g->pinned=1;
    }
    for(i=0;i<x->nimports;i++){
        const char *name=x->modules[x->imports[i].module].name;id=system_module_id(name);
        if(id)addr=hostcall_stub(rt,id,x->imports[i].ordinal);
        else{g=find_guest_module(rt,name);addr=export_ordinal(&g->image,x->imports[i].ordinal);}
        if(!addr){fprintf(stderr,"soft386: unresolved %s.%u\n",name,x->imports[i].ordinal);fatal("unresolved DLL export");}
        x->imports[i].address=addr;
        if(rt->trace_hc||!rt->quiet)fprintf(stderr,"soft386: resolve %s.%u -> %08X\n",name,x->imports[i].ordinal,addr);
    }
    for(i=0;i<x->nname_imports;i++){
        char proc[256];uint32_t off=x->name_imports[i].name_offset,p,n;
        const char *name=x->modules[x->name_imports[i].module].name;
        if(off>UINT32_MAX-x->impproc_off)fatal("import name offset overflow");p=x->impproc_off+off;
        if(!file_range(x,p,1))fatal("missing import name");n=x->file[p++];
        if(!n||!file_range(x,p,n))fatal("invalid import name");memcpy(proc,x->file+p,n);proc[n]=0;
        id=system_module_id(name);
        if(id>=HC_PMWIN&&id<=HC_HELPMGR){uint32_t ord=soft386_pm_named_ordinal((id-HC_PMWIN)>>24,proc);addr=ord?hostcall_stub(rt,id,ord):0;}
        else if(id==HC_SO32DLL||id==HC_TCP32DLL){uint32_t ord=soft386_net_named_ordinal(id==HC_TCP32DLL,proc);addr=ord?hostcall_stub(rt,id,ord):0;}
        else if(id==HC_QUECALLS){uint32_t ord=soft386_queue_named_ordinal(proc);addr=ord?hostcall_stub(rt,id,ord):0;}
        else if(id==HC_PMWP)addr=!strcmp(proc,"PMWPOrdinal203")?hostcall_stub(rt,id,203):0;
        else if(id)addr=0;
        else{g=find_guest_module(rt,name);addr=export_name(&g->image,proc);}
        if(!addr){fprintf(stderr,"soft386: unresolved %s.%s\n",name,proc);fatal("unresolved named export");}
        x->name_imports[i].address=addr;
    }
}
static struct GuestModule *load_guest_module(struct Runtime *rt,const char *name)
{
    struct GuestModule *g=find_guest_module(rt,name);unsigned slot;char path[4096];const char *previous_path=loader_path;
    if(g)return g; /* Objects/exports exist before resolving cyclic imports. */
    if(!locate_guest_module(rt,name,path))return NULL;
    for(slot=0;slot<MAX_MODULES;slot++)if(!rt->modules[slot])break;
    if(slot==MAX_MODULES)return NULL;
    g=calloc(1,sizeof(*g));if(!g)return NULL;
    g->handle=0x1000u+slot;g->state=1;strncpy(g->name,base_name(name),sizeof(g->name)-1);strcpy(g->path,path);
    g->image.file=load_file(path,&g->image.file_size);if(!g->image.file){free(g);return NULL;}
    loader_path=g->path;
    if(rt->trace_hc)fprintf(stderr,"soft386: loading guest DLL %s from %s\n",name,path);
    parse_header(&g->image);
    if((g->image.module_flags&MOD_TYPE_MASK)!=MOD_TYPE_DLL)fatal("guest library is not an LE/LX DLL");
    parse_objects(&g->image,rt,0);parse_import_modules(&g->image);scan_fixups(&g->image);
    rt->modules[slot]=g;
    module_resources(rt,&g->image,g->handle);
    resolve_imports(rt,&g->image);apply_fixups(&g->image,rt->ram);install_c386_helpers(&g->image,rt);
    g->state=2;
    loader_path=previous_path;
    if(rt->trace_hc)fprintf(stderr,"soft386: DLL %s handle=%08X first-object=%08X\n",path,g->handle,g->image.objects[0].mapped_addr);
    return g;
}

static void free_guest_modules(struct Runtime *rt)
{
    unsigned i;for(i=0;i<MAX_MODULES;i++)if(rt->modules[i]){
        free_le_image(&rt->modules[i]->image);free(rt->modules[i]);rt->modules[i]=NULL;
    }
}
