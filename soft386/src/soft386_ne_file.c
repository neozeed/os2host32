/* NE-specific 16-bit FILESTATUS marshalling.  Never cast guest bytes to a
 * native struct: the 16-bit and 32-bit OS/2 layouts are different sizes. */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "soft386_ne_file.h"
#include <errno.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <stdlib.h>
#else
#include <unistd.h>
#if defined(__linux__) || defined(__APPLE__)
#include <sys/xattr.h>
#endif
#endif

static uint16_t u16(const uint8_t *p) {return (uint16_t)(p[0]|(uint16_t)p[1]<<8);}
static uint32_t u32(const uint8_t *p) {return (uint32_t)u16(p)|((uint32_t)u16(p+2)<<16);}
#ifdef _WIN32
static void put16(uint8_t *p,uint16_t x) {p[0]=(uint8_t)x;p[1]=(uint8_t)(x>>8);}
static void put32(uint8_t *p,uint32_t x) {put16(p,(uint16_t)x);put16(p+2,(uint16_t)(x>>16));}
#endif
static uint32_t file_error(void)
{
    switch(errno){case EBADF:return 6u;case ENOENT:return 2u;
    case EACCES:case EPERM:return 5u;case EINVAL:return 87u;
    case ENOSPC:return 112u;default:return 5u;}
}
/* OS/2 FDATE/FTIME are two 16-bit DOS packed words, interpreted in the
 * local timezone, with 00:00 (both zero) meaning "leave unchanged". */
