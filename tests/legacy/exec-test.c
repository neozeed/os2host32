/*
 * exec-test.c
 *
 * Small OS/2 2.x / Microsoft C/386 test for DosExecPgm.
 *
 * Intended test:
 *   exec-test.exe
 *
 * It synchronously starts:
 *   args.exe one two three
 *
 * The argument buffer is deliberately in the native OS/2 form:
 *   "args.exe\0one two three\0\0"
 *
 * ANSI C89-oriented.
 */

#define INCL_DOSPROCESS
#include <os2.h>
#include <stdio.h>

int main(void)
{
    CHAR fail_name[260];
    RESULTCODES results;
    APIRET rc;

    /*
     * A C string literal already receives its final terminating NUL,
     * so this initializer becomes:
     *
     *   args.exe\0one two three\0\0
     */
    static CHAR arg_block[] = "args.exe\0one two three\0";

    fail_name[0] = '\0';
    results.codeTerminate = 0;
    results.codeResult = 0;

    printf("exec-test: calling DosExecPgm\n");
    printf("exec-test: child = args.exe one two three\n");

    rc = DosExecPgm(fail_name,
                    (LONG)sizeof(fail_name),
                    EXEC_SYNC,
                    arg_block,
                    (PSZ)0,
                    &results,
                    "args.exe");

    printf("exec-test: DosExecPgm rc = %lu\n",
           (unsigned long)rc);

    if (rc != 0) {
        printf("exec-test: failing object = [%s]\n", fail_name);
        return 1;
    }

    printf("exec-test: child termination code = %lu\n",
           (unsigned long)results.codeTerminate);
    printf("exec-test: child result code      = %lu\n",
           (unsigned long)results.codeResult);

    return 0;
}
