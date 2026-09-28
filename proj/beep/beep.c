typedef unsigned long APIRET;

extern APIRET DosBeep(unsigned long frequency,
                      unsigned long duration);

int main(void)
{
    DosBeep(440, 500);
    DosBeep(660, 500);
    DosBeep(880, 1000);
    return 0;
}
