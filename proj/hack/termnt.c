/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* hack.termcap.c - version 1.0.3 */
/*
 * Native Win32 console backend.
 *
 * Hack's screen bookkeeping is 1-based: curx == 1, cury == 1 is the
 * upper-left cell.  Win32 console coordinates are 0-based, so keep the
 * conversion in nt_coord() and do not leak ANSI/VT escape sequences into
 * the rest of the game.
 */

#if _MSC_VER < 1100
#define __export
#define __huge
#endif

#include <windows.h>
#include <stdio.h>

#include "config.h"     /* for ROWNO and COLNO */
#include "flag.h"       /* for flags.nonull */

HANDLE hConsoleOut;
HANDLE hConsoleIn;

char *CD;                /* tested in pri.c: docorner() */
int CO, LI;              /* used in pri.c and pager.c */

extern xchar curx, cury;

static int nt_console_ok = 0;
static COORD nt_origin;
static WORD nt_normal_attr;
static WORD nt_standout_attr;
static CONSOLE_CURSOR_INFO nt_saved_cursor;
static int nt_have_saved_cursor = 0;

/*
 * Translate Hack's 1-based logical screen coordinates into Win32 console
 * buffer coordinates.  The logical screen is anchored to the visible
 * console window that was present at startup, rather than blindly assuming
 * that the window starts at buffer row zero.
 */
static COORD
nt_coord(x, y)
int x, y;
{
        COORD p;

        if(x < 1) x = 1;
        if(y < 1) y = 1;

        p.X = (SHORT)(nt_origin.X + x - 1);
        p.Y = (SHORT)(nt_origin.Y + y - 1);
        return p;
}

static void
nt_flush()
{
        (void) fflush(stdout);
}

/* Fill part of one logical screen line without disturbing the cursor. */
static void
nt_clear_line(y, first_col)
int y, first_col;
{
        COORD p;
        DWORD count, done;

        if(!nt_console_ok)
                return;
        if(y < 1 || y > LI)
                return;
        if(first_col < 1)
                first_col = 1;
        if(first_col > CO)
                return;

        p = nt_coord(first_col, y);
        count = (DWORD)(CO - first_col + 1);
        done = 0;
        (void) FillConsoleOutputCharacterA(hConsoleOut, ' ', count, p, &done);
        done = 0;
        (void) FillConsoleOutputAttribute(hConsoleOut, nt_normal_attr,
                                          count, p, &done);
}

startup()
{
        CONSOLE_SCREEN_BUFFER_INFO info;
        int width, height;

        hConsoleOut = GetStdHandle(STD_OUTPUT_HANDLE);
        hConsoleIn  = GetStdHandle(STD_INPUT_HANDLE);

        nt_origin.X = 0;
        nt_origin.Y = 0;
        nt_normal_attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        nt_standout_attr = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;

        CO = COLNO;
        LI = ROWNO + 2;
        CD = "WIN32";          /* native clear-to-end-of-screen is available */

        if(hConsoleOut != INVALID_HANDLE_VALUE && hConsoleOut != NULL &&
           GetConsoleScreenBufferInfo(hConsoleOut, &info)) {
                nt_console_ok = 1;
                nt_origin.X = info.srWindow.Left;
                nt_origin.Y = info.srWindow.Top;
                nt_normal_attr = info.wAttributes;

                /* Reverse foreground/background for standout text. */
                nt_standout_attr = (WORD)
                    ((nt_normal_attr & 0xFF00) |
                     ((nt_normal_attr & 0x000F) << 4) |
                     ((nt_normal_attr & 0x00F0) >> 4));

                width = info.srWindow.Right - info.srWindow.Left + 1;
                height = info.srWindow.Bottom - info.srWindow.Top + 1;

                /* xchar coordinates only have a small positive range. */
                if(width > 127) width = 127;
                if(height > 127) height = 127;

                CO = width;
                LI = height;

                if(GetConsoleCursorInfo(hConsoleOut, &nt_saved_cursor))
                        nt_have_saved_cursor = 1;
        }

        (void) SetConsoleTitleA("Hack 1.03");

        if(!nt_console_ok || CO < COLNO || LI < ROWNO + 2)
                setclipped();

        set_whole_screen();     /* uses LI and CD */
        return(0);
}

start_screen()
{
        if(nt_console_ok) {
                nt_flush();
                (void) SetConsoleTextAttribute(hConsoleOut, nt_normal_attr);
        }
        return(0);
}

end_screen()
{
        if(nt_console_ok) {
                nt_flush();
                (void) SetConsoleTextAttribute(hConsoleOut, nt_normal_attr);
                if(nt_have_saved_cursor)
                        (void) SetConsoleCursorInfo(hConsoleOut, &nt_saved_cursor);
        }
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

/*
 * A Win32 console has direct cursor addressing, so the old termcap relative
 * movement optimisation is unnecessary.  Keep nocmov() because the rest of
 * the historical source expects the symbol to exist.
 */
nocmov(x, y)
int x, y;
{
        cmov(x, y);
        return(0);
}

cmov(x, y)
register int x, y;
{
        COORD p;

        if(x < 1) x = 1;
        if(y < 1) y = 1;
        if(x > CO) x = CO;
        if(y > LI) y = LI;

        nt_flush();
        p = nt_coord(x, y);
        if(nt_console_ok)
                (void) SetConsoleCursorPosition(hConsoleOut, p);

        cury = (xchar)y;
        curx = (xchar)x;
        return(0);
}

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

/* Clear from the logical cursor to the end of the current line. */
cl_end()
{
        nt_flush();
        nt_clear_line((int)cury, (int)curx);
        return(0);
}

clear_screen()
{
        int y;
        COORD p;

        nt_flush();

        if(nt_console_ok) {
                for(y = 1; y <= LI; ++y)
                        nt_clear_line(y, 1);

                p = nt_coord(1, 1);
                (void) SetConsoleCursorPosition(hConsoleOut, p);
                (void) SetConsoleTextAttribute(hConsoleOut, nt_normal_attr);
        }

        curx = cury = 1;
        return(0);
}

home()
{
        cmov(1, 1);
        return(0);
}

standoutbeg()
{
        if(nt_console_ok) {
                nt_flush();
                (void) SetConsoleTextAttribute(hConsoleOut, nt_standout_attr);
        }
        return(0);
}

standoutend()
{
        if(nt_console_ok) {
                nt_flush();
                (void) SetConsoleTextAttribute(hConsoleOut, nt_normal_attr);
        }
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
        nt_flush();
        if(!Beep(750, 50))
                (void) fputc('\007', stdout);
        return(0);
}

delay_output()
{
        nt_flush();
        Sleep(50);
        return(0);
}

/* Clear from the logical cursor to the end of the visible Hack screen. */
cl_eos()
{
        int y;
        int save_x, save_y;

        nt_flush();

        save_x = (int)curx;
        save_y = (int)cury;

        nt_clear_line(save_y, save_x);
        for(y = save_y + 1; y <= LI; ++y)
                nt_clear_line(y, 1);

        /* FillConsoleOutput* does not move the cursor, but reassert it in
           case a host console implementation behaves differently. */
        cmov(save_x, save_y);
        return(0);
}
