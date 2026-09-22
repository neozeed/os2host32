#include <stdio.h>
#include <string.h>

/* Pull the static COPY path/default helpers into this host-side regression.
 * Unused command/backend sections are discarded by --gc-sections. */
#include "cmdfile.c"


static char fake_cwd[260] = "C:\\TEMP";
static char opened_destination[260];
static int write_open_count;
static int fake_read_done;

CmdO2Rc CmdO2QueryCurrentDir(char *buffer, unsigned long cap)
{
    size_t n;
    n = strlen(fake_cwd);
    if ((unsigned long)(n + 1U) > cap)
        return CMDO2_ERROR_INVALID_PARAMETER;
    strcpy(buffer, fake_cwd);
    return 0;
}

int CmdO2IsDirectory(const char *path)
{
    /* Deliberately reject BIN\ here: cmdfile.c must normalize the trailing
     * separator before asking the backend whether the destination is a dir. */
    return strcmp(path, ".") == 0 || strcmp(path, "C:\\TEMP") == 0 ||
           strcmp(path, "BIN") == 0;
}

CmdO2Rc CmdO2OpenWriteReplace(const char *path, CmdO2Handle *handle)
{
    ++write_open_count;
    strncpy(opened_destination, path, sizeof(opened_destination) - 1U);
    opened_destination[sizeof(opened_destination) - 1U] = '\0';
    *handle = 2UL;
    return 0;
}

CmdO2Rc CmdO2OpenRead(const char *path, CmdO2Handle *handle)
{
    (void)path;
    fake_read_done = 0;
    *handle = 1UL;
    return 0;
}

CmdO2Rc CmdO2Read(CmdO2Handle handle, void *buffer,
                  unsigned long count, unsigned long *actual)
{
    (void)handle;
    if (!fake_read_done && count != 0UL) {
        ((unsigned char *)buffer)[0] = (unsigned char)'X';
        *actual = 1UL;
        fake_read_done = 1;
    } else {
        *actual = 0UL;
    }
    return 0;
}

CmdO2Rc CmdO2Write(CmdO2Handle handle, const void *buffer,
                   unsigned long count, unsigned long *actual)
{
    (void)handle;
    (void)buffer;
    *actual = count;
    return 0;
}

CmdO2Rc CmdO2Close(CmdO2Handle handle)
{
    (void)handle;
    return 0;
}

CmdO2Rc CmdO2Delete(const char *path)
{
    (void)path;
    return 0;
}

CmdO2Rc CmdO2FindFirst(const char *pattern, CmdO2FindHandle *handle,
                       struct CmdO2FindData *data)
{
    (void)pattern;
    (void)handle;
    (void)data;
    return CMDO2_ERROR_FILE_NOT_FOUND;
}

CmdO2Rc CmdO2FindNext(CmdO2FindHandle handle, struct CmdO2FindData *data)
{
    (void)handle;
    (void)data;
    return CMDO2_ERROR_NO_MORE_FILES;
}

CmdO2Rc CmdO2FindClose(CmdO2FindHandle handle)
{
    (void)handle;
    return 0;
}

static int copy_integration(void)
{
    int rc;

    write_open_count = 0;
    opened_destination[0] = '\0';
    rc = CmdFileCopy("C:\\TEST.TXT");
    if (rc != 0 || write_open_count != 1 ||
        strcmp(opened_destination, ".\\TEST.TXT") != 0) {
        fprintf(stderr,
                "implicit destination copy failed: rc=%d opens=%d dst=[%s]\n",
                rc, write_open_count, opened_destination);
        return 0;
    }

    write_open_count = 0;
    opened_destination[0] = '\0';
    rc = CmdFileCopy("C:\\TEMP\\TEST.TXT");
    if (rc == 0 || write_open_count != 0) {
        fprintf(stderr,
                "implicit self-copy guard failed: rc=%d opens=%d dst=[%s]\n",
                rc, write_open_count, opened_destination);
        return 0;
    }

    write_open_count = 0;
    rc = CmdFileCopy("TEST.TXT");
    if (rc == 0 || write_open_count != 0) {
        fprintf(stderr,
                "relative self-copy guard failed: rc=%d opens=%d\n",
                rc, write_open_count);
        return 0;
    }

    write_open_count = 0;
    opened_destination[0] = '\0';
    rc = CmdFileCopy("C:\\TEST.TXT BIN\\");
    if (rc != 0 || write_open_count != 1 ||
        strcmp(opened_destination, "BIN\\TEST.TXT") != 0) {
        fprintf(stderr,
                "trailing-slash directory destination failed: rc=%d opens=%d dst=[%s]\n",
                rc, write_open_count, opened_destination);
        return 0;
    }
    return 1;
}

static int same(const char *a, const char *b, const char *cwd, int want)
{
    int got;

    got = cf_paths_same_with_cwd(a, b, cwd);
    if (got != want) {
        fprintf(stderr, "same-path mismatch: [%s] [%s] cwd=[%s] got=%d want=%d\n",
                a, b, cwd, got, want);
        return 0;
    }
    return 1;
}

static int default_dst(int nsrc, int saw_plus, int expect_source,
                       const char *initial, const char *want, int want_rc)
{
    char dst[32];
    int rc;

    strcpy(dst, initial);
    rc = cf_copy_default_destination(nsrc, saw_plus, expect_source,
                                     dst, sizeof(dst));
    if (rc != want_rc || strcmp(dst, want) != 0) {
        fprintf(stderr,
                "default-dst mismatch: nsrc=%d plus=%d expect=%d initial=[%s] -> rc=%d dst=[%s], want rc=%d dst=[%s]\n",
                nsrc, saw_plus, expect_source, initial, rc, dst,
                want_rc, want);
        return 0;
    }
    return 1;
}

int main(void)
{
    if (!default_dst(1, 0, 0, "", ".", 1)) return 1;
    if (!default_dst(1, 0, 0, "OUT.TXT", "OUT.TXT", 0)) return 1;
    if (!default_dst(2, 1, 0, "", "", 0)) return 1;
    if (!default_dst(1, 0, 1, "", "", 0)) return 1;

    if (!same("C:\\TEMP\\TEST.TXT", ".\\test.txt", "C:\\TEMP", 1)) return 1;
    if (!same("test.txt", ".\\TEST.TXT", "C:\\TEMP", 1)) return 1;
    if (!same("C:\\TEMP\\SUB\\..\\TEST.TXT", ".\\test.txt", "C:\\TEMP", 1)) return 1;
    if (!same("\\TEMP\\TEST.TXT", ".\\test.txt", "C:\\TEMP", 1)) return 1;
    if (!same("C:/TEMP/TEST.TXT", ".\\test.txt", "C:\\TEMP", 1)) return 1;
    if (!same("C:\\TEST.TXT", ".\\test.txt", "C:\\TEMP", 0)) return 1;
    if (!same("D:\\TEMP\\TEST.TXT", ".\\test.txt", "C:\\TEMP", 0)) return 1;
    if (!copy_integration()) return 1;

    puts("CMD32_COPY_QOL_HOST_OK");
    return 0;
}
