#include <assert.h>
#include <stdio.h>
typedef unsigned short O2USHORT;
typedef unsigned long O2ULONG;
typedef unsigned int UINT;
#define MB_OK 0
#define MB_OKCANCEL 1
#define MB_ABORTRETRYIGNORE 2
#define MB_YESNOCANCEL 3
#define MB_YESNO 4
#define MB_RETRYCANCEL 5
#define MB_ICONHAND 0x10
#define MB_ICONQUESTION 0x20
#define MB_ICONEXCLAMATION 0x30
#define MB_ICONASTERISK 0x40
#define MB_DEFBUTTON2 0x100
#define MB_DEFBUTTON3 0x200
#define MB_SYSTEMMODAL 0x1000
#define IDOK 1
#define IDNO 7
static int answer, calls;
static UINT seen;
static int MessageBoxA(void *owner,const char *text,const char *caption,UINT flags)
{ (void)owner;(void)text;(void)caption;seen=flags;++calls;return answer; }
static O2USHORT map_box(O2ULONG style)
{
    void *wh=0;
    const char *text="test", *caption="test";
    UINT flags;
    int rc;
    /* OS/2 and Win32 use different button/icon encodings. In particular
     * OS/2 YESNO=4 must return MBID_YES=6, not DID_OK=1. */
    switch (style & 0x000fUL) {
    case 0: flags = MB_OK; break;
    case 1: flags = MB_OKCANCEL; break;
    case 2: flags = MB_RETRYCANCEL; break;
    case 3: flags = MB_ABORTRETRYIGNORE; break;
    case 4: flags = MB_YESNO; break;
    case 5: flags = MB_YESNOCANCEL; break;
    default:
        /* Do not turn an unsupported choice into an affirmative result. */
        return (O2USHORT)0xffffU;
    }
    switch (style & 0x00f0UL) {
    case 0x10: flags |= MB_ICONQUESTION; break;
    case 0x20: flags |= MB_ICONEXCLAMATION; break;
    case 0x30: flags |= MB_ICONASTERISK; break;
    case 0x40: flags |= MB_ICONHAND; break;
    }
    switch (style & 0x0300UL) {
    case 0x0100: flags |= MB_DEFBUTTON2; break;
    case 0x0200: flags |= MB_DEFBUTTON3; break;
    }
    if (style & 0x1000UL) flags |= MB_SYSTEMMODAL;
    rc = MessageBoxA(wh, text ? text : "", caption ? caption : "", flags);
    /* Standard OK/Cancel/Abort/Retry/Ignore/Yes/No IDs agree (1..7). */
    if (rc >= IDOK && rc <= IDNO) return (O2USHORT)rc;
    return (O2USHORT)0xffffU;
}
int main(void)
{
    int i;
    unsigned int buttons[6] = {0,1,5,2,4,3};
    for(i=0;i<6;++i){answer=1;assert(map_box((O2ULONG)i)==1);assert(seen==buttons[i]);}
    answer=6;assert(map_box(0x4014)==6 && seen==(MB_YESNO|MB_ICONQUESTION));
    answer=7;assert(map_box(4)==7);
    answer=2;assert(map_box(5)==2);
    answer=1;assert(map_box(0x1231)==1 && seen==(MB_OKCANCEL|MB_ICONASTERISK|MB_DEFBUTTON3|MB_SYSTEMMODAL));
    answer=0;assert(map_box(4)==0xffffU);
    calls=0;assert(map_box(6)==0xffffU && calls==0);
    puts("PASS message-box mapping: six button sets, Yes/No/Cancel, icons, defaults, errors");
    return 0;
}
