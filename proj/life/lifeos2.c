/*
 * LIFEOS2.C
 *
 * 16-bit OS/2 VIO/KBD port of the NT console version of DeSmet C88 LIFE.
 *
 * This deliberately targets the classic 16-bit OS/2 Base APIs.  M_I386 is
 * not used: <os2.h> therefore selects INCL_16 and maps Vio/Kbd/Dos calls to
 * their 16-bit entry points.
 */


#define INCL_16
#define INCL_VIO
#define INCL_KBD
#define INCL_DOSPROCESS
#include <os2.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define ROWS 23
#define COLS 80

/* BIOS-compatible scan codes returned by OS/2 KbdCharIn for extended keys. */
#define up_char         72
#define down_char       80
#define left_char       75
#define right_char      77
#define bol_char        200
#define eol_char        201
#define pageup_char     202
#define pagedown_char   203
#define bof_char        204
#define eof_char        205
#define Ins_char        82
#define Del_char        83
#define NextWord_char   208
#define PrevWord_char   209

#define M1  210
#define M2  211
#define M3  212
#define M4  213
#define M5  214
#define M6  215
#define M7  216
#define M8  217
#define M9  218
#define M10 219

static char world[ROWS][COLS];
static char create_mode = 1;
static char quit_flag;
static short population;
static short crow, ccol;
static long generation;
static char color_opt, color;
static int ndel;
static int extended;

static VIOCURSORINFO saved_cursor;
static int saved_cursor_valid;

static void instruct(void);
static void setup(void);
static void screen(void);
static void create(int suspend);
static void cycle(void);
static void add8(short row, short col);
static int life_delay(int n);

static void scr_clr(void);
static int  scr_csts(void);
static void scr_cursoff(void);
static void scr_curson(void);
static void scr_co(char c);
static void scr_setup(void);
static void scr_rowcol(int y, int x);
static void vio_puts(const char *s);
static void vio_write_at(int y, int x, const char *s);

int main(int argc, char **argv)
{
    int ch;

    scr_setup();
    scr_clr();

    if (argc > 1 && toupper((unsigned char)*argv[1]) == 'C')
        color_opt = 1;

    instruct();
    ndel = 100;
    setup();

    do {
        generation++;
        cycle();
        screen();
        life_delay(ndel);

        ch = scr_csts();
        switch (ch) {
        case 27:
            quit_flag = 1;
            break;
        case '+':
            ndel--;
            if (ndel < 5)
                ndel = 5;
            break;
        case '-':
            ndel++;
            if (ndel > 300)
                ndel = 300;
            break;
        default:
            break;
        }
    } while (population && !quit_flag);

    scr_rowcol(ROWS, 0);
    if (population == 0)
        vio_puts("Nobody left, sorry about that.\r\n");
    else
        vio_puts("bye\r\n");

    scr_curson();
    return 0;
}

static void instruct(void)
{
    vio_puts("                The game of Life by John Conway\r\n");
    vio_puts("                 Use LIFE C with color monitor.\r\n");
    vio_puts(" If started with a number, a random pattern starts the game.\r\n");
    vio_puts("  Otherwise, move cursor with the 4 arrow keys to create life.\r\n");
    vio_puts("    DEL changes cursor movement to mean that cells are deleted\r\n");
    vio_puts("                 INS flips back to create mode.\r\n");
    vio_puts("          The 's' key will toggle the game on or off.\r\n");
    vio_puts("       A + will speed generations up, a - will slow them.\r\n");
    vio_puts("         Repeat +'s or -'s until the speed suits you.\r\n");
    vio_puts("                     Tap ESC to bail out.\r\n");
    vio_puts("            Enter starting number of cells or tap CR   ");
}

