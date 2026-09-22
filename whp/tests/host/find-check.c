#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
typedef uint32_t DWORD; typedef uint16_t WORD; typedef int32_t LONG; typedef void *HANDLE;
typedef struct { long long QuadPart; } LARGE_INTEGER;
typedef struct { uint32_t a,b; } FILETIME;
typedef struct { WORD wYear,wMonth,wDayOfWeek,wDay,wHour,wMinute,wSecond,wMilliseconds; } SYSTEMTIME;
typedef struct { LONG Bias,StandardBias,DaylightBias; } TIME_ZONE_INFORMATION;
typedef struct { DWORD dwFileAttributes; FILETIME ftCreationTime,ftLastAccessTime,ftLastWriteTime; DWORD nFileSizeHigh,nFileSizeLow; } WIN32_FILE_ATTRIBUTE_DATA;
typedef WIN32_FILE_ATTRIBUTE_DATA BY_HANDLE_FILE_INFORMATION;
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define STD_INPUT_HANDLE 0
#define STD_OUTPUT_HANDLE 1
#define STD_ERROR_HANDLE 2
#define FILE_CURRENT SEEK_CUR
#define FILE_BEGIN SEEK_SET
#define GENERIC_READ 1
#define GENERIC_WRITE 2
#define FILE_SHARE_READ 1
#define FILE_SHARE_WRITE 2
#define FILE_ATTRIBUTE_NORMAL 128
#define FILE_FLAG_WRITE_THROUGH 0x80000000u
#define OPEN_EXISTING 1
#define TRUNCATE_EXISTING 2
#define CREATE_NEW 3
#define OPEN_ALWAYS 4
#define CREATE_ALWAYS 5
#define ERROR_ALREADY_EXISTS 183
#define ERROR_BROKEN_PIPE 109
#define GetFileExInfoStandard 0
#define TIME_ZONE_ID_INVALID 0xffffffffu
#define TIME_ZONE_ID_STANDARD 1
#define TIME_ZONE_ID_DAYLIGHT 2
#define MAX_FILES 128
#define OS2_ERROR_INVALID_PARAMETER 87
#define OS2_ERROR_INVALID_HANDLE 6
#define OS2_ERROR_INVALID_FUNCTION 1
#define RAM_SIZE 65536u
#define MAX_SEARCHES 32
#define ERROR_NO_MORE_FILES 18
#define ERROR_FILE_NOT_FOUND 2
typedef struct { DWORD dwFileAttributes; FILETIME ftCreationTime,ftLastAccessTime,ftLastWriteTime; DWORD nFileSizeHigh,nFileSizeLow; char cFileName[260]; } WIN32_FIND_DATAA;
struct GuestSearch { int active,pending,ended; uint32_t id,filter; HANDLE native; WIN32_FIND_DATAA data; };
struct Runtime { uint8_t *ram; HANDLE files[MAX_FILES]; int std_closed[3]; struct GuestSearch searches[MAX_SEARCHES]; uint32_t next_search_id; };
static DWORD last;
static DWORD GetLastError(void){return last;}
static void SetLastError(DWORD e){last=e;}
static DWORD maperr(void){return last=errno==ENOENT?2:errno==EEXIST?80:errno==EBADF?6:5;}
static int fdof(HANDLE h){return (int)(intptr_t)h-1;}
static HANDLE GetStdHandle(DWORD n){return (HANDLE)(intptr_t)(n+1);}
static HANDLE CreateFileA(const char *p,DWORD access,DWORD share,void *sec,DWORD disp,DWORD attr,void *tmpl){
 int flags=access==3?O_RDWR:access==2?O_WRONLY:O_RDONLY;struct stat st;int exists=stat(p,&st)==0;int fd;
 (void)share;(void)sec;(void)attr;(void)tmpl;
 if(disp==CREATE_NEW)flags|=O_CREAT|O_EXCL;
 if(disp==OPEN_ALWAYS)flags|=O_CREAT;
 if(disp==CREATE_ALWAYS)flags|=O_CREAT|O_TRUNC;
 if(disp==TRUNCATE_EXISTING)flags|=O_TRUNC;
 fd=open(p,flags,0600);if(fd<0){maperr();return INVALID_HANDLE_VALUE;}
 last=exists&&(disp==OPEN_ALWAYS||disp==CREATE_ALWAYS)?183:0;return(HANDLE)(intptr_t)(fd+1);
}
static int CloseHandle(HANDLE h){if(close(fdof(h))){maperr();return 0;}return 1;}
static int SetFilePointerEx(HANDLE h,LARGE_INTEGER d,LARGE_INTEGER *out,DWORD m){off_t r=lseek(fdof(h),d.QuadPart,m);if(r<0){maperr();return 0;}if(out)out->QuadPart=r;return 1;}
static int SetEndOfFile(HANDLE h){if(ftruncate(fdof(h),lseek(fdof(h),0,SEEK_CUR))){maperr();return 0;}return 1;}
static int ReadFile(HANDLE h,void *p,DWORD n,DWORD *done,void *ov){ssize_t r;(void)ov;r=read(fdof(h),p,n);if(r<0){*done=0;maperr();return 0;}*done=(DWORD)r;return 1;}
static int DeleteFileA(const char *p){if(unlink(p)){maperr();return 0;}return 1;}
static DWORD GetFullPathNameA(const char *p,DWORD cb,char *out,void *part){size_t n=strlen(p);(void)part;if(cb>n)memcpy(out,p,n+1);return (DWORD)n;}
static int GetFileAttributesExA(const char *p,int level,WIN32_FILE_ATTRIBUTE_DATA *d){struct stat st;(void)level;if(stat(p,&st)){maperr();return 0;}memset(d,0,sizeof(*d));d->nFileSizeLow=(DWORD)st.st_size;return 1;}
static int GetFileInformationByHandle(HANDLE h,BY_HANDLE_FILE_INFORMATION *d){struct stat st;if(fstat(fdof(h),&st)){maperr();return 0;}memset(d,0,sizeof(*d));d->nFileSizeLow=(DWORD)st.st_size;return 1;}
static int FileTimeToLocalFileTime(const FILETIME *a,FILETIME *b){*b=*a;return 1;}
static int FileTimeToDosDateTime(const FILETIME *f,WORD *d,WORD *t){(void)f;*d=0;*t=0;return 1;}
static void GetLocalTime(SYSTEMTIME *s){memset(s,0,sizeof(*s));s->wYear=2026;s->wMonth=9;s->wDay=20;}
static DWORD GetTimeZoneInformation(TIME_ZONE_INFORMATION *z){memset(z,0,sizeof(*z));return 0;}
static int guest_range(uint32_t a,uint32_t n){return a<RAM_SIZE&&n<=RAM_SIZE-a;}
static uint32_t guest_u32(struct Runtime *r,uint32_t a){uint32_t v;memcpy(&v,r->ram+a,4);return v;}
static void wr32(uint8_t *p,uint32_t v){memcpy(p,&v,4);}
static void guest_put_u32(struct Runtime *r,uint32_t a,uint32_t v){wr32(r->ram+a,v);}
static int guest_copy_cstr(struct Runtime *r,uint32_t a,char *out,uint32_t cap){uint32_t i;for(i=0;i<cap;i++){if(!guest_range(a+i,1))return 0;out[i]=r->ram[a+i];if(!out[i])return 1;}return 0;}
#include "../v2_fileio.h"
static uint32_t call(struct Runtime *r,uint32_t ord,uint32_t a,uint32_t b,uint32_t c,uint32_t d){guest_put_u32(r,104,a);guest_put_u32(r,108,b);guest_put_u32(r,112,c);guest_put_u32(r,116,d);return dispatch_fileio(r,ord,100);}
static int fileio_tests(void){
 struct Runtime r;uint32_t h;LARGE_INTEGER pos;uint8_t *ram=calloc(1,RAM_SIZE);memset(&r,0,sizeof(r));r.ram=ram;
 guest_put_u32(&r,136,0xdeadbeefu); /* not a DosOpen argument */
 strcpy((char*)ram+1000,"/tmp/v2-fileio-unit.dat");unlink((char*)ram+1000);
 guest_put_u32(&r,120,0);guest_put_u32(&r,124,0x10);guest_put_u32(&r,128,0x42);
 assert(call(&r,273,1000,2000,2004,0)==0);h=guest_u32(&r,2000);assert(h>=3&&guest_u32(&r,2004)==2);
 assert(write(fdof(os2_handle(&r,h)),"abcdef",6)==6);pos.QuadPart=0;assert(SetFilePointerEx(os2_handle(&r,h),pos,NULL,FILE_BEGIN));
 assert(call(&r,281,h,3000,9,2008)==0&&guest_u32(&r,2008)==6&&!memcmp(ram+3000,"abcdef",6));
 assert(call(&r,281,h,3000,9,2008)==0&&guest_u32(&r,2008)==0);
 assert(call(&r,272,h,3,0,0)==0);assert(lseek(fdof(os2_handle(&r,h)),0,SEEK_CUR)==6);
 assert(call(&r,279,h,1,4000,24)==0&&guest_u32(&r,4012)==3);
 assert(call(&r,281,h,RAM_SIZE-1,2,2008)==87);
 assert(call(&r,257,h,0,0,0)==0);assert(call(&r,257,h,0,0,0)==6);
 assert(call(&r,223,1000,1,4000,24)==0&&guest_u32(&r,4012)==3);
 assert(call(&r,230,5000,0,0,0)==0&&ram[5005]==9);
 guest_put_u32(&r,124,0x10);assert(call(&r,273,1000,2000,2004,0)==80);
 guest_put_u32(&r,124,0x11);assert(call(&r,273,1000,2000,2004,99)==0&&guest_u32(&r,2004)==1);
 h=guest_u32(&r,2000);assert(call(&r,279,h,1,4000,24)==0&&guest_u32(&r,4012)==3);assert(call(&r,257,h,0,0,0)==0);
 guest_put_u32(&r,124,0x12);assert(call(&r,273,1000,2000,2004,8)==0&&guest_u32(&r,2004)==3);
 h=guest_u32(&r,2000);assert(call(&r,279,h,1,4000,24)==0&&guest_u32(&r,4012)==8);assert(call(&r,257,h,0,0,0)==0);
 assert(call(&r,259,1000,0,0,0)==0);guest_put_u32(&r,124,1);assert(call(&r,273,1000,2000,2004,0)==2);
 free(ram);puts("file helper tests PASS (POSIX backend shim; Windows execution still required)");return 0;
}

