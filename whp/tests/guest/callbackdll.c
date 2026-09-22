#define TEST_NAME "callbackdll"
#include "cbtest.h"
extern ULONG _cdecl CbDllAddress(void);
extern ULONG _cdecl CbDllCount(void);
static ULONG caller_tib;
static ULONG _cdecl from_dll(ULONG a,ULONG b)
{
    CHECK(fs_tib2()==caller_tib);
    CHECK(DosSleep(1)==0);
    return a+b;
}
int main(void)
{
    ULONG result,i;
    union { ULONG address; CALLBACK2 function; } target,back;
    target.address=CbDllAddress();
    back.function=from_dll;
    caller_tib=fs_tib2();
    for(i=0;i<10;i++) {
        CHECK(HostInvoke(target.function,back.address,i,&result)==0);
        CHECK(result==i+107 && fs_tib2()==caller_tib);
    }
    CHECK(CbDllCount()==10);
    printf("callbackdll PASS\n");return 0;
}