static void setup(void)
{
    short rnumber;
    short i, row, col, seed, rnum;
    int ch;

    rnumber = 0;
    seed = 0;

    while (1) {
        while ((ch = scr_csts()) == 0) {
            seed++;
            DosSleep(1L);
        }
        if (ch < '0' || ch > '9')
            break;
        scr_co((char)ch);
        rnumber *= 10;
        rnumber += (short)(ch - '0');
    }

    scr_cursoff();
    scr_clr();
    vio_write_at(ROWS, 0, "Generation    0  Population    0");
    vio_write_at(ROWS, 35, "ESC=quit. + speeds. - slows. S stops&starts.");

    srand((unsigned)seed);

    for (i = 0; i < rnumber; i++) {
        rnum = (short)rand();
        if (rnum < 0)
            rnum = (short)-rnum;
        row = (short)(rnum % ROWS);
        col = (short)((rnum / ROWS) % COLS);
        world[row][col] = 'X';
        scr_rowcol(row, col);
        scr_co(2);
    }

    if (rnumber == 0)
        create(1);
}

static void screen(void)
{
    short row, col;
    char cell;
    char buf[16];

    population = 0;

    for (row = 0; row < ROWS; row++) {
        for (col = 0; col < COLS; col++) {
            cell = world[row][col];

            /* Stay alive with 2 or 3 neighbours; born with exactly 3. */
            if (cell && (cell == 3 || cell == 'X' + 2 || cell == 'X' + 3)) {
                population++;
                if (cell < 'X') {
                    scr_rowcol(row, col);
                    scr_co(2);
                }
                cell = 'X';
            } else {
                if (cell >= 'X') {
                    scr_rowcol(row, col);
                    scr_co(' ');
                }
                cell = 0;
            }
            world[row][col] = cell;
        }
    }

    sprintf(buf, "%4ld", generation);
    vio_write_at(ROWS, 11, buf);
    sprintf(buf, "%4d", population);
    vio_write_at(ROWS, 28, buf);
}

static void create(int suspend)
{
    int ch;
    char wait;

    if (suspend)
        scr_curson();

    /*
     * The NT port had this disabled as "while (1==2)".  Restore the
     * original intended behaviour: while paused, wait for keys; while the
     * simulation is running, consume only keys already waiting in KBD.
     */
    while ((ch = scr_csts()) || suspend) {
        if (ch == 0) {
            DosSleep(1L);
            continue;
        }

        switch (ch) {
        case up_char:
            if (extended)
                crow = crow ? (short)(crow - 1) : (short)(ROWS - 1);
            else
                continue;
            break;

        case down_char:
            if (extended)
                crow = (crow == ROWS - 1) ? 0 : (short)(crow + 1);
            else
                continue;
            break;

        case left_char:
            if (extended)
                ccol = ccol ? (short)(ccol - 1) : (short)(COLS - 1);
            else
                continue;
            break;

        case right_char:
            if (extended)
                ccol = (ccol == COLS - 1) ? 0 : (short)(ccol + 1);
            else
                continue;
            break;

        case 's':
            suspend = !suspend;
            if (suspend)
                scr_curson();
            else
                scr_cursoff();
            continue;

        case '+':
            ndel -= ndel / 10;
            if (ndel < 5)
                ndel = 5;
            continue;

        case '-':
            ndel += ndel / 10;
            if (ndel > 300)
                ndel = 300;
            continue;

        case Ins_char:
            if (extended)
                create_mode = 1;
            continue;

        case Del_char:
            if (extended)
                create_mode = 0;
            continue;

        case 0x1b:
            quit_flag = 1;
            scr_cursoff();
            return;

        default:
            continue;
        }

        world[crow][ccol] = create_mode ? 'X' : 0;
        scr_rowcol(crow, ccol);

        if (create_mode) {
            scr_co(2);
            population++;
        } else {
            wait = 30;
            while (wait--) {
                scr_co(1);
                scr_rowcol(crow, ccol);
            }
            scr_co(' ');
        }

        /* Keep the editing cursor on the selected cell. */
        if (suspend)
            scr_rowcol(crow, ccol);
    }

    scr_cursoff();
}