/* Deterministic directory backend: tests guest packing/iteration, not Windows
   wildcard implementation. Native Win32 enumeration is tested by find1.exe. */
struct MockFind { int pos, exact; };
static void mock_data(int pos, WIN32_FIND_DATAA *d) {
    static const char *names[]={"ALPHA.TXT","HIDDEN.TXT","BETA.TXT","SUBDIR"};
    memset(d,0,sizeof(*d)); strcpy(d->cFileName,names[pos]);
    d->dwFileAttributes=pos==1?2:pos==3?16:32;
    d->nFileSizeLow=pos==0?3:pos==2?6:1;
}
static HANDLE FindFirstFileA(const char *p,WIN32_FIND_DATAA *d) {
    struct MockFind *f;
    if (!strcmp(p,"MISSING")) { last=2; return INVALID_HANDLE_VALUE; }
    f=calloc(1,sizeof(*f)); assert(f);
    f->exact=!strcmp(p,"ALPHA.TXT");mock_data(0,d);return f;
}
static int FindNextFileA(HANDLE h,WIN32_FIND_DATAA *d) {
    struct MockFind *f=h;
    if(f->exact || ++f->pos==4) {last=18;return 0;}
    mock_data(f->pos,d);return 1;
}
static int FindClose(HANDLE h) {free(h);return 1;}
#include "../v2_find.h"
static uint32_t first(struct Runtime *r,uint32_t id,uint32_t attr,uint32_t cb,uint32_t n,uint32_t level) {
    uint32_t args[]={1000,2000,attr,3000,cb,2004,level};unsigned i;
    guest_put_u32(r,2000,id);guest_put_u32(r,2004,n);
    for(i=0;i<7;++i)guest_put_u32(r,104+4*i,args[i]);
    return dispatch_find(r,264,100);
}
static uint32_t next(struct Runtime *r,uint32_t h,uint32_t cb,uint32_t n) {
    uint32_t args[]={h,3000,cb,2004};unsigned i;
    guest_put_u32(r,2004,n);for(i=0;i<4;++i)guest_put_u32(r,104+4*i,args[i]);
    return dispatch_find(r,265,100);
}
static uint32_t closefind(struct Runtime *r,uint32_t h) {
    guest_put_u32(r,104,h);return dispatch_find(r,263,100);
}
int main(void) {
    struct Runtime r;uint32_t h,h2,old;int i;
    unsetenv("WHP_OS2_FIND_ZERO_COUNT");
    assert(fileio_tests()==0);
    memset(&r,0,sizeof(r));r.ram=calloc(1,RAM_SIZE);r.next_search_id=0x10000;
    strcpy((char*)r.ram+1000,"*");
    assert(first(&r,0xffffffffu,0,279,2,1)==0);h=guest_u32(&r,2000);
    assert(guest_u32(&r,2004)==1 && !strcmp((char*)r.ram+3023,"ALPHA.TXT"));
    assert(r.ram[3022]==9 && guest_u32(&r,3012)==3);
    assert(first(&r,0xffffffffu,2,279,1,1)==0);h2=guest_u32(&r,2000);assert(h2!=h);
    assert(next(&r,h,8,1)==111 && guest_u32(&r,2004)==0);
    assert(next(&r,h,279,1)==0 && !strcmp((char*)r.ram+3023,"BETA.TXT"));
    assert(next(&r,h,279,1)==18 && guest_u32(&r,2004)==0);
    assert(next(&r,h,279,1)==18);
    assert(next(&r,h2,279,1)==0 && !strcmp((char*)r.ram+3023,"HIDDEN.TXT"));
    assert(closefind(&r,h)==0 && closefind(&r,h)==6);old=h;
    assert(next(&r,old,279,1)==6);
    assert(closefind(&r,h2)==0);
    assert(first(&r,1,0,279,1,1)==0 && guest_u32(&r,2000)==1);
    assert(first(&r,1,0x1000,279,1,1)==0 && !strcmp((char*)r.ram+3023,"SUBDIR"));
    assert(closefind(&r,1)==0);
    assert(first(&r,0xffffffffu,0,279,1,99)==87);
    assert(first(&r,0xffffffffu,0,279,0,1)==87);
    assert(setenv("WHP_OS2_FIND_ZERO_COUNT","1",1)==0);
    assert(first(&r,1,7,284,0,1)==0 && guest_u32(&r,2004)==1);
    assert(!strcmp((char*)r.ram+3023,"ALPHA.TXT"));
    assert(next(&r,1,279,0)==87); /* compatibility only affects FindFirst */
    assert(closefind(&r,1)==0);
    assert(first(&r,1,7,8,0,1)==111);
    assert(first(&r,1,7,284,0,99)==87);
    strcpy((char*)r.ram+1000,"MISSING");
    assert(first(&r,1,7,284,0,1)==18 && guest_u32(&r,2004)==0);
    unsetenv("WHP_OS2_FIND_ZERO_COUNT");
    assert(first(&r,1,7,284,0,1)==87);
    strcpy((char*)r.ram+1000,"MISSING");assert(first(&r,0xffffffffu,0,279,1,1)==18);
    strcpy((char*)r.ram+1000,"*");
    for(i=0;i<64;++i){assert(first(&r,0xffffffffu,0,279,1,1)==0);h=guest_u32(&r,2000);assert(h!=old);assert(closefind(&r,h)==0);}
    for(i=0;i<32;++i)assert(first(&r,0xffffffffu,0,279,1,1)==0);
    assert(first(&r,0xffffffffu,0,279,1,1)==4);
    for(i=0;i<32;++i)assert(closefind(&r,r.searches[i].id)==0);
    assert(setenv("WHP_OS2_FIND_LAYOUT","GA",1)==0);
    assert(first(&r,0xffffffffu,0,284,1,1)==111);
    memset(r.ram+3000,0xa5,292);
    assert(first(&r,0xffffffffu,0,292,1,1)==0);h=guest_u32(&r,2000);
    assert(guest_u32(&r,3000)==0 && guest_u32(&r,3016)==3);
    assert(r.ram[3028]==9&&!strcmp((char*)r.ram+3029,"ALPHA.TXT")&&r.ram[3285]==0xa5);
    assert(next(&r,h,292,1)==0&&!strcmp((char*)r.ram+3029,"BETA.TXT"));
    assert(closefind(&r,h)==0);unsetenv("WHP_OS2_FIND_LAYOUT");
    assert(first(&r,0xffffffffu,0,279,1,1)==0&&!strcmp((char*)r.ram+3023,"ALPHA.TXT"));
    assert(closefind(&r,guest_u32(&r,2000))==0);
    free(r.ram);puts("directory dispatcher tests PASS (mock Win32 enumeration)");return 0;
}
