#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include "os2_doscalls_backend.h"
static O2APIRET delete_file(void *o,const char *path)
{ (void)o;return DeleteFileA(path)?0:(O2APIRET)GetLastError(); }
static O2APIRET copy_file(void *o,const char *from,const char *to,int replace)
{ (void)o;return CopyFileA(from,to,!replace)?0:(O2APIRET)GetLastError(); }
static O2APIRET flush_file(void *o,O2NATIVE n,int all)
{
    HANDLE h=(HANDLE)(uintptr_t)n;DWORD type=GetFileType(h);(void)o;
    if(type==FILE_TYPE_CHAR || (all && type==FILE_TYPE_PIPE)) return 0;
    return FlushFileBuffers(h)?0:(O2APIRET)GetLastError();
}
static O2APIRET inherit_file(void *o,O2NATIVE n,int inherit)
{ (void)o;return SetHandleInformation((HANDLE)(uintptr_t)n,HANDLE_FLAG_INHERIT,inherit?HANDLE_FLAG_INHERIT:0)?0:(O2APIRET)GetLastError(); }
static O2APIRET disk_info(void *o,O2ULONG disk,struct O2DosDiskInfo *d)
{
    char root[4]="C:\\",cwd[MAX_PATH];DWORD spc,bps,freec,total,serial,n; (void)o;
    if(!disk) {
        n=GetCurrentDirectoryA(sizeof(cwd),cwd);
        if(!n) return (O2APIRET)GetLastError();
        if(n>=sizeof(cwd)) return 206;
        if(cwd[1]!=':') return 15;
        root[0]=cwd[0];
    } else root[0]=(char)('A'+disk-1);
    if(!GetDiskFreeSpaceA(root,&spc,&bps,&freec,&total)) return (O2APIRET)GetLastError();
    if(!GetVolumeInformationA(root,d->label,sizeof(d->label),&serial,NULL,NULL,NULL,0)) return (O2APIRET)GetLastError();
    d->sectors_per_unit=spc;d->bytes_per_sector=bps;d->free_units=freec;d->total_units=total;d->serial=serial;
    return 0;
}
static O2APIRET counter(void *o,int freq,uint64_t *v)
{
    LARGE_INTEGER q;(void)o;
    if(!(freq?QueryPerformanceFrequency(&q):QueryPerformanceCounter(&q))) return 50;
    *v=(uint64_t)q.QuadPart;return 0;
}
static O2APIRET resource_size(void *o,O2ULONG module,O2ULONG type,O2ULONG id,O2ULONG *size)
{
    typedef const void *(__cdecl *Query)(O2ULONG,unsigned short,unsigned short,O2ULONG *);
    HMODULE pm;FARPROC proc;Query query;(void)o;
    pm=GetModuleHandleA("PMWIN.dll");proc=pm?GetProcAddress(pm,"OS2PM_QueryResource"):NULL;
    if(!proc) return 2;
    memcpy(&query,&proc,sizeof(query));
    return query(module,(unsigned short)type,(unsigned short)id,size)?0:2;
}
static const struct Os2DosPlatformOps ops={delete_file,copy_file,flush_file,inherit_file,disk_info,counter,resource_size};
const struct Os2DosPlatformOps *os2_dos_platform_win32(void) { return &ops; }
