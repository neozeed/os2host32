/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* hack.termcap.c - version 1.0.3 */

#include <stdio.h>
#include "config.h"	/* for ROWNO and COLNO */
#include "flag.h"	/* for flags.nonull */
extern char *tgetstr(), *tgoto(), *getenv();
extern long *alloc();

#ifndef lint
extern			/* it is defined in libtermlib (libtermcap) */
#endif lint
	short ospeed;		/* terminal baudrate; used by tputs */
static char tbuf[512];
static char *HO, *CL, *CE, *UP, *CM, *ND, *XD, *BC, *SO, *SE, *TI, *TE;
static char *VS, *VE;
static int SG;
static char PC = '\0';
char *CD;		/* tested in pri.c: docorner() */
int CO, LI;		/* used in pri.c and whatis.c */

static char tgotobuf[20];
#define tgoto(fmt, x, y)        (sprintf(tgotobuf, fmt, y+1, x+1), tgotobuf)

startup()
{
         /* the ANSI termcap */
        HO = "\033[H";		//	String to position cursor at upper left corner.
        CL = "\033[2J";		//	String of commands to clear the entire screen and position the cursor at the upper left corner.
        CE = "\033[K";		//	String of commands to clear from the cursor to the end of the current line.
        UP = "\033[A";		//	up String to move the cursor vertically up one line.
        CM = "\033[%d;%dH";   //	String of commands to position the cursor at line l, column c. Both parameters are origin-zero, and are defined
					//	relative to the screen, not relative to display memory. All display terminals except a few very obsolete ones 
					//	support `cm', so it is acceptable for an application program to refuse to operate on terminals lacking 'cm'.
					  /* used with function tgoto() */
        ND = "\033[C";		//	String to move the cursor right one column.
        XD = "\033[B";		//	It seems that xd is no longer supported, and we should use
     					//    a linefeed instead; unfortunately this requires resetting
					//    CRMOD, and many output routines will have to be modified
 					//    slightly. Let's leave that till the next release. 
        BC = "\033[D";		//	Very obsolete alternative name for the `le' capability.
					//	le String to move the cursor left one column.
					//	LE String to move cursor left n columns.
        SO = "\033[7m";		//	String of commands to enter standout mode.
        SE = "\033[0m";		//	String of commands to leave standout mode.
#if 0
        TI = "";
        TE = "";
#else
	TI = "\033[0m\033[=7l";			//	term init
	TE = "\033[0m\033[=7h";			//	term end
#endif
        VS = "";
        VE = "";


        CD = "\033";
        CO = COLNO;
        LI = ROWNO;
}

start_screen()
{
	xputs(TI);	//\E[0m\E[=7l		String of commands to put the terminal into whatever special modes are needed 
			//				or appropriate for programs that move the cursor nonsequentially around the screen. 
			//				Programs that use termcap to do full-screen display should output this string when they start up.
	xputs(VS);	//String of commands to enhance the cursor.
}

end_screen()
{
	xputs(VE);	//String of commands to return the cursor to normal.
	xputs(TE);	//\E[0m\E[=7h		String of commands to undo what is done by the `ti' string. Programs that output 
			//				the `ti' string on entry should output this string when they exit.

}

/* Cursor movements */
extern xchar curx, cury;

curs(x, y)
register int x, y;      /* not xchar: perhaps xchar is unsigned and
                           curx-x would be unsigned as well */
{

        if (y == cury && x == curx)
                return;
        if(!ND && (curx != x || x <= 3)) {      /* Extremely primitive */
                cmov(x, y);                     /* bunker!wtm */
                return;
        }
        if(abs(cury-y) <= 3 && abs(curx-x) <= 3)
                nocmov(x, y);
        else if((x <= 3 && abs(cury-y)<= 3) || (!CM && x<abs(curx-x))) {
                (void) putchar('\r');
                curx = 1;
                nocmov(x, y);
        } else if(!CM) {
                nocmov(x, y);
        } else
                cmov(x, y);
}

nocmov(x, y)
{
        if (cury > y) {
                if(UP) {
                        while (cury > y) {      /* Go up. */
                                xputs(UP);
                                cury--;
                        }
                } else if(CM) {
                        cmov(x, y);
                } else if(HO) {
                        home();
                        curs(x, y);
                } /* else impossible("..."); */
        } else if (cury < y) {
                if(XD) {
                        while(cury < y) {
                                xputs(XD);
                                cury++;
                        }
                } else if(CM) {
                        cmov(x, y);
                } else {
                        while(cury < y) {
                                xputc('\n');
                                curx = 1;
                                cury++;
                        }
                }
        }
        if (curx < x) {         /* Go to the right. */
                if(!ND) cmov(x, y); else        /* bah */
                        /* should instead print what is there already */
                while (curx < x) {
                        xputs(ND);
                        curx++;
                }
        } else if (curx > x) {
                while (curx > x) {      /* Go to the left. */
                        xputs(BC);
                        curx--;
                }
        }
}

cmov(x, y)
register x, y;
{
        xputs(tgoto(CM, x-1, y-1));
        cury = y;
        curx = x;
}

xputc(c) char c; {
        (void) fputc(c, stdout);
}

xputs(s) char *s; {
        fputs(s, stdout);
}


/*	The tputs routine applies padding information to the string str and outputs it. The str must be a terminfo string variable
	or the return value from tparm, tgetstr, or tgoto. affcnt is the number of lines affected, or 1 if not applicable. 
	putc is a putchar-like routine to which the characters are passed, one at a time.
*/


cl_end() {
        if(CE)
                xputs(CE);
        else {  /* no-CE fix - free after Harold Rynes */
                /* this looks terrible, especially on a slow terminal
                   but is better than nothing */
                register cx = curx, cy = cury;

                while(curx < COLNO) {
                        xputc(' ');
                        curx++;
                }
                curs(cx, cy);
        }
}

clear_screen() {
        xputs(CL);
        curx = cury = 1;
}

home()
{
        if(HO)
                xputs(HO);
        else if(CM)
                xputs(tgoto(CM, 0, 0));
        else
                curs(1, 1);     /* using UP ... */
        curx = cury = 1;
}

standoutbeg()
{
        if(SO) xputs(SO);
}

standoutend()
{
        if(SE) xputs(SE);
}

backsp()
{
        xputs(BC);
        curx--;
}

bell()
{
        (void) putchar('\007');         /* curx does not change */
        (void) fflush(stdout);
}

static short tmspc10[] = {              /* from termcap */
        0, 2000, 1333, 909, 743, 666, 500, 333, 166, 83, 55, 41, 20, 10, 5
};

delay_output() {
        /* delay 50 ms - could also use a 'nap'-system call */
        /* BUG: if the padding character is visible, as it is on the 5620
           then this looks terrible. */

        /* simulate the delay with "cursor here" 5 times*/
        register i;
        for (i = 0; i < 5; i++)
                cmov(curx, cury);
}

cl_eos()                        /* free after Robert Viduya */
{                               /* must only be called with curx = 1 */

        if(CD)
                xputs(CD);
        else {
                register int cx = curx, cy = cury;
                while(cury <= LI-2) {
                        cl_end();
                        xputc('\n');
                        curx = 1;
                        cury++;
                }
                cl_end();
                curs(cx, cy);
        }
}

