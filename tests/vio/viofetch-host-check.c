#include "viofetch_api.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static char screen[25][81];
static int query_failure, mode_failure, clear_failure, output_failure, cursor_failure;
static unsigned query_calls;
int viofetch_main(void);
USHORT VioGetMode(VIOMODEPREFIX *mode, USHORT h)
{ assert(!h && mode->cb==12);mode->col=80;mode->row=25;return (USHORT)mode_failure; }
USHORT VioScrollUp(USHORT t, USHORT l, USHORT b, USHORT r, USHORT n, char *cell, USHORT h)
{
    unsigned i;
    assert(!t && !l && b==65535 && r==65535 && n==65535 && !h && cell[0]==' ' && cell[1]==7);
    for(i=0;i<25;i++){memset(screen[i],' ',80);screen[i][80]=0;}
    return (USHORT)clear_failure;
}
USHORT VioWrtCharStrAtt(char *text,USHORT n,USHORT r,USHORT c,BYTE *a,USHORT h)
{ assert(r<25 && c+n<=80 && a && !h);memcpy(screen[r]+c,text,n);return (USHORT)output_failure; }
USHORT VioSetCurPos(USHORT r,USHORT c,USHORT h)
{ assert(r==14 && !c && !h);return (USHORT)cursor_failure; }
APIRET DosQuerySysInfo(ULONG first,ULONG last,void *out,ULONG cb)
{
    ULONG *v=(ULONG *)out;
    query_calls++;
    if(query_failure)return 50;
    if(first==11) {assert(last==12 && cb==8);v[0]=20;v[1]=0;}
    else if(first==14) {assert(last==14 && cb==4);v[0]=180000;}
    else {assert(first==19 && last==19 && cb==4);v[0]=536870912;}
    return 0;
}
APIRET DosQueryCp(ULONG cb,ULONG *cp,ULONG *actual)
{query_calls++;assert(cb==32);if(query_failure)return 50;cp[0]=437;cp[1]=437;cp[2]=850;*actual=12;return 0;}
APIRET DosQueryCtryInfo(ULONG cb,const COUNTRYCODE *cc,COUNTRYINFO *ci,ULONG *actual)
{query_calls++;assert(cb==44 && !cc->country && !cc->codepage);if(query_failure)return 50;ci->country=44;*actual=44;return 0;}
APIRET DosQueryCurrentDisk(ULONG *drive,ULONG *map)
{query_calls++;if(query_failure)return 50;*drive=3;*map=4;return 0;}
APIRET DosQueryFSInfo(ULONG drive,ULONG level,void *out,ULONG cb)
{
    FSALLOCATE *fs=(FSALLOCATE *)out;
    query_calls++;assert(!drive && level==1 && cb==18);if(query_failure)return 50;
    fs->cSectorUnit=8;fs->cbSector=512;fs->cUnit=0xf0000000u;fs->cUnitAvail=2097152;return 0;
}
int main(void)
{
    assert(sizeof(COUNTRYINFO)==44 && sizeof(FSALLOCATE)==18 && sizeof(VIOMODEPREFIX)==12);
    assert(!viofetch_main() && query_calls==7);
    assert(strstr(screen[3],"OS/2 2.00"));assert(strstr(screen[4],"3 min"));
    assert(strstr(screen[5],"512.00 MB"));assert(strstr(screen[7],"(437), 850"));
    assert(strstr(screen[8],"United Kingdom"));assert(strstr(screen[9],"8192 / 15728640"));
    assert(strstr(screen[10],"Drive C:"));
    query_failure=1;assert(!viofetch_main());
    assert(strstr(screen[3],"query rc=50"));assert(strstr(screen[5],"query rc=50"));
    assert(strstr(screen[9],"query rc=50"));query_failure=0;
    mode_failure=6;assert(viofetch_main()==1);mode_failure=0;
    clear_failure=87;assert(viofetch_main()==1);clear_failure=0;
    output_failure=1;assert(viofetch_main()==1);output_failure=0;
    cursor_failure=87;assert(viofetch_main()==1);
    puts("viofetch mixed API sizes, system fields, >4 GiB disk math and errors: PASS");
    return 0;
}
