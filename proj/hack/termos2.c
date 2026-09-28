/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* hack.termcap.c - version 1.0.3 */
/*
 * Native OS/2 character-session backend for Hack 1.03.
 *
 * This is intentionally a close analogue of termnt.c: ordinary character
 * output remains on stdout (Hack contains a number of direct putchar/fputs/
 * printf calls), while cursor movement, erasing, screen sizing, standout
 * attributes, delay, and cursor state use native OS/2 VIO/DOS services.
 *
 * Hack keeps screen coordinates 1-based.  The VIO API is 0-based.
 */

#define INCL_16
#define INCL_VIO
#define INCL_KBD
#define INCL_DOSPROCESS
#define INCL_DOSMISC
#include <os2.h>
#include <stdio.h>

#include "config.h"     /* ROWNO, COLNO, xchar */
#include "flag.h"       /* flags.nonull */

char *CD;               /* tested in pri.c: docorner() */
int CO, LI;             /* used in pri.c and pager.c */

extern xchar curx, cury;

static BYTE os2_normal_attr = 0x07;
static BYTE os2_standout_attr = 0x70;
static VIOCURSORINFO os2_saved_cursor;
static int os2_have_saved_cursor = 0;
static int os2_vio_ok = 0;

static int os2_so_active = 0;
static USHORT os2_so_row = 0;
static USHORT os2_so_col = 0;

static void
os2_flush()
{
        (void) fflush(stdout);
}

static void
os2_pos(x, y, prow, pcol)
int x, y;
PUSHORT prow, pcol;
{
        if(x < 1) x = 1;
        if(y < 1) y = 1;
        if(x > CO) x = CO;
        if(y > LI) y = LI;

        *prow = (USHORT)(y - 1);
        *pcol = (USHORT)(x - 1);
}

/* Reverse the conventional text-mode foreground/background nibbles. */
static BYTE
os2_reverse_attr(a)
BYTE a;
{
        BYTE fg, bg;

        fg = (BYTE)(a & 0x0f);
        bg = (BYTE)(a & 0x70);

        /* Move foreground intensity into the high background bit, exactly
         * as a simple nibble swap would do on an IBM text attribute. */
        return((BYTE)((fg << 4) | (bg >> 4)));
}

static void
os2_clear_cells(x, y, count)
int x, y;
unsigned count;
{
        BYTE cell[2];
        USHORT row, col;

        if(!os2_vio_ok || count == 0)
                return;

        os2_pos(x, y, &row, &col);
        cell[0] = ' ';
        cell[1] = os2_normal_attr;
        (void) VioWrtNCell(cell, (USHORT)count, row, col, 0);
}

/* Apply an attribute over a cursor-delimited linear screen span. */
static void
os2_attr_span(srow, scol, erow, ecol, attr)
USHORT srow, scol, erow, ecol;
BYTE attr;
{
        USHORT row, start, count;

        if(!os2_vio_ok)
                return;
        if(erow < srow || (erow == srow && ecol <= scol))
                return;

        row = srow;
        while(row <= erow && row < (USHORT)LI) {
                start = (row == srow) ? scol : 0;

                if(row == erow)
                        count = (ecol > start) ? (USHORT)(ecol - start) : 0;
                else
                        count = (USHORT)(CO - start);

                if(count)
                        (void) VioWrtNAttr(&attr, count, row, start, 0);

                if(row == erow)
                        break;
                ++row;
        }
}

startup()
{
        VIOMODEINFO mode;
        BYTE cell[2];
        USHORT cb, row, col;

        CO = COLNO;
        LI = ROWNO + 2;
        CD = "VIO";             /* native clear-to-end-of-screen exists */

        mode.cb = sizeof(mode);
        if(VioGetMode(&mode, 0) == 0) {
                os2_vio_ok = 1;

                if(mode.col)
                        CO = (int)mode.col;
                if(mode.row)
                        LI = (int)mode.row;

                /* curx/cury are xchar values: keep them in their range. */
                if(CO > 127) CO = 127;
                if(LI > 127) LI = 127;

                row = col = 0;
                (void) VioGetCurPos(&row, &col, 0);
                cb = 2;
                if(VioReadCellStr((PCH)cell, &cb, row, col, 0) == 0 && cb >= 2)
                        os2_normal_attr = cell[1];

                os2_standout_attr = os2_reverse_attr(os2_normal_attr);
                if(os2_standout_attr == os2_normal_attr)
                        os2_standout_attr = 0x70;

                if(VioGetCurType(&os2_saved_cursor, 0) == 0)
                        os2_have_saved_cursor = 1;
        }

        if(!os2_vio_ok || CO < COLNO || LI < ROWNO + 2)
                setclipped();

        set_whole_screen();
        return(0);
}

