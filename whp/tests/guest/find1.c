#define INCL_DOSFILEMGR
#include <os2.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { printf("find1 FAIL line %d\n",__LINE__); return 1; } } while(0)
static int makefile(char *path, ULONG attr, ULONG size)
{
    HFILE h; ULONG action,n; APIRET rc; char text[]="abcdef";
    rc=DosOpen((PSZ)path,&h,&action,0,attr,0x10,0x42,0);
    if(rc) { printf("create %s rc=%lu\n",path,(ULONG)rc); return 0; }
    rc=DosWrite(h,(PVOID)text,size,&n);
    if(DosClose(h)) return 0;
    return rc==0 && n==size && action==2;
}
static unsigned bit(PFILEFINDBUF p)
{
    if(!strcmp(p->achName,"ALPHA.TXT")) return 1;
    if(!strcmp(p->achName,"BETA.TXT")) return 2;
    if(!strcmp(p->achName,"HIDDEN.TXT")) return 4;
    return 0;
}
int main(void)
{
    HDIR h,h2,old; ULONG n; APIRET rc; unsigned seen,b;
    FILEFINDBUF fb,fb2;
    unsigned char small[8];
    char pattern[]="*.TXT",exact[]="ALPHA.TXT",missing[]="NO-SUCH-*.ZZZ",dir[]="SUBDIR";
    char alpha[]="ALPHA.TXT",beta[]="BETA.TXT",hidden[]="HIDDEN.TXT";
    CHECK((char *)fb.achName-(char *)&fb==23);
    CHECK(sizeof(FILEFINDBUF)>=279);
    CHECK(makefile(alpha,0,3)); CHECK(makefile(beta,0,6)); CHECK(makefile(hidden,FILE_HIDDEN,1));
    h=HDIR_CREATE; n=1;
    CHECK(DosFindFirst((PSZ)pattern,&h,0,&fb,sizeof(fb),&n,1)==0 && n==1);
    CHECK(fb.cchName==strlen(fb.achName));
    h2=HDIR_CREATE; n=1;
    CHECK(DosFindFirst((PSZ)exact,&h2,0,&fb2,sizeof(fb2),&n,1)==0 && h2!=h && n==1);
    CHECK(!strcmp(fb2.achName,alpha) && fb2.cbFile==3);
    CHECK(DosFindClose(h2)==0);
    seen=bit(&fb); CHECK(seen==1 || seen==2);
    n=1; CHECK(DosFindNext(h,(PFILEFINDBUF)small,sizeof(small),&n)==111 && n==0);
    n=1; CHECK(DosFindNext(h,&fb,sizeof(fb),&n)==0 && n==1);
    b=bit(&fb); CHECK(b && !(seen&b)); seen|=b; CHECK(seen==3);
    n=1; CHECK(DosFindNext(h,&fb,sizeof(fb),&n)==18 && n==0);
    n=1; CHECK(DosFindNext(h,&fb,sizeof(fb),&n)==18 && n==0);
    old=h; CHECK(DosFindClose(h)==0); CHECK(DosFindClose(old)==6);
    n=1; CHECK(DosFindNext(old,&fb,sizeof(fb),&n)==6 && n==0);
    /* Larger requested counts: accept a partial batch and walk to exhaustion. */
    h=HDIR_CREATE; n=2; seen=0;
    rc=DosFindFirst((PSZ)pattern,&h,FILE_HIDDEN,&fb,sizeof(fb),&n,1);
    while(rc==0) {
        CHECK(n==1); b=bit(&fb); CHECK(b && !(seen&b)); seen|=b;
        if(b==4) CHECK((fb.attrFile&FILE_HIDDEN)!=0 && fb.cbFile==1);
        n=2; rc=DosFindNext(h,&fb,sizeof(fb),&n);
    }
    CHECK(rc==18 && n==0 && seen==7 && h!=old); CHECK(DosFindClose(h)==0);
    h=HDIR_CREATE; n=1; CHECK(DosFindFirst((PSZ)missing,&h,0,&fb,sizeof(fb),&n,1)==18 && n==0);
    h=HDIR_CREATE; n=1; CHECK(DosFindFirst((PSZ)exact,&h,0,&fb,sizeof(fb),&n,99)==87);
    h=HDIR_CREATE; n=0; CHECK(DosFindFirst((PSZ)exact,&h,0,&fb,sizeof(fb),&n,1)==87);
    h=HDIR_SYSTEM; n=1; CHECK(DosFindFirst((PSZ)exact,&h,0,&fb,sizeof(fb),&n,1)==0 && h==HDIR_SYSTEM);
    n=1; CHECK(DosFindFirst((PSZ)dir,&h,FILE_DIRECTORY,&fb,sizeof(fb),&n,1)==0);
    CHECK((fb.attrFile&FILE_DIRECTORY)!=0); CHECK(DosFindClose(h)==0);
    CHECK(DosDelete((PSZ)alpha)==0); CHECK(DosDelete((PSZ)beta)==0); CHECK(DosDelete((PSZ)hidden)==0);
    printf("find1 PASS\n"); return 0;
}
