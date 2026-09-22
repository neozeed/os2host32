/* C/386 Beta-2 FIL_STANDARD: no next-entry offset; name at byte 23.
 * One complete FILEFINDBUF is returned per call, even for a larger request.
 */
#define BETA_FINDBUF_SIZE 279u
static struct GuestSearch *find_search(struct Runtime *rt, uint32_t id)
{
    uint32_t i;
    for (i=0; i<MAX_SEARCHES; ++i)
        if (rt->searches[i].active && rt->searches[i].id==id) return &rt->searches[i];
    return NULL;
}

static int find_matches(DWORD attr, uint32_t filter)
{
    uint32_t required = (filter >> 8) & 0x37u;
    if ((attr & required) != required) return 0;
    /* Normal searches include read-only/archive, but special entries opt in. */
    return (attr & 0x16u & ~(filter | required)) == 0;
}

static uint32_t find_result(struct Runtime *rt, struct GuestSearch *s,
                             uint32_t buf, uint32_t cb, uint32_t count)
{
    size_t n;
    uint8_t *p;
    uint32_t allocation;
    const char *layout=getenv("WHP_OS2_FIND_LAYOUT");
    int ga=layout && !strcmp(layout,"GA");
    guest_put_u32(rt,count,0);
    if (cb < (ga ? 285u : BETA_FINDBUF_SIZE)) return 111u;
    for (;;) {
        if (!s->pending) {
            if (s->ended) return 18u;
            if (!FindNextFileA(s->native, &s->data)) {
                DWORD e = GetLastError();
                if (e == ERROR_NO_MORE_FILES) s->ended=1;
                return e;
            }
            s->pending=1;
        }
        if (find_matches(s->data.dwFileAttributes, s->filter)) break;
        s->pending=0;
    }
    n=strlen(s->data.cFileName);
    if (n>255u || s->data.nFileSizeHigh) return 111u;
    p=rt->ram+buf;
    memset(p,0,ga ? 285u : BETA_FINDBUF_SIZE);
    if(ga)p+=4; /* oNextEntryOffset remains zero: one returned record. */
    file_stamp(p,&s->data.ftCreationTime);
    file_stamp(p+4,&s->data.ftLastAccessTime);
    file_stamp(p+8,&s->data.ftLastWriteTime);
    wr32(p+12,s->data.nFileSizeLow);
    allocation=s->data.nFileSizeLow;
    if (allocation<=0xfffff000u) allocation=(allocation+4095u)&~4095u;
    wr32(p+16,allocation);
    if(ga) {
        wr32(p+20,s->data.dwFileAttributes&0x37u);p[24]=(uint8_t)n;
        memcpy(p+25,s->data.cFileName,n+1u);
    } else {
        file_put16(p+20,(uint16_t)(s->data.dwFileAttributes&0x37u));
        p[22]=(uint8_t)n;memcpy(p+23,s->data.cFileName,n+1u);
    }
    s->pending=0;
    guest_put_u32(rt,count,1);
    return 0;
}

static uint32_t dispatch_find(struct Runtime *rt, uint32_t ordinal, uint32_t esp)
{
    uint32_t a[7], i, rc, id;
    struct GuestSearch *s;
    for (i=0;i<7;++i) a[i]=0;
    i=ordinal==264 ? 7u : (ordinal==265 ? 4u : 1u);
    if (!file_buffer(esp+4u,i*4u)) return OS2_ERROR_INVALID_PARAMETER;
    for (i=0;i<(ordinal==264 ? 7u : (ordinal==265 ? 4u : 1u));++i)
        a[i]=guest_u32(rt,esp+4u+i*4u);
    fprintf(stderr,"v2: DOSCALLS.%u directory API handle/arg=%08X\n",ordinal,a[0]);
    if (ordinal==263) {
        s=find_search(rt,a[0]);
        if (!s) return OS2_ERROR_INVALID_HANDLE;
        if (!FindClose(s->native)) return GetLastError();
        s->active=0; return 0;
    }
    if (ordinal==265) {
        if (!file_buffer(a[3],4) || !file_buffer(a[1],a[2]) || !guest_u32(rt,a[3]))
            return OS2_ERROR_INVALID_PARAMETER;
        s=find_search(rt,a[0]);
        guest_put_u32(rt,a[3],0);
        if (!s) return OS2_ERROR_INVALID_HANDLE;
        return find_result(rt,s,a[1],a[2],a[3]);
    }
    if (ordinal==264) {
        struct GuestSearch candidate;
        char path[1024];
        if (!file_buffer(a[1],4) || !file_buffer(a[5],4) ||
            !file_buffer(a[3],a[4]) ||
            !guest_copy_cstr(rt,a[0],path,sizeof(path))) return OS2_ERROR_INVALID_PARAMETER;
        fprintf(stderr,"v2: DosFindFirst request path=\"%s\" count=%u cb=%u attr=%08X level=%u\n",
                path,guest_u32(rt,a[5]),a[4],a[2],a[6]);
        if (!guest_u32(rt,a[5])) {
            const char *compat=getenv("WHP_OS2_FIND_ZERO_COUNT");
            if (!compat || strcmp(compat,"1")) return OS2_ERROR_INVALID_PARAMETER;
            fprintf(stderr,"v2: DosFindFirst compatibility: zero input count treated as one\n");
        }
        guest_put_u32(rt,a[5],0);
        if (a[6]!=1u || (a[2]&~0x3737u)) return OS2_ERROR_INVALID_PARAMETER;
        if (a[4]<BETA_FINDBUF_SIZE) return 111u;
        id=guest_u32(rt,a[1]);
        s=find_search(rt,id);
        if (id!=0xffffffffu && id!=1u && !s) return OS2_ERROR_INVALID_HANDLE;
        if (!s) {
            for (i=0;i<MAX_SEARCHES;++i) if (!rt->searches[i].active) break;
            if (i==MAX_SEARCHES) return 4u;
            s=&rt->searches[i];
        }
        memset(&candidate,0,sizeof(candidate));
        candidate.native=FindFirstFileA(path,&candidate.data);
        if (candidate.native==INVALID_HANDLE_VALUE) {
            rc=GetLastError(); return rc==ERROR_FILE_NOT_FOUND ? 18u : rc;
        }
        candidate.filter=a[2]; candidate.pending=1; candidate.active=1;
        rc=find_result(rt,&candidate,a[3],a[4],a[5]);
        if (rc) { FindClose(candidate.native); return rc; }
        if (id==0xffffffffu) {
            /* IDs are never reused in this process, so stale handles stay invalid. */
            if (rt->next_search_id<0x10000u || rt->next_search_id==0xffffffffu) {
                FindClose(candidate.native); guest_put_u32(rt,a[5],0); return 4u;
            }
            id=rt->next_search_id++;
        }
        candidate.id=id;
        if (s->active) FindClose(s->native);
        *s=candidate;
        guest_put_u32(rt,a[1],id);
        fprintf(stderr,"v2: DosFindFirst path=\"%s\" -> HDIR=%08X name=\"%s\"\n",path,id,candidate.data.cFileName);
        return 0;
    }
    return OS2_ERROR_INVALID_FUNCTION;
}
