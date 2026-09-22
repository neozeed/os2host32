/* C/386 prerelease API regression. Creates V2IO-TST.DAT exclusively.
 * An existing file is never replaced; delete the prior test file manually
 * after investigating any failed test. No SDK structure packing assumptions.
 */
#define INCL_DOSPROCESS
#define INCL_DOSFILEMGR
#define INCL_DOSDATETIME
#include <os2.h>
#include <stdio.h>
#include <string.h>
typedef ULONG U;
#define CHECK(x) do { if (!(x)) { printf("fileio1 FAIL line %d\n", __LINE__); return 1; } } while (0)
static U get32(const unsigned char *p)
{ return (U)p[0] | ((U)p[1]<<8) | ((U)p[2]<<16) | ((U)p[3]<<24); }
int main(void)
{
    HFILE h;
    ULONG action, n, pos;
    APIRET rc;
    unsigned char info[24];
    DATETIME dt;
    char buf[32], full[512];
    char path[] = "V2IO-TST.DAT";
    char payload[] = "abcdef";
    char onebyte[] = "X";
    CHECK(DosOpen(path, &h, &action, 0, 0, 0x10, 0x42, 0) == 0);
    CHECK(action == 2);
    CHECK(DosWrite(h, (PVOID)payload, 6, &n) == 0 && n == 6);
    CHECK(DosSetFilePtr(h, -3L, 2, &pos) == 0 && pos == 3);
    memset(buf, 0, sizeof(buf));
    CHECK(DosRead(h, (PVOID)buf, 3, &n) == 0 && n == 3 && memcmp(buf, "def", 3) == 0);
    CHECK(DosRead(h, (PVOID)buf, 1, &n) == 0 && n == 0);
    CHECK(DosQueryFileInfo(h, 1, (PVOID)info, sizeof(info)) == 0 && get32(info+12) == 6);
    CHECK(DosSetFilePtr(h, 2L, 0, &pos) == 0 && pos == 2);
    CHECK(DosSetFileSize(h, 3) == 0);
    CHECK(DosSetFilePtr(h, 0L, 1, &pos) == 0 && pos == 2);
    CHECK(DosRead(h, (PVOID)buf, sizeof(buf), &n) == 0 && n == 1 && buf[0] == 'c');
    CHECK(DosClose(h) == 0);
    CHECK(DosRead(h, (PVOID)buf, 1, &n) == 6);
    CHECK(DosClose(h) == 6);
    CHECK(DosQueryPathInfo(path, 1, (PVOID)info, sizeof(info)) == 0 && get32(info+12) == 3);
    CHECK(DosQueryPathInfo(path, 5, (PVOID)full, sizeof(full)) == 0);
    CHECK(DosOpen(path, &h, &action, 0, 0, 1, 0x40, 0) == 0 && action == 1);
    CHECK(DosRead(h, (PVOID)buf, sizeof(buf), &n) == 0 && n == 3 && memcmp(buf,"abc",3) == 0);
    rc = DosWrite(h, (PVOID)onebyte, 1, &n); CHECK(rc != 0);
    CHECK(DosClose(h) == 0);
    CHECK(DosGetDateTime(&dt) == 0 && dt.month >= 1 && dt.month <= 12);
    CHECK(DosDelete(path) == 0);
    CHECK(DosOpen(path, &h, &action, 0, 0, 1, 0x40, 0) == 2);
    printf("fileio1 PASS\n");
    return 0;
}
