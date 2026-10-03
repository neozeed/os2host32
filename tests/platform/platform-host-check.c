#include <stdio.h>
#include <string.h>
#include "os2_doscalls_backend.h"
#include "os2_msg.h"
#include "os2_track.h"
static unsigned checks;
#define CHECK(c) do { ++checks;if(!(c)) { printf("FAIL %d: %s\n",__LINE__,#c);return 1; } } while(0)
static unsigned invoked;static O2APIRET result;static O2NATIVE flushed;static int inherited;
static O2APIRET del(void *o,const char *p) { (void)o;(void)p;++invoked;return result; }
static O2APIRET copy(void *o,const char *p,const char *q,int r) { (void)o;(void)p;(void)q;inherited=r;++invoked;return result; }
static O2APIRET flush(void *o,O2NATIVE h,int all) { (void)o;(void)all;flushed=h;++invoked;return result; }
static O2APIRET inherit(void *o,O2NATIVE h,int b) { (void)o;flushed=h;inherited=b;++invoked;return result; }
static O2APIRET disk(void *o,O2ULONG n,struct O2DosDiskInfo *d)
{ (void)o;(void)n;++invoked;d->sectors_per_unit=8;d->bytes_per_sector=512;d->total_units=1000;d->free_units=700;d->serial=0x12345678;strcpy(d->label,"HELLO");return result; }
static O2APIRET counter(void *o,int freq,uint64_t *v)
{ (void)o;++invoked;*v=freq?10000000:((uint64_t)0x12345678<<32)|0xabcdef01;return result; }
static O2APIRET resource(void *o,O2ULONG m,O2ULONG t,O2ULONG i,O2ULONG *size)
{ (void)o;++invoked;if(m!=1 || t!=10 || i!=42) return 2;*size=123;return result; }
static const struct Os2DosPlatformOps ops={del,copy,flush,inherit,disk,counter,resource};
static void put(unsigned char *p,uint32_t n,unsigned bytes)
{ unsigned i;for(i=0;i<bytes;++i) { p[i]=(unsigned char)n;n>>=8; } }
int main(void)
{
    struct Os2DosSession s;O2HFILE h,h2;O2ULONG n,mode;struct O2DosQword q;
    unsigned char buf[128],file[100],bad[100];const char *table[]={"world","X%1"};
    struct O2TrackInfo t;struct O2TrackRect r;int x,y;unsigned i,edges;uint32_t actual;
    os2_dos_session_init(&s,NULL,NULL);s.platform=&ops;
    CHECK(os2_dos_DosForceDelete(&s,NULL)==87 && invoked==0);
    result=5;CHECK(os2_dos_DosForceDelete(&s,"file")==5 && invoked==1);result=0;
    CHECK(os2_dos_DosCopy(&s,"a","b",8)==87);
    CHECK(os2_dos_DosCopy(&s,"a","b",2)==50);
    CHECK(os2_dos_DosCopy(&s,"a","b",0)==0 && inherited==0);
    CHECK(os2_dos_DosCopy(&s,"a","b",1)==0 && inherited==1);
    CHECK(os2_dos_DosSetMaxFH(&s,40)==0 && s.max_file_handles==40);
    CHECK(os2_dos_DosSetMaxFH(&s,1000)==8 && s.max_file_handles==40);
    CHECK(os2_dos_DosSetMaxFH(&s,20)==87 && s.max_file_handles==40);
    CHECK(os2_dos_DosSetMaxFH(&s,0)==87 && s.max_file_handles==40);
    CHECK(os2_dos_DosSetMaxFH(&s,40)==0 && s.max_file_handles==40);
    h=os2_dos_alloc_hfile(&s,123);CHECK(h==3);
    CHECK(os2_dos_DosQueryFHState(&s,h,&mode)==50);
    os2_dos_remember_mode(&s,h,0x42);
    CHECK(os2_dos_DosQueryFHState(&s,h,&mode)==0 && mode==0x42);
    CHECK(os2_dos_DosSetFHState(&s,h,0x80)==0 && inherited==0 && flushed==123);
    CHECK(os2_dos_DosQueryFHState(&s,h,&mode)==0 && mode==0xc2);
    CHECK(os2_dos_DosSetFHState(&s,h,0x4000)==50);
    CHECK(os2_dos_DosSetFHState(&s,h,1)==87);
    result=5;CHECK(os2_dos_DosSetFHState(&s,h,0)==5);
    CHECK(os2_dos_DosQueryFHState(&s,h,&mode)==0 && mode==0xc2);result=0;
    CHECK(os2_dos_DosResetBuffer(&s,h)==0 && flushed==123);
    h2=os2_dos_alloc_hfile(&s,456);os2_dos_remember_mode(&s,h2,0x40);
    invoked=0;CHECK(os2_dos_DosResetBuffer(&s,h2)==0 && invoked==0);
    CHECK(os2_dos_DosResetBuffer(&s,0xffffffffUL)==0 && invoked==1);
    result=29;CHECK(os2_dos_DosResetBuffer(&s,h)==29);result=0;
    os2_dos_free_hfile(&s,h);CHECK(os2_dos_DosQueryFHState(&s,h,&mode)==6);
    h=os2_dos_alloc_hfile(&s,789);CHECK(!s.file_mode_known[h]);
    memset(buf,0xcc,sizeof(buf));CHECK(os2_dos_DosQueryFSInfo(&s,3,1,buf,17)==111 && buf[0]==0xcc);
    CHECK(os2_dos_DosQueryFSInfo(&s,3,1,buf,18)==0 && buf[4]==8 && buf[8]==0xe8 && buf[17]==2 && buf[18]==0xcc);
    CHECK(os2_dos_DosQueryFSInfo(&s,3,2,buf,17)==0 && buf[0]==0x78 && buf[4]==5 && !memcmp(buf+5,"HELLO\0",6));
    CHECK(os2_dos_DosQueryFSInfo(&s,3,99,buf,sizeof(buf))==124);
    CHECK(os2_dos_DosTmrQueryFreq(&s,&n)==0 && n==10000000);
    CHECK(os2_dos_DosTmrQueryTime(&s,&q)==0 && q.lo==0xabcdef01 && q.hi==0x12345678);
    CHECK(os2_dos_DosTmrQueryTime(&s,NULL)==87);
    CHECK(os2_dos_DosQueryResourceSize(&s,1,10,42,&n)==0 && n==123);
    CHECK(os2_dos_DosQueryResourceSize(&s,1,10,43,&n)==2 && n==0);
    CHECK(os2_dos_DosQueryResourceSize(&s,1,65536,42,&n)==87);
    s.platform=NULL;CHECK(os2_dos_DosTmrQueryFreq(&s,&n)==1);
    puts("Portable DOSCALLS validation, state, wire layouts and backend errors PASS");
    /* Independently laid out two-message MKMSGF fixture. */
    memset(file,0,sizeof(file));memcpy(file,"\xffMKMSGF\0TST",11);
    put(file+11,2,2);put(file+13,100,2);file[15]=1;put(file+16,2,2);put(file+18,31,2);
    put(file+31,35,2);put(file+33,47,2);
    memcpy(file+35,"IHello %1!\r\n",12);memcpy(file+47,"EFail %2\r\n",10);
    CHECK(os2_msg_from_file(file,57,100,table,2,(char *)buf,sizeof(buf),&actual)==0);
    CHECK(actual==14 && !memcmp(buf,"Hello world!\r\n",14));
    CHECK(os2_msg_from_file(file,57,101,table,2,(char *)buf,sizeof(buf),&actual)==0);
    CHECK(actual==19 && !memcmp(buf,"TST0101: Fail X%1\r\n",19));
    memset(buf,0xcc,sizeof(buf));CHECK(os2_msg_from_file(file,57,101,table,2,(char *)buf,4,&actual)==316);
    CHECK(actual==4 && !memcmp(buf,"TST0",4) && buf[4]==0xcc);
    CHECK(os2_msg_from_file(file,57,99,table,2,(char *)buf,sizeof(buf),&actual)==317 && actual==0);
    CHECK(os2_msg_from_file(file,57,100,table,10,(char *)buf,sizeof(buf),&actual)==320);
    CHECK(os2_msg_insert(table,1,"%1/%2/%0/%%",11,(char *)buf,sizeof(buf),&actual)==0);
    CHECK(actual==14 && !memcmp(buf,"world/%2/%0/%%",14));
    CHECK(os2_msg_insert(NULL,1,"",0,(char *)buf,0,&actual)==87);
    for(i=0;i<31;++i) CHECK(os2_msg_from_file(file,i,100,table,2,(char *)buf,sizeof(buf),&actual)==319);
    memcpy(bad,file,57);put(bad+33,1,2);CHECK(os2_msg_from_file(bad,57,100,table,2,(char *)buf,sizeof(buf),&actual)==319);
    memcpy(bad,file,57);put(bad+22,56,4);CHECK(os2_msg_from_file(bad,57,101,table,2,(char *)buf,sizeof(buf),&actual)==0 && actual==18);
    memcpy(bad,file,57);put(bad+22,1000,4);CHECK(os2_msg_from_file(bad,57,100,table,2,(char *)buf,sizeof(buf),&actual)==319);
    memcpy(bad,file,57);put(bad+16,9,2);CHECK(os2_msg_from_file(bad,57,100,table,2,(char *)buf,sizeof(buf),&actual)==319);
    memcpy(bad,file,57);bad[35]='?';CHECK(os2_msg_from_file(bad,57,100,table,2,(char *)buf,sizeof(buf),&actual)==317 && actual==0);
    file[15]=0;put(file+31,39,4);put(file+35,51,4);memcpy(file+39,"IHello %1!\r\n",12);memcpy(file+51,"EFail %2\r\n",10);
    CHECK(os2_msg_from_file(file,61,100,table,2,(char *)buf,sizeof(buf),&actual)==0 && actual==14);
    put(file+16,0,2);put(file+18,0,2);CHECK(os2_msg_from_file(file,61,101,table,2,(char *)buf,sizeof(buf),&actual)==0);
    puts("MKMSGF 16/32-bit indexes, v0/v2, insertion, prefix, bounds and truncation PASS");
    memset(&t,0,sizeof(t));t.rect.left=t.rect.bottom=20;t.rect.right=t.rect.top=60;
    t.boundary.right=t.boundary.top=100;t.min_x=t.min_y=10;t.max_x=t.max_y=80;t.grid_x=t.grid_y=8;
    t.flags=0xaf;CHECK(os2_track_valid(&t));os2_track_step(&t,9,-9,&r);
    CHECK(r.left==28 && r.bottom==12 && r.right==68 && r.top==52);
    os2_track_step(&t,1000,-1000,&r);CHECK(r.right==100 && r.bottom==0 && r.right-r.left==40);
    for(edges=1;edges<=15;++edges) for(x=-120;x<=120;x+=13) for(y=-120;y<=120;y+=17) {
        t.flags=0x80|edges;CHECK(os2_track_valid(&t));os2_track_step(&t,x,y,&r);
        CHECK(r.left>=0 && r.right<=100 && r.bottom>=0 && r.top<=100);
        CHECK(r.right-r.left>=10 && r.right-r.left<=80 && r.top-r.bottom>=10 && r.top-r.bottom<=80);
    }
    t.flags=0x101;CHECK(!os2_track_valid(&t));t.flags=0;t.rect.right=0;CHECK(!os2_track_valid(&t));
    printf("R11 portable platform: %u checks PASS\n",checks);return 0;
}
