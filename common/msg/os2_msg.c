/* MKMSGF v0/v2 decoder and insertion semantics; no host file or UI API. */
#include <string.h>
#include <stdio.h>
#include "os2_msg.h"
static uint32_t number(const unsigned char *p,unsigned bytes)
{ uint32_t n=0;unsigned i;for(i=0;i<bytes;++i) n|=(uint32_t)p[i]<<(8*i);return n; }
struct Sink { char *data;uint32_t cap,used;int overflow; };
static void put(struct Sink *s,const char *p,uint32_t n)
{
    uint32_t room=s->cap-s->used,copy=n<room?n:room;
    if(copy) memcpy(s->data+s->used,p,copy);
    s->used+=copy;if(copy<n) s->overflow=1;
}
static uint32_t validate(const char *const *table,uint32_t count,char *dst,uint32_t cb,uint32_t *actual)
{
    uint32_t i;if(!actual || (!dst && cb)) return 87;*actual=0;
    if(count>9) return 320;
    if(count && !table) return 87;
    for(i=0;i<count;++i) if(!table[i]) return 87;
    return 0;
}
static void insert(struct Sink *s,const char *src,uint32_t len,const char *const *table,uint32_t count)
{
    uint32_t i=0,n;
    while(i<len) {
        if(src[i]=='%' && i+1<len && src[i+1]>='1' && src[i+1]<='9' && (n=(uint32_t)(src[i+1]-'1'))<count) {
            put(s,table[n],(uint32_t)strlen(table[n]));i+=2;
        } else { put(s,src+i,1);++i; }
    }
}
uint32_t os2_msg_insert(const char *const *table,uint32_t count,const char *src,uint32_t len,char *dst,uint32_t cb,uint32_t *actual)
{
    struct Sink s;uint32_t rc=validate(table,count,dst,cb,actual);
    if(rc) return rc;
    if(!src && len) return 87;
    s.data=dst;s.cap=cb;s.used=0;s.overflow=0;insert(&s,src,len,table,count);
    *actual=s.used;return s.overflow?316:0;
}
uint32_t os2_msg_from_file(const unsigned char *p,uint32_t size,uint32_t id,const char *const *table,uint32_t count,char *dst,uint32_t cb,uint32_t *actual)
{
    uint32_t rc,total,first,version,index,width,start,end,ext,i,off,prev=0;struct Sink s;char prefix[16];
    rc=validate(table,count,dst,cb,actual);if(rc) return rc;
    if(!p || size<31 || memcmp(p,"\xffMKMSGF\0",8)) return 319;
    total=number(p+11,2);first=number(p+13,2);version=number(p+16,2);
    if(version!=0 && version!=2) return 319;
    if(p[15]>1 || total>65536UL-first) return 319;
    width=p[15]?2:4;index=version?number(p+18,2):31;
    if(index<31 || index>size || total>(size-index)/width) return 319;
    ext=version?number(p+22,4):0;
    if(ext && (ext>size || ext<index+total*width)) return 319;
    end=ext?ext:size;
    for(i=0;i<total;++i) {
        off=number(p+index+i*width,width);
        if(off<index+total*width || off>=end || (i && off<=prev)) return 319;
        prev=off;
    }
    if(id<first || id-first>=total) return 317;
    i=id-first;start=number(p+index+i*width,width);
    if(i+1<total) end=number(p+index+(i+1)*width,width);
    if(!strchr("IWEHP?",p[start]) || !p[start]) return 319;
    if(p[start]=='?') return 317;
    s.data=dst;s.cap=cb;s.used=0;s.overflow=0;
    if(p[start]=='E' || p[start]=='W') {
        sprintf(prefix,"%.3s%04lu: ",(const char *)p+8,(unsigned long)id);
        put(&s,prefix,(uint32_t)strlen(prefix));
    }
    insert(&s,(const char *)p+start+1,end-start-1,table,count);
    *actual=s.used;return s.overflow?316:0;
}
