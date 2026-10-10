/* C89 / Microsoft C/386: flat DOS/NLS calls plus far16 VIO. */
#include "viofetch_api.h"
#include <stdio.h>
#include <string.h>
static USHORT screen_rows, screen_cols, write_error;
static void print_color(USHORT row, USHORT col, const char *str, BYTE attr)
{
    size_t n;
    USHORT rc;
    if (row >= screen_rows || col >= screen_cols) return;
    n = strlen(str);
    if (n > (size_t)(screen_cols - col)) n = screen_cols - col;
    rc = VioWrtCharStrAtt((char *)str, (USHORT)n, row, col, &attr, 0);
    if (rc && !write_error) write_error = rc;
}
/* Country is a locale, not proof of the loaded keyboard layout. */
static const char *country_name(ULONG country)
{
    switch (country) {
    case 1: return "United States";
    case 2: return "Canada";
    case 33: return "France";
    case 34: return "Spain";
    case 39: return "Italy";
    case 44: return "United Kingdom";
    case 49: return "Germany";
    default: return "Other";
    }
}
void draw_os2_logo(void) {
    /* Diamond top cap */
    print_color(1, 10, "    /\\    ", 0x08);
    print_color(2,  8, "  /    \\  ", 0x08);
    print_color(3,  6, " /        \\ ", 0x08);

    /* Row 4: Upper loops of O, S, /, 2 */
    print_color(4,  4, "/  ", 0x08);
    print_color(4,  7, "\xDC\xDF\xDF\xDF\xDC", 0x0E);   /* O top arc:    ▄▀▀▀▄  (Yellow) */
    print_color(4, 13, "\xDC\xDF\xDF\xDF\xDC", 0x0C);   /* S top arc:    ▄▀▀▀▄  (Red)    */
    print_color(4, 19, "  \xDC\xDF",       0x0E);   /* / top:          ▄▀   (Yellow) */
    print_color(4, 23, "\xDC\xDF\xDF\xDF\xDC", 0x0C);   /* 2 top hook:   ▄▀▀▀▄  (Red)    */
    print_color(4, 28, " \\", 0x08);

    /* Row 5: Upper vertical segments */
    print_color(5,  2, "/   ", 0x08);
    print_color(5,  6, "\xDB     \xDB", 0x0E);          /* O sides:      █   █  (Yellow) */
    print_color(5, 13, "\xDB    ",       0x0C);          /* S left bar:   █      (Red)    */
    print_color(5, 20, "\xDC\xDF ",      0x0E);          /* / mid-high:    ▄▀    (Yellow) */
    print_color(5, 27, "\xDB",           0x0B);          /* 2 right bar:      █  (Cyan)   */
    print_color(5, 29, "   \\", 0x08);

    /* Row 6: Center transitions */
    print_color(6,  1, "<    ", 0x08);
    print_color(6,  6, "\xDB     \xDB", 0x0C);          /* O sides:      █   █  (Red)    */
    print_color(6, 14, "\xDF\xDF\xDF\xDC ", 0x0B);      /* S mid bridge:  ▀▀▀▄  (Cyan)   */
    print_color(6, 19, "\xDC\xDF  ",     0x0E);          /* / center:     ▄▀     (Yellow) */
    print_color(6, 25, "\xDC\xDF\xDF  ", 0x0B);          /* 2 diagonal:   ▄▀▀    (Cyan)   */
    print_color(6, 31, " >", 0x08);

    /* Row 7: Lower segments & bottom hooks */
    print_color(7,  2, "\\   ", 0x08);
    print_color(7,  7, "\xDF\xDC\xDC\xDC\xDF", 0x0C);   /* O bottom arc: ▀▄▄▄▀  (Red)    */
    print_color(7, 17, "\xDB",           0x0B);          /* S lower right:    █  (Cyan)   */
    print_color(7, 18, "\xDC\xDF  ",     0x0E);          /* / mid-low:    ▄▀     (Yellow) */
    print_color(7, 23, "\xDC\xDF\xDF    ", 0x0B);        /* 2 diag-low:   ▄▀▀    (Cyan)   */
    print_color(7, 29, "   /", 0x08);

    /* Row 8: Bases of S and 2 */
    print_color(8,  4, "\\  ", 0x08);
    print_color(8, 13, "\xDF\xDC\xDC\xDC\xDF", 0x0B);   /* S bottom arc: ▀▄▄▄▀  (Cyan)   */
    print_color(8, 19, "\xDF   ",        0x0E);          /* / base:       ▀      (Yellow) */
    print_color(8, 23, "\xDF\xDF\xDF\xDF\xDF\xDF", 0x0E);/* 2 flat base: ▀▀▀▀▀▀ (Yellow) */
    print_color(8, 28, " /", 0x08);

    /* Diamond bottom closure */
    print_color(9,   6, " \\        / ", 0x08);
    print_color(10,  8, "  \\    /  ",   0x08);
    print_color(11, 10, "    \\/    ",   0x08);
}


