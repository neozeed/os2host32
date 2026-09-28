#include <stdio.h>

void OSinit(void)
{
	srand( (unsigned)time(NULL));	//seed the random number generator
}
void OSdinit(void){}