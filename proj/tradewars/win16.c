#include <io.h>
void OSinit(void)
{
struct _wsizeinfo ws;
int rc;

memset(&ws,0x0,sizeof(ws));
rc=_wgetsize(1,_WINCURRREQ,&ws);
ws._h=24;
ws._w=80;
ws._type=_WINSIZEMAX;
_wsetsize(1,&ws);
_wsetexit(2);
_wabout("TradewarsC 0.7a 2009");
srand( (unsigned)time(NULL));	//seed the random number generator
}

void OSdinit(void)
{
printf("\nPress enter to exit!\n");
getchar();
}
