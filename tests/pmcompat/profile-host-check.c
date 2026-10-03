/* Run the production profile adapter with a controlled Win32 profile API.
   This checks handle/encoding semantics, not Windows INI disk behavior. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../dlls/pmshapi/pmshapi.c"
/* Shell lifecycle is covered separately by switchlist-host-check. */
void pmsh_switch_init(void) { }
void pmsh_switch_term(void) { }

static char stored[1024];
static int present;
static int flushes;
void *GetProcessHeap(void) { return (void *)1; }
void *HeapAlloc(void *heap,DWORD flags,size_t n)
{ (void)heap; return flags&8 ? calloc(1,n) : malloc(n); }
BOOL HeapFree(void *heap,DWORD flags,void *p)
{ (void)heap; (void)flags; free(p); return TRUE; }
DWORD GetModuleFileNameA(void *m,char *p,DWORD n)
{ (void)m; (void)n; strcpy(p,"C:\\test\\host.exe"); return (DWORD)strlen(p); }
DWORD GetEnvironmentVariableA(const char *n,char *v,DWORD z)
{ (void)n; (void)v; (void)z; return 0; }
DWORD GetFullPathNameA(const char *n,DWORD z,char *p,char **tail)
{
    size_t need=strlen(n)+9;
    (void)tail;
    if(need>=z) return (DWORD)need;
    strcpy(p,"C:\\test\\"); strcat(p,n); return (DWORD)strlen(p);
}
BOOL WritePrivateProfileStringA(const char *app,const char *key,const char *value,const char *file)
{
    (void)file;
    if(!app && !key && !value) { ++flushes; return FALSE; }
    if(!value) { present=0; return TRUE; }
    if(strlen(value)>=sizeof(stored)) return FALSE;
    strcpy(stored,value); present=1; return TRUE;
}
DWORD GetPrivateProfileStringA(const char *app,const char *key,const char *def,char *out,DWORD cap,const char *file)
{
    const char *s=present?stored:def;
    size_t n=strlen(s);
    (void)file;
    if(!app || !key) {
        if(!present) { out[0]=out[1]=0; return 0; }
        memcpy(out,"name\0\0",6); return 5;
    }
    if(n>=cap) n=cap-1;
    memcpy(out,s,n);out[n]=0;return (DWORD)n;
}
unsigned GetPrivateProfileIntA(const char *a,const char *k,INT d,const char *f)
{ (void)a;(void)k;(void)f;return (unsigned)d; }
#define CHECK(c) do { ++checks; if(!(c)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c); return 1; } } while(0)
int main(void)
{
    DWORD handles[32],size,h,old;
    unsigned i;
    unsigned char data[4]={0,255,1,0},out[4];
    int checks=0;
    h=PrfOpenProfile(1,"profile.ini"); CHECK(h!=0);
    size=99;CHECK(!PrfQueryProfileSize(h,"a","k",&size) && size==0);
    CHECK(PrfWriteProfileData(h,"a","k",data,4));
    CHECK(PrfQueryProfileSize(h,"a","k",&size) && size==4);
    size=4;CHECK(PrfQueryProfileData(h,"a","k",out,&size) && !memcmp(data,out,4));
    CHECK(PrfQueryProfileSize(h,NULL,NULL,&size) && size==6);
    CHECK(PrfWriteProfileString(h,"a","k",""));
    CHECK(PrfQueryProfileSize(h,"a","k",&size) && size==1);
    CHECK(PrfCloseProfile(h) && flushes==1);
    CHECK(!PrfCloseProfile(h));
    CHECK(!PrfQueryProfileSize(h,"a","k",&size));
    CHECK(!PrfWriteProfileData(h,"a","k",data,4));
    CHECK(!PrfCloseProfile(0xffffffffU));
    for(i=0;i<32;++i) { handles[i]=PrfOpenProfile(1,"p.ini"); CHECK(handles[i]!=0); }
    CHECK(PrfOpenProfile(1,"full.ini")==0);
    old=handles[5];CHECK(PrfCloseProfile(old));
    handles[5]=PrfOpenProfile(1,"reused.ini");CHECK(handles[5]!=0 && handles[5]!=old);
    CHECK(!PrfCloseProfile(old));
    for(i=0;i<32;++i) CHECK(PrfCloseProfile(handles[i]));
    printf("profile-host-check: %d checks PASS (mock Win32 profile I/O)\n",checks);
    return 0;
}
