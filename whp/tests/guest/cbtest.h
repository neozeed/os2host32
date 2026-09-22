#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#include <os2.h>
#include <stdio.h>
typedef ULONG (_cdecl *CALLBACK2)(ULONG,ULONG);
/* Synthetic host-test API, explicitly imported through .DEF; not an OS/2 API. */
extern ULONG _cdecl HostInvoke(CALLBACK2 fn,ULONG a,ULONG b,ULONG *result);
static void fail(ULONG line)
{ printf("%s FAIL line %lu\n",TEST_NAME,line);DosExit(EXIT_PROCESS,1); }
#define CHECK(x) do { if(!(x)) fail((ULONG)__LINE__); } while(0)
static unsigned char fs_code[]={0x8c,0xe0,0x8e,0xe0,0x64,0xa1,0x0c,0,0,0,0xc3};
static ULONG fs_tib2(void)
{
    union { unsigned char *data;ULONG (_cdecl *call)(void); } u;
    u.data=fs_code;return u.call();
}