static int decode_time(uint16_t date,uint16_t clock,struct tm *tm)
{
    int year=1980+((date>>9)&127), month=(date>>5)&15, day=date&31;
    int hour=(clock>>11)&31,minute=(clock>>5)&63,second=(clock&31)*2;
    int mdays[]={31,28,31,30,31,30,31,31,30,31,30,31};
    if(month<1||month>12||hour>23||minute>59||second>59)return 0;
    if((year%4==0&&(year%100!=0||year%400==0)))mdays[1]=29;
    if(day<1||day>mdays[month-1])return 0;
    memset(tm,0,sizeof(*tm));tm->tm_year=year-1900;tm->tm_mon=month-1;
    tm->tm_mday=day;tm->tm_hour=hour;tm->tm_min=minute;tm->tm_sec=second;
    tm->tm_isdst=-1;return 1;
}
#ifdef _WIN32
static uint32_t win_error(DWORD e)
{
    switch(e){case ERROR_INVALID_HANDLE:return 6u;
    case ERROR_FILE_NOT_FOUND:return 2u;case ERROR_PATH_NOT_FOUND:return 3u;
    case ERROR_ACCESS_DENIED:case ERROR_SHARING_VIOLATION:return 5u;
    case ERROR_INVALID_PARAMETER:return 87u;default:return e?e:5u;}
}
static int win_filetime(uint16_t d,uint16_t t,FILETIME *out)
{
    struct tm tm;SYSTEMTIME local;FILETIME ft;
    if(!decode_time(d,t,&tm))return 0;
    memset(&local,0,sizeof(local));
    local.wYear=(WORD)(tm.tm_year+1900);local.wMonth=(WORD)(tm.tm_mon+1);
    local.wDay=(WORD)tm.tm_mday;local.wHour=(WORD)tm.tm_hour;
    local.wMinute=(WORD)tm.tm_min;local.wSecond=(WORD)tm.tm_sec;
    if(!SystemTimeToFileTime(&local,&ft))return 0;
    return LocalFileTimeToFileTime(&ft,out)!=0;
}
#endif
uint32_t soft386_ne_set_file_info(int fd,uint16_t level,
                                  const uint8_t *p,uint16_t cb)
{
    uint16_t cd,ct,ad,at,wd,wt,attr;
    struct tm tmp;
    if(fd<0)return 6u;
    if(level!=1u)return 124u; /* ERROR_INVALID_LEVEL */
    if(!p||cb<22u)return 122u; /* ERROR_INSUFFICIENT_BUFFER */
    cd=u16(p);ct=u16(p+2);ad=u16(p+4);at=u16(p+6);
    wd=u16(p+8);wt=u16(p+10);attr=u16(p+20);
    if((attr&~0x37u)!=0u)return 87u;
    if((cd||ct)&&!decode_time(cd,ct,&tmp))return 87u;
    if((ad||at)&&!decode_time(ad,at,&tmp))return 87u;
    if((wd||wt)&&!decode_time(wd,wt,&tmp))return 87u;
#ifdef _WIN32
    {
        intptr_t native=_get_osfhandle(fd);HANDLE h;
        FILETIME creation,access,write;
        DWORD attrs,want,n;
        char raw[2048],path[2048];
        HMODULE kernel;
        typedef DWORD (WINAPI *PFN_FINALPATH)(HANDLE,LPSTR,DWORD,DWORD);
        PFN_FINALPATH finalpath;
        if(native==-1)return 6u;h=(HANDLE)native;
        if((cd||ct)&&!win_filetime(cd,ct,&creation))return 87u;
        if((ad||at)&&!win_filetime(ad,at,&access))return 87u;
        if((wd||wt)&&!win_filetime(wd,wt,&write))return 87u;
        if((cd||ct)||(ad||at)||(wd||wt))
            if(!SetFileTime(h,(cd||ct)?&creation:NULL,(ad||at)?&access:NULL,
                            (wd||wt)?&write:NULL))return win_error(GetLastError());
        /* Attribute changes are pathname-only on Windows, so resolve the
         * actual open handle, not a potentially renamed guest path. */
        kernel=GetModuleHandleA("kernel32.dll");
        finalpath=kernel?(PFN_FINALPATH)GetProcAddress(kernel,"GetFinalPathNameByHandleA"):NULL;
        if(!finalpath)return 1u;
        n=finalpath(h,raw,sizeof(raw),0);
        if(!n||n>=sizeof(raw))return n?206u:win_error(GetLastError());
        if(strncmp(raw,"\\\\?\\UNC\\",8)==0){
            path[0]='\\';path[1]='\\';strncpy(path+2,raw+8,sizeof(path)-3);
            path[sizeof(path)-1]=0;
        }else if(strncmp(raw,"\\\\?\\",4)==0){
            strncpy(path,raw+4,sizeof(path)-1);path[sizeof(path)-1]=0;
        }else{strncpy(path,raw,sizeof(path)-1);path[sizeof(path)-1]=0;}
        attrs=GetFileAttributesA(path);
        if(attrs==INVALID_FILE_ATTRIBUTES)return win_error(GetLastError());
        want=attrs&~(FILE_ATTRIBUTE_NORMAL|FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_HIDDEN|
                      FILE_ATTRIBUTE_SYSTEM|FILE_ATTRIBUTE_ARCHIVE);
        if(attr&1u)want|=FILE_ATTRIBUTE_READONLY;
        if(attr&2u)want|=FILE_ATTRIBUTE_HIDDEN;
        if(attr&4u)want|=FILE_ATTRIBUTE_SYSTEM;
        if(attr&0x20u)want|=FILE_ATTRIBUTE_ARCHIVE;
        if(!want)want=FILE_ATTRIBUTE_NORMAL;
        if(want!=attrs&&!SetFileAttributesA(path,want))return win_error(GetLastError());
    }
#else
    {
        struct stat sb;
        struct timespec stamp[2];
        time_t x;
        mode_t mode;
        if(fstat(fd,&sb)<0)return file_error();
        if(!S_ISREG(sb.st_mode))return 6u;
        if((ad||at)||(wd||wt)){
            stamp[0].tv_sec=sb.st_atime;stamp[0].tv_nsec=0;
            stamp[1].tv_sec=sb.st_mtime;stamp[1].tv_nsec=0;
            if(ad||at){decode_time(ad,at,&tmp);x=mktime(&tmp);
                if(x==(time_t)-1)return 87u;
                stamp[0].tv_sec=x;}
            if(wd||wt){decode_time(wd,wt,&tmp);x=mktime(&tmp);
                if(x==(time_t)-1)return 87u;
                stamp[1].tv_sec=x;}
            if(futimens(fd,stamp)<0)return file_error();
        }
        mode=sb.st_mode;
        if(attr&1u)mode&=~(S_IWUSR|S_IWGRP|S_IWOTH);
        else mode|=S_IWUSR;
        if((mode&0777u)!=(sb.st_mode&0777u)&&fchmod(fd,mode)<0)return file_error();
        /* Hidden/system/archive are Windows-specific.  Preserve semantics
         * on Win32; POSIX diagnostic host cannot represent them. */
    }
#endif
    return 0u;
}

/* OS/2 1.x FEALIST format, not OS/2 2.x FEA2LIST:
 * DWORD total, followed by [BYTE flags, BYTE namelen, WORD valuelen,
 * name[namelen], NUL, value[valuelen]].  The records themselves are packed.
 * The Microsoft RC 2.01 trace exposes a single .TYPE FEA occupying bytes
 * 4..29 while cbList is 32; accept ONLY a 1..3 byte, zero-filled trailing
 * DWORD padding area.  Do not silently skip gaps between FEA entries. */
