/* Native-personality policy. No Windows headers, handles or API calls. */
#include <string.h>
#include "os2_doscalls_backend.h"
static void lock(struct Os2DosSession *s)
{ if(s->backend && s->backend->state_lock) s->backend->state_lock(s->backend_opaque); }
static void unlock(struct Os2DosSession *s)
{ if(s->backend && s->backend->state_unlock) s->backend->state_unlock(s->backend_opaque); }
static void word(unsigned char *p,O2ULONG n,unsigned count)
{ unsigned i;for(i=0;i<count;++i) { p[i]=(unsigned char)n;n>>=8; } }
void os2_dos_remember_mode(struct Os2DosSession *s,O2HFILE h,O2ULONG mode)
{ if(!s || h>=O2_DOS_MAX_HANDLES) return;lock(s);s->file_modes[h]=mode;s->file_mode_known[h]=1;unlock(s); }
O2APIRET os2_dos_DosForceDelete(struct Os2DosSession *s,const char *path)
{
    if(!s || !path || !*path) return O2_ERROR_INVALID_PARAMETER;
    if(!s->platform || !s->platform->delete_file) return O2_ERROR_INVALID_FUNCTION;
    return s->platform->delete_file(s->backend_opaque,path);
}
O2APIRET os2_dos_DosCopy(struct Os2DosSession *s,const char *from,const char *to,O2ULONG flags)
{
    if(!s || !from || !to || !*from || !*to || (flags&~7UL)) return O2_ERROR_INVALID_PARAMETER;
    /* Append and fail-on-EA require a streaming/EA implementation. */
    if(flags&6) return 50;
    if(!s->platform || !s->platform->copy_file) return O2_ERROR_INVALID_FUNCTION;
    return s->platform->copy_file(s->backend_opaque,from,to,(flags&1)!=0);
}
O2APIRET os2_dos_DosSetMaxFH(struct Os2DosSession *s,O2ULONG count)
{
    if(!s) return O2_ERROR_INVALID_PARAMETER;
    if(count>O2_DOS_MAX_HANDLES) return O2_ERROR_NOT_ENOUGH_MEMORY;
    lock(s);
    if(count<s->max_file_handles) { unlock(s);return O2_ERROR_INVALID_PARAMETER; }
    s->max_file_handles=count;
    unlock(s);return 0;
}
static O2APIRET flush_one(struct Os2DosSession *s,O2HFILE h,int all)
{
    O2NATIVE n;O2APIRET rc;int readonly;
    rc=os2_dos_resolve_hfile(s,h,&n);if(rc) return rc;
    lock(s);readonly=s->file_mode_known[h] && !(s->file_modes[h]&3);unlock(s);
    if(readonly) return 0;
    return s->platform->flush_file(s->backend_opaque,n,all);
}
O2APIRET os2_dos_DosResetBuffer(struct Os2DosSession *s,O2HFILE h)
{
    O2APIRET rc,first=0;O2ULONG i;
    if(!s) return O2_ERROR_INVALID_PARAMETER;
    if(!s->platform || !s->platform->flush_file) return O2_ERROR_INVALID_FUNCTION;
    if(h!=0xffffffffUL && h!=0xffffUL) return flush_one(s,h,0);
    for(i=0;i<O2_DOS_MAX_HANDLES;++i) {
        rc=flush_one(s,i,1);
        if(rc && rc!=O2_ERROR_INVALID_HANDLE && !first) first=rc;
    }
    return first;
}
O2APIRET os2_dos_DosQueryFHState(struct Os2DosSession *s,O2HFILE h,O2ULONG *mode)
{
    O2NATIVE n;O2APIRET rc;
    if(!s || !mode) return O2_ERROR_INVALID_PARAMETER;
    rc=os2_dos_resolve_hfile(s,h,&n);if(rc) return rc;
    lock(s);
    if(!s->file_mode_known[h]) { unlock(s);return 50; }
    *mode=s->file_modes[h];unlock(s);return 0;
}
O2APIRET os2_dos_DosSetFHState(struct Os2DosSession *s,O2HFILE h,O2ULONG mode)
{
    O2NATIVE n;O2ULONG old;O2APIRET rc;
    if(!s || (mode&~0x7f88UL)) return O2_ERROR_INVALID_PARAMETER;
    rc=os2_dos_DosQueryFHState(s,h,&old);if(rc) return rc;
    /* Inheritance is mutable. Reject unsupported changes to I/O policy;
     * Windows cannot change CreateFile caching flags on an existing handle. */
    if((mode^old)&0x7f08UL) return 50;
    rc=os2_dos_resolve_hfile(s,h,&n);if(rc) return rc;
    if(!s->platform || !s->platform->inherit_file) return O2_ERROR_INVALID_FUNCTION;
    rc=s->platform->inherit_file(s->backend_opaque,n,!(mode&0x80));
    if(!rc) os2_dos_remember_mode(s,h,(old&~0x80UL)|(mode&0x80));
    return rc;
}
O2APIRET os2_dos_DosQueryFSInfo(struct Os2DosSession *s,O2ULONG disk,O2ULONG level,void *buf,O2ULONG cb)
{
    struct O2DosDiskInfo d;O2APIRET rc;unsigned char *p=(unsigned char *)buf;size_t n;
    if(!s || !buf || disk>26) return O2_ERROR_INVALID_PARAMETER;
    if(level!=1 && level!=2) return 124;
    if(cb<(level==1?18UL:17UL)) return O2_ERROR_BUFFER_OVERFLOW;
    if(!s->platform || !s->platform->disk_info) return O2_ERROR_INVALID_FUNCTION;
    memset(&d,0,sizeof(d));rc=s->platform->disk_info(s->backend_opaque,disk,&d);if(rc) return rc;
    if(level==1) {
        if(d.bytes_per_sector>65535) return O2_ERROR_INVALID_PARAMETER;
        word(p,0,4);word(p+4,d.sectors_per_unit,4);word(p+8,d.total_units,4);
        word(p+12,d.free_units,4);word(p+16,d.bytes_per_sector,2);
    } else {
        n=strlen(d.label);if(n>11) n=11;
        memset(p,0,17);word(p,d.serial,4);p[4]=(unsigned char)n;memcpy(p+5,d.label,n);
    }
    return 0;
}
O2APIRET os2_dos_DosTmrQueryFreq(struct Os2DosSession *s,O2ULONG *freq)
{
    uint64_t v;O2APIRET rc;
    if(!s || !freq) return O2_ERROR_INVALID_PARAMETER;
    if(!s->platform || !s->platform->counter) return O2_ERROR_INVALID_FUNCTION;
    rc=s->platform->counter(s->backend_opaque,1,&v);if(rc) return rc;
    if(!v || v>0xffffffffUL) return O2_ERROR_INVALID_PARAMETER;
    *freq=(O2ULONG)v;return 0;
}
O2APIRET os2_dos_DosTmrQueryTime(struct Os2DosSession *s,struct O2DosQword *time)
{
    uint64_t v;O2APIRET rc;
    if(!s || !time) return O2_ERROR_INVALID_PARAMETER;
    if(!s->platform || !s->platform->counter) return O2_ERROR_INVALID_FUNCTION;
    rc=s->platform->counter(s->backend_opaque,0,&v);if(rc) return rc;
    time->lo=(O2ULONG)v;time->hi=(O2ULONG)(v>>32);return 0;
}
O2APIRET os2_dos_DosQueryResourceSize(struct Os2DosSession *s,O2ULONG module,O2ULONG type,O2ULONG id,O2ULONG *size)
{
    if(!s || !size || type>65535 || id>65535) return O2_ERROR_INVALID_PARAMETER;
    *size=0;
    if(!s->platform || !s->platform->resource_size) return O2_ERROR_INVALID_FUNCTION;
    return s->platform->resource_size(s->backend_opaque,module,type,id,size);
}
