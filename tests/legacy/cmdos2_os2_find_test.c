/*
 * cmdos2_os2_find_test.c - M29I direct OS/2 DosFindFirst/DosFindNext probe.
 *
 * Create two deterministic files, enumerate both through the stable cmdos2.h
 * boundary, validate names and sizes, then close and remove the fixtures.
 * This exercises DOSCALLS.264 and DOSCALLS.265 through the genuine C/386 LX
 * backend rather than testing only the native Win32 personality in isolation.
 */
#include <stdio.h>
#include "cmdos2.h"

static int same(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (*a != *b)
            return 0;
        ++a;
        ++b;
    }
    return *a == *b;
}

static CmdO2Rc make_file(const char *name, const char *data,
                         unsigned long cb)
{
    CmdO2Handle h;
    unsigned long actual;
    CmdO2Rc rc;

    h = CMDO2_HANDLE_ALLOCATE;
    rc = CmdO2OpenWriteReplace(name, &h);
    if (rc != 0)
        return rc;
    actual = 0;
    rc = CmdO2Write(h, data, cb, &actual);
    (void)CmdO2Close(h);
    if (rc != 0)
        return rc;
    if (actual != cb)
        return CMDO2_ERROR_INVALID_PARAMETER;
    return 0;
}

int main(void)
{
    static const char file_a[] = "m29i-find-a.tmp";
    static const char file_b[] = "m29i-find-b.tmp";
    static const char data_a[] = "A";
    static const char data_b[] = "BBBB";
    struct CmdO2FindData fd;
    CmdO2FindHandle h;
    CmdO2Rc rc;
    int saw_a;
    int saw_b;
    int entries;

    if (!CmdO2Init()) {
        printf("CmdO2Init failed: %s\r\n", CmdO2InitError());
        return 1;
    }

    (void)CmdO2Delete(file_a);
    (void)CmdO2Delete(file_b);

    rc = make_file(file_a, data_a, 1UL);
    if (rc != 0) {
        printf("create %s rc=%lu\r\n", file_a, rc);
        CmdO2Done();
        return 2;
    }
    rc = make_file(file_b, data_b, 4UL);
    if (rc != 0) {
        printf("create %s rc=%lu\r\n", file_b, rc);
        (void)CmdO2Delete(file_a);
        CmdO2Done();
        return 3;
    }

    h = CMDO2_FIND_CREATE;
    saw_a = 0;
    saw_b = 0;
    entries = 0;

    rc = CmdO2FindFirst("m29i-find-?.tmp", &h, &fd);
    while (rc == 0) {
        ++entries;
        printf("find[%d] name=[%s] size=%lu alloc=%lu attr=%lu\r\n",
               entries, fd.name, fd.size, fd.alloc, fd.attr);

        if (same(fd.name, file_a)) {
            if (fd.size != 1UL) {
                printf("wrong size for %s: %lu\r\n", file_a, fd.size);
                rc = CMDO2_ERROR_INVALID_PARAMETER;
                break;
            }
            ++saw_a;
        } else if (same(fd.name, file_b)) {
            if (fd.size != 4UL) {
                printf("wrong size for %s: %lu\r\n", file_b, fd.size);
                rc = CMDO2_ERROR_INVALID_PARAMETER;
                break;
            }
            ++saw_b;
        } else {
            printf("unexpected wildcard match [%s]\r\n", fd.name);
            rc = CMDO2_ERROR_INVALID_PARAMETER;
            break;
        }

        rc = CmdO2FindNext(h, &fd);
    }

    if (h != CMDO2_FIND_CREATE)
        (void)CmdO2FindClose(h);

    (void)CmdO2Delete(file_a);
    (void)CmdO2Delete(file_b);
    CmdO2Done();

    if (rc != CMDO2_ERROR_NO_MORE_FILES) {
        printf("find enumeration stopped rc=%lu\r\n", rc);
        return 4;
    }
    if (entries != 2 || saw_a != 1 || saw_b != 1) {
        printf("find enumeration mismatch entries=%d a=%d b=%d\r\n",
               entries, saw_a, saw_b);
        return 5;
    }

    printf("M29I_DOSFIND_BACKEND_OK\r\n");
    return 0;
}