static uint32_t check_fealist(const uint8_t *p,uint32_t n,uint32_t *bad)
{
    uint32_t at=4u;
    *bad=0u;
    if(!p||n<4u||n>65535u||u32(p)!=n)return 255u;
    while(at<n){
        uint32_t off=at,namelen,vallen,i;
        if(at>4u && n-at<=3u && (n&3u)==0u){
            for(i=at;i<n;i++)if(p[i]!=0u)break;
            if(i==n)return 0u;
        }
        if(n-at<4u){*bad=off;return 255u;}
        namelen=p[at+1u];vallen=u16(p+at+2u);
        if(p[at]&0x7fu){*bad=off;return 255u;}
        at+=4u;
        if(!namelen){*bad=off;return 254u;}
        if(n-at<namelen+1u||n-at-namelen-1u<vallen){
            *bad=off;return 255u;
        }
        if(p[at+namelen]!=0u){*bad=off;return 254u;}
        for(i=0;i<namelen;i++){
            if(p[at+i]<0x21u||p[at+i]>0x7eu||p[at+i]=='/'||
               p[at+i]=='\\'||p[at+i]=='='){
                *bad=off;return 254u;
            }
        }
        at+=namelen+1u+vallen;
    }
    return 0u;
}

uint32_t soft386_ne_set_file_eas(int fd,const uint8_t *list,
                                uint32_t list_size,uint32_t *error_offset)
{
    uint32_t rc,at,bad=0;
    if(error_offset)*error_offset=0u;
    if(fd<0)return 6u;
    rc=check_fealist(list,list_size,&bad);
    if(rc){if(error_offset)*error_offset=bad;return rc;}
    /* An empty FEA list changes nothing and is safe on FAT as well as NTFS. */
    if(list_size==4u)return 0u;
#ifdef _WIN32
    {
        /* NT's native FILE_FULL_EA_INFORMATION has DWORD NextEntryOffset,
         * BYTE Flags, BYTE EaNameLength, WORD EaValueLength, then name/NUL/value.
         * Its entries are DWORD aligned; OS/2 1.x FEAs are not. */
        typedef struct {LONG status;ULONG_PTR information;} NE_IOSB;
        typedef LONG (NTAPI *PFN_SET_EA)(HANDLE,NE_IOSB *,PVOID,ULONG);
        typedef ULONG (NTAPI *PFN_STATUS_ERROR)(LONG);
        HMODULE ntdll=GetModuleHandleA("ntdll.dll");
        PFN_SET_EA native_set=ntdll?(PFN_SET_EA)GetProcAddress(ntdll,"NtSetEaFile"):NULL;
        PFN_STATUS_ERROR status_error=ntdll?(PFN_STATUS_ERROR)GetProcAddress(ntdll,"RtlNtStatusToDosError"):NULL;
        intptr_t h=_get_osfhandle(fd);
        uint8_t *native,*prev=NULL;
        uint32_t out=0u,used=0u,cap;
        NE_IOSB iosb;
        LONG status;
        if(h==-1)return 6u;
        if(!native_set)return 282u; /* ERROR_EAS_NOT_SUPPORTED */
        cap=list_size*2u+16u;
        native=(uint8_t *)calloc(1,cap);
        if(!native)return 8u;
        for(at=4u;at<list_size;){
            uint32_t off=at,namelen=list[at+1u],vallen=u16(list+at+2u);
            uint32_t span=8u+namelen+1u+vallen,aligned=(span+3u)&~3u;
            uint8_t *q;
            if(out+aligned>cap){free(native);return 255u;}
            q=native+out;
            if(prev)put32(prev,(uint32_t)(q-prev));
            prev=q;put32(q,0u);q[4]=list[at];q[5]=(uint8_t)namelen;
            put16(q+6,(uint16_t)vallen);
            memcpy(q+8,list+at+4u,namelen+1u+vallen);
            used=out+span;
            out+=aligned;
            at=off+4u+namelen+1u+vallen;
        }
        memset(&iosb,0,sizeof(iosb));
        /* Last entry has NextEntryOffset=0 and its unpadded byte length. */
        status=native_set((HANDLE)h,&iosb,native,(ULONG)used);
        free(native);
        if(status<0){
            if((uint32_t)status==0xc000004fu)return 282u;
            if(status_error){ULONG err=status_error(status);return err?err:5u;}
            return 5u;
        }
    }
#elif defined(__linux__) || defined(__APPLE__)
    for(at=4u;at<list_size;){
        uint32_t off=at,namelen=list[at+1u],vallen=u16(list+at+2u);
        char name[266];int ok;
        memcpy(name,"user.os2.",9);
        memcpy(name+9,list+at+4u,namelen);
        name[9u+namelen]=0;
#ifdef __APPLE__
        ok=fsetxattr(fd,name,list+at+5u+namelen,vallen,0,0);
#else
        ok=fsetxattr(fd,name,list+at+5u+namelen,vallen,0);
#endif
        if(ok<0){
            if(error_offset)*error_offset=off;
            if(errno==ENOTSUP)return 282u;
            return file_error();
        }
        at=off+4u+namelen+1u+vallen;
    }
#else
    (void)at;return 282u;
#endif
    return 0u;
}