static void cycle(void)
{
    short row, col;

    create(0);

    for (row = 0; row < ROWS; row++) {
        if (world[row][0] >= 'X')
            add8(row, 0);
        if (world[row][COLS - 1] >= 'X')
            add8(row, COLS - 1);
    }

    for (col = 1; col < COLS - 1; col++) {
        if (world[0][col] >= 'X')
            add8(0, col);
        if (world[ROWS - 1][col] >= 'X')
            add8(ROWS - 1, col);
    }

    for (row = 1; row < ROWS - 1; row++) {
        for (col = 1; col < COLS - 1; col++) {
            if (world[row][col] >= 'X') {
                world[row - 1][col - 1]++;
                world[row - 1][col]++;
                world[row - 1][col + 1]++;
                world[row][col - 1]++;
                world[row][col + 1]++;
                world[row + 1][col - 1]++;
                world[row + 1][col]++;
                world[row + 1][col + 1]++;
            }
        }
    }
}

static void add8(short row, short col)
{
    short rrow, xcol, rr, cc;

    for (rr = (short)(row - 1); rr <= row + 1; rr++) {
        for (cc = (short)(col - 1); cc <= col + 1; cc++) {
            rrow = (rr != -1) ? rr : (short)(ROWS - 1);
            xcol = (cc != -1) ? cc : (short)(COLS - 1);
            if (rrow >= ROWS)
                rrow = 0;
            if (xcol >= COLS)
                xcol = 0;
            world[rrow][xcol]++;
        }
    }
    world[row][col]--;
}

static int life_delay(int n)
{
    if (n < 0)
        n = 0;
    DosSleep((ULONG)n);
    return 0;
}

static void scr_clr(void)
{
    BYTE cell[2];

    cell[0] = ' ';
    cell[1] = 0x07;

    VioScrollUp(0, 0, 0xffff, 0xffff, 0xffff, cell, 0);
    VioSetCurPos(0, 0, 0);
}

static int scr_csts(void)
{
    KBDKEYINFO key;
    APIRET rc;

    memset(&key, 0, sizeof(key));
    rc = KbdCharIn(&key, IO_NOWAIT, 0);

    if (rc != 0 || key.fbStatus == 0) {
        extended = 0;
        return 0;
    }

    if (key.chChar == 0 || key.chChar == 0xe0) {
        extended = 1;
        return (int)key.chScan;
    }

    extended = 0;
    return (int)key.chChar;
}

static void scr_cursoff(void)
{
    VIOCURSORINFO ci;

    if (!saved_cursor_valid) {
        if (VioGetCurType(&saved_cursor, 0) == 0)
            saved_cursor_valid = 1;
    }

    if (saved_cursor_valid)
        ci = saved_cursor;
    else {
        ci.yStart = 0;
        ci.cEnd = 0;
        ci.cx = 0;
    }

    ci.attr = 0xffff;
    VioSetCurType(&ci, 0);
}

static void scr_curson(void)
{
    VIOCURSORINFO ci;

    if (saved_cursor_valid) {
        VioSetCurType(&saved_cursor, 0);
        return;
    }

    ci.yStart = (USHORT)-90;
    ci.cEnd = (USHORT)-100;
    ci.cx = 0;
    ci.attr = 0;
    VioSetCurType(&ci, 0);
}

static void scr_co(char c)
{
    USHORT row, col;
    char out;

    out = c;

    /* VioWrtCharStr writes the raw display character, so CP437 1/2 work. */
    if (VioGetCurPos(&row, &col, 0) != 0)
        return;

    if (color_opt && c == 2) {
        BYTE attr;
        attr = (BYTE)(1 + ((++color) % 15));
        VioWrtCharStrAtt(&out, 1, row, col, &attr, 0);
    } else {
        VioWrtCharStr(&out, 1, row, col, 0);
    }

    if (col + 1 < COLS)
        VioSetCurPos(row, (USHORT)(col + 1), 0);
}

static void scr_rowcol(int y, int x)
{
    VioSetCurPos((USHORT)y, (USHORT)x, 0);
}

static void scr_setup(void)
{
    if (VioGetCurType(&saved_cursor, 0) == 0)
        saved_cursor_valid = 1;
}

static void vio_puts(const char *s)
{
    VioWrtTTY((PCH)s, (USHORT)strlen(s), 0);
}

static void vio_write_at(int y, int x, const char *s)
{
    VioWrtCharStr((PCH)s, (USHORT)strlen(s), (USHORT)y, (USHORT)x, 0);
}