start_screen()
{
        os2_flush();
        return(0);
}

end_screen()
{
        os2_flush();
        if(os2_have_saved_cursor)
                (void) VioSetCurType(&os2_saved_cursor, 0);
        return(0);
}

/* Cursor movements */
curs(x, y)
register int x, y;
{
        if(x < 1) x = 1;
        if(y < 1) y = 1;

        if(y == cury && x == curx)
                return(0);
        cmov(x, y);
        return(0);
}

nocmov(x, y)
int x, y;
{
        cmov(x, y);
        return(0);
}

cmov(x, y)
register int x, y;
{
        USHORT row, col;

        if(x < 1) x = 1;
        if(y < 1) y = 1;
        if(x > CO) x = CO;
        if(y > LI) y = LI;

        os2_flush();
        os2_pos(x, y, &row, &col);
        if(os2_vio_ok)
                (void) VioSetCurPos(row, col, 0);

        cury = (xchar)y;
        curx = (xchar)x;
        return(0);
}

/*
 * Keep the character stream on stdout.  Hack 1.03 itself performs output
 * through putchar(), fputs(), puts(), and printf() in addition to xputc/xputs;
 * using one stream for printable text keeps those paths ordered.  Every VIO
 * operation flushes stdout first.
 */
xputc(c)
char c;
{
        (void) fputc(c, stdout);
        return(0);
}

xputs(s)
char *s;
{
        (void) fputs(s, stdout);
        return(0);
}

/* Clear from Hack's logical cursor through the end of its current line. */
cl_end()
{
        unsigned count;

        os2_flush();
        if(curx < 1 || curx > CO)
                return(0);
        count = (unsigned)(CO - (int)curx + 1);
        os2_clear_cells((int)curx, (int)cury, count);
        return(0);
}

clear_screen()
{
        BYTE cell[2];

        os2_flush();
        cell[0] = ' ';
        cell[1] = os2_normal_attr;

        if(os2_vio_ok) {
                /* The OS/2 reference explicitly documents all-FFFF as the
                 * clear-whole-screen form of VioScrollUp. */
                (void) VioScrollUp(0, 0, 0xffff, 0xffff, 0xffff, cell, 0);
                (void) VioSetCurPos(0, 0, 0);
        }

        curx = cury = 1;
        return(0);
}

home()
{
        cmov(1, 1);
        return(0);
}

/*
 * VIO has per-cell attributes rather than a Win32-like persistent console
 * text attribute.  Record the cursor at standoutbeg(), let Hack emit through
 * its normal stdout paths, then recolor exactly that span at standoutend().
 */
standoutbeg()
{
        os2_flush();
        if(os2_vio_ok &&
           VioGetCurPos(&os2_so_row, &os2_so_col, 0) == 0)
                os2_so_active = 1;
        return(0);
}

standoutend()
{
        USHORT row, col;

        os2_flush();
        if(os2_so_active && os2_vio_ok &&
           VioGetCurPos(&row, &col, 0) == 0)
                os2_attr_span(os2_so_row, os2_so_col, row, col,
                              os2_standout_attr);
        os2_so_active = 0;
        return(0);
}

backsp()
{
        if(curx > 1)
                cmov((int)curx - 1, (int)cury);
        return(0);
}

bell()
{
        os2_flush();
        (void) DosBeep(880, 50);
        return(0);
}

delay_output()
{
        (void) DosSleep(50L);
        return(0);
}

/* Clear from the logical cursor to the end of the screen. */
cl_eos()
{
        unsigned count;

        os2_flush();
        if(curx < 1 || curx > CO || cury < 1 || cury > LI)
                return(0);

        count = (unsigned)(CO - (int)curx + 1);
        if((int)cury < LI)
                count += (unsigned)((LI - (int)cury) * CO);

        os2_clear_cells((int)curx, (int)cury, count);
        return(0);
}

