/*
 * rc-child.c - M29K child-result fixture.
 *
 * Returns the decimal result code supplied as argv[1].  It deliberately does
 * almost nothing else so synchronous DosExecPgm result propagation can be
 * tested without another subsystem getting in the way.
 */
#include <stdlib.h>

int main(int argc, char **argv)
{
    int rc;

    rc = 0;
    if (argc > 1)
        rc = atoi(argv[1]);
    if (rc < 0)
        rc = 0;
    if (rc > 255)
        rc &= 255;
    return rc;
}
