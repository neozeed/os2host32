/* M23b conditional-execution regression: failing child. */
#include <stdio.h>

int main(void)
{
    printf("status7: returning 7\n");
    return 7;
}
