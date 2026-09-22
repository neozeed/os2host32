/* CRT-free guest DLL: a host-invoked DLL function calls DOSCALLS and then
 * requests a nested host-to-guest callback into executable code. */
typedef unsigned long ULONG;
typedef ULONG (*CALLBACK2)(ULONG,ULONG);
extern ULONG HostInvoke(CALLBACK2,ULONG,ULONG,ULONG *);
extern ULONG DosSleep(ULONG);
static ULONG count;
ULONG CbDllInvoke(ULONG fn,ULONG arg)
{
    ULONG result;
    union { ULONG address; CALLBACK2 function; } target;
    target.address=fn;
    ++count;
    if(DosSleep(0)!=0) return 0xbad00001UL;
    if(HostInvoke(target.function,arg,7,&result)!=0) return 0xbad00002UL;
    return result+100;
}
ULONG CbDllCount(void) { return count; }

ULONG CbDllAddress(void)
{
    union { ULONG address; CALLBACK2 function; } target;
    target.function=CbDllInvoke;return target.address;
}
