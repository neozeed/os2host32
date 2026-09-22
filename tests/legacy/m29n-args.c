/*
 * m29n-args.c - C/386 argv probe for M29N command lookup/quoting.
 *
 * The point is to observe what the original C/386 startup code receives after
 * CMD32OS2 resolves a command name through PATH and DosExecPgm transports the
 * native OS/2 program-name\0argument-tail block through OS2HOST32.
 */
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    int i;

    puts("M29N_ARGS_OK");
    printf("ARGC=%d\n", argc);
    for (i = 0; i < argc; ++i) {
        printf("ARGV%d_LEN=%u ARGV%d=[%s]\n",
               i, (unsigned)strlen(argv[i]), i, argv[i]);
    }
    return 0;
}
