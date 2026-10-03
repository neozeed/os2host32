#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "os2_msg.h"
static HANDLE message_file(const char *path)
{
    HANDLE h;char *dpath;char found[32768];DWORD n,len,error;
    h=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(h!=INVALID_HANDLE_VALUE) return h;
    error=GetLastError();
    if((error!=ERROR_FILE_NOT_FOUND && error!=ERROR_PATH_NOT_FOUND) || strchr(path,'\\') || strchr(path,'/') || strchr(path,':')) return h;
    n=GetEnvironmentVariableA("DPATH",NULL,0);
    if(!n || n>32768) { SetLastError(error);return h; }
    dpath=(char *)HeapAlloc(GetProcessHeap(),0,n);
    if(!dpath) { SetLastError(ERROR_NOT_ENOUGH_MEMORY);return h; }
    len=GetEnvironmentVariableA("DPATH",dpath,n);
    if(len && len<n) {
        len=SearchPathA(dpath,path,NULL,sizeof(found),found,NULL);
        if(len && len<sizeof(found)) {
            h=CreateFileA(found,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
            if(h==INVALID_HANDLE_VALUE) error=GetLastError();
        }
    }
    HeapFree(GetProcessHeap(),0,dpath);if(h==INVALID_HANDLE_VALUE) SetLastError(error);return h;
}
uint32_t __cdecl DosTrueGetMessage(const void *segment,const char *const *table,uint32_t count,
    char *dst,uint32_t cb,uint32_t id,const char *file,uint32_t *actual)
{
    HANDLE h;DWORD size,high,got;unsigned char *data;uint32_t rc;
    if(!actual || (!dst && cb) || !file || !*file) return 87;
    *actual=0;if(count>9) return 320;
    if(segment) {
        unsigned char header[18],empty[4];SIZE_T n;uint32_t off;
        if(!ReadProcessMemory(GetCurrentProcess(),segment,header,sizeof(header),&n) || n!=sizeof(header) ||
           memcmp(header,"\xffMSGSEG32\0",10)) return 319;
        memcpy(&off,header+14,4);
        /* The shipped TELNETPM has an empty bound-message table. External
         * file fallback is implemented; nonempty MSGBIND tables are deferred. */
        if(off>0x1000000UL || !ReadProcessMemory(GetCurrentProcess(),(const unsigned char *)segment+off,empty,4,&n) || n!=4) return 319;
        if(memcmp(empty,"\0\0\xff\xff",4)) return 50;
    }
    h=message_file(file);
    if(h==INVALID_HANDLE_VALUE) {
        DWORD e=GetLastError();
        if(e==ERROR_FILE_NOT_FOUND || e==ERROR_PATH_NOT_FOUND) return 2;
        return e==ERROR_NOT_ENOUGH_MEMORY?8:318;
    }
    size=GetFileSize(h,&high);
    if(high || size==INVALID_FILE_SIZE || size>32UL*1024*1024) { CloseHandle(h);return 319; }
    data=(unsigned char *)HeapAlloc(GetProcessHeap(),0,size?size:1);
    if(!data) { CloseHandle(h);return 8; }
    if(!ReadFile(h,data,size,&got,NULL) || got!=size) rc=318;
    else rc=os2_msg_from_file(data,size,id,table,count,dst,cb,actual);
    HeapFree(GetProcessHeap(),0,data);CloseHandle(h);return rc;
}
uint32_t __cdecl DosInsertMessage(const char *const *table,uint32_t count,const char *src,uint32_t len,char *dst,uint32_t cb,uint32_t *actual)
{ return os2_msg_insert(table,count,src,len,dst,cb,actual); }
