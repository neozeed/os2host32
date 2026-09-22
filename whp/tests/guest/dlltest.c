/*
 * dlltest.c - first WHP V2 user-DLL regression.
 *
 * The .DEF deliberately imports one symbol by name and two by ordinal.
 */
#include <stdio.h>

extern unsigned long V2DllAdd(unsigned long, unsigned long);
extern unsigned long V2DllNext(void);
extern unsigned long V2DllSay(char *, unsigned long);

int main(void)
{
    unsigned long a;
    unsigned long n1;
    unsigned long n2;
    static char msg[] = "Hello from guest V2DLL.DLL!\r\n";

    a = V2DllAdd(20UL, 22UL);
    if (a != 42UL) {
        puts("dlltest: V2DllAdd FAILED");
        return 40;
    }

    n1 = V2DllNext();
    n2 = V2DllNext();
    if (n1 != 1UL || n2 != 2UL) {
        puts("dlltest: DLL data FAILED");
        return 41;
    }

    if (V2DllSay(msg, sizeof(msg) - 1UL) != 0) {
        puts("dlltest: DLL DosWrite FAILED");
        return 42;
    }

    puts("dlltest PASS");
    return 0;
}