static ULONG disk_mb(ULONG units, ULONG sectors, USHORT sector_bytes)
{
    ULONG bytes, divisor;
    /* Divide before multiplying, avoiding overflow beyond 4 GiB. */
    if (!sector_bytes || !sectors || sectors > 0xffffffffUL / sector_bytes) return 0;
    bytes = sectors * sector_bytes;
    if (bytes > 1048576UL) {
        divisor = bytes / 1048576UL;
        return units > 0xffffffffUL / divisor ? 0xffffffffUL : units * divisor;
    }
    divisor = 1048576UL / bytes;
    return divisor ? units / divisor : 0;
}
int main(void)
{
    ULONG version[2], ms, avail, cp[8], cp_len, actual, drive, drive_map, alt;
    APIRET vr, tr, mr, cr, nr, dr, fr;
    COUNTRYCODE cc;
    COUNTRYINFO ci;
    FSALLOCATE fs;
    VIOMODEPREFIX mode;
    BYTE fill[2], c;
    char buf[128];
    USHORT row, col, rc;
    unsigned i, count;
    memset(version, 0, sizeof(version)); memset(cp, 0, sizeof(cp));
    memset(&cc, 0, sizeof(cc)); memset(&ci, 0, sizeof(ci));
    memset(&fs, 0, sizeof(fs)); memset(&mode, 0, sizeof(mode));
    ms = avail = cp_len = actual = drive = drive_map = alt = 0;
    mode.cb = sizeof(mode); write_error = 0;
    rc = VioGetMode(&mode, 0);
    if (rc) { fprintf(stderr, "VioGetMode failed: rc=%u\n", rc); return 1; }
    screen_rows = mode.row; screen_cols = mode.col;
    if (screen_rows < 16 || screen_cols < 76) {
        fprintf(stderr, "viofetch needs 76 columns and 16 rows (got %u x %u)\n", screen_cols, screen_rows);
        return 1;
    }
    vr = DosQuerySysInfo(11, 12, version, sizeof(version));
    tr = DosQuerySysInfo(14, 14, &ms, sizeof(ms));
    mr = DosQuerySysInfo(19, 19, &avail, sizeof(avail));
    cr = DosQueryCp(sizeof(cp), cp, &cp_len);
    nr = DosQueryCtryInfo(sizeof(ci), &cc, &ci, &actual);
    dr = DosQueryCurrentDisk(&drive, &drive_map);
    fr = DosQueryFSInfo(0, 1, &fs, sizeof(fs));
    fill[0] = ' '; fill[1] = 7;
    rc = VioScrollUp(0, 0, 0xffff, 0xffff, 0xffff, (char *)fill, 0);
    if (rc) { fprintf(stderr, "VioScrollUp failed: rc=%u\n", rc); return 1; }
    draw_os2_logo(); row = 1;
    print_color(row++,36,"OS/2 Desktop",0x0a);
    print_color(row++,36,"--------------------",8);
    if (!vr) sprintf(buf,"OS:         MS OS/2 %lu.%02lu",
        (unsigned long)(version[0] >= 10 ? version[0]/10 : version[0]), (unsigned long)version[1]);
    else sprintf(buf,"OS:         query rc=%lu",(unsigned long)vr);
    print_color(row++,36,buf,7);
    if (!tr) sprintf(buf,"Uptime:     %lu min",(unsigned long)(ms/60000UL));
    else sprintf(buf,"Uptime:     query rc=%lu",(unsigned long)tr);
    print_color(row++,36,buf,7);
    if (!mr) sprintf(buf,"Avail Mem:  %lu.%02lu MB",(unsigned long)(avail/1048576UL),
        (unsigned long)(((avail%1048576UL)*100UL)/1048576UL));
    else sprintf(buf,"Avail Mem:  query rc=%lu",(unsigned long)mr);
    print_color(row++,36,buf,7);
    sprintf(buf,"Display:    %u x %u (VIO)",mode.col,mode.row); print_color(row++,36,buf,7);
    if (!cr || cr==473) {
        count=(unsigned)(cp_len/sizeof(cp[0])); if(count>8) count=8;
        for(i=1;i<count;++i) if(cp[i] && cp[i]!=cp[0]) {alt=cp[i];break;}
        if(alt) sprintf(buf,"Code Page:  (%lu), %lu",(unsigned long)cp[0],(unsigned long)alt);
        else sprintf(buf,"Code Page:  (%lu)",(unsigned long)cp[0]);
    } else sprintf(buf,"Code Page:  query rc=%lu",(unsigned long)cr);
    print_color(row++,36,buf,7);
    if(!nr) sprintf(buf,"Country:    %s",country_name(ci.country));
    else sprintf(buf,"Country:    query rc=%lu",(unsigned long)nr);
    print_color(row++,36,buf,7);
    if(!fr && !dr && drive>=1 && drive<=26) sprintf(buf,"Disk %c: %lu / %lu MB free",(int)('A'+drive-1),
        (unsigned long)disk_mb(fs.cUnitAvail,fs.cSectorUnit,fs.cbSector),
        (unsigned long)disk_mb(fs.cUnit,fs.cSectorUnit,fs.cbSector));
    else sprintf(buf,"Disk:       query rc=%lu",(unsigned long)(fr?fr:dr));
    print_color(row++,36,buf,7);
    if(!dr && drive>=1 && drive<=26) sprintf(buf,"Cur Drv:    Drive %c:",(int)('A'+drive-1));
    else sprintf(buf,"Cur Drv:    query rc=%lu",(unsigned long)dr);
    print_color(row++,36,buf,7);
    ++row; col=36;
    for(c=1;c<=7;++c) { print_color(row,col,"   ",(BYTE)(c<<4)); col+=4; }
    rc=VioSetCurPos(14,0,0);
    if(write_error || rc) { fprintf(stderr,"VIO output failed: rc=%u\n",write_error?write_error:rc);return 1; }
    return 0;
}
