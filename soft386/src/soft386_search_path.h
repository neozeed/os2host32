/* DosSearchPath: environment-name mode is resolved from the guest PIB before
 * the native filesystem service sees a literal path. No host getenv fallback. */
static uint32_t guest_search_path(struct Runtime *rt,uint32_t flags,uint32_t path,uint32_t file,uint32_t out,uint32_t cb)
{
    struct Soft386GuestMemoryOps m;char paths[4096],name[4096],key[4096],entry[65536];
    uint32_t env,n,rc;char *buffer;void *fn;int found=0;
    bridge_memory_ops(rt,&m);
    if((flags&~7u)||!file||!out||!cb||cb>0x01000000u||!m.valid(m.opaque,out,cb,1)||
       !m.read_cstr(m.opaque,file,name,sizeof(name)))return OS2_ERROR_INVALID_PARAMETER;
    paths[0]=0;
    if(path&&!m.read_cstr(m.opaque,path,paths,sizeof(paths)))return OS2_ERROR_INVALID_PARAMETER;
    if(flags&2u){
        if(!*paths)return 203;
        strcpy(key,paths);env=guest_u32(rt,GUEST_INFO+16);n=0;
        while(n<65536u&&env<=UINT32_MAX-n){
            char *eq;
            if(!m.read_cstr(m.opaque,env+n,entry,sizeof(entry)))return OS2_ERROR_INVALID_PARAMETER;
            if(!entry[0])break;
            eq=strchr(entry,'=');
            if(eq){*eq=0;if(!_stricmp(entry,key)){
                if(strlen(eq+1)>4090u)return 111;
                strcpy(paths,eq+1);found=1;break;}*eq='=';}
            if(strlen(entry)+1>65536u-n)return OS2_ERROR_INVALID_PARAMETER;
            n+=(uint32_t)strlen(entry)+1;
        }
        if(!found||!*paths)return 203;flags&=~2u;
    }
    /* Leave space for the backend's optional current-directory prefix. */
    if(strlen(paths)>4090u)return 111;
    if(!rt->native_dos.loaded||!(fn=rt->native_dos.get_proc(rt->native_dos.opaque,228)))return OS2_ERROR_INVALID_FUNCTION;
    buffer=calloc(1,cb);if(!buffer)return OS2_ERROR_NOT_ENOUGH_MEMORY;
    rc=((uint32_t (*)(uint32_t,const char *,const char *,char *,uint32_t))fn)(flags,paths,name,buffer,cb);
    if(!rc){char *end=memchr(buffer,0,cb);if(!end)rc=111;
        else if(!m.write(m.opaque,out,buffer,(uint32_t)(end-buffer)+1))rc=OS2_ERROR_INVALID_PARAMETER;}
    free(buffer);return rc;
}
