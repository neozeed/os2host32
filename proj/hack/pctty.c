/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* tty.c - version 1.0.3 */
/* With thanks to the people who sent code for SYSV - hpscdi!jon,
   arnold@ucsf-cgl, wcs@bo95b, cbcephus!pds and others. */

#include <stdio.h>
#ifdef OS2
#define INCL_16
#define INCL_KBD
#define INCL_DOSPROCESS

/*	not in the 6.78 sdk?!	*/
#ifndef KEYBOARD_ECHO_OFF
#define KEYBOARD_ECHO_OFF             0x0002
#define KEYBOARD_ASCII_MODE           0x0008
#endif

#include <os2.h>
#else
#ifndef QDOS
#include <conio.h>
#endif
#endif
#include "hack.h"

static char erase_char, kill_char;
static boolean settty_needed = FALSE;

/*
 * Some systems may have getchar() return EOF for various reasons, and
 * we should not quit before seeing at least NR_OF_EOFS consecutive EOFs.
 */
#define	NR_OF_EOFS	20

#ifdef OS2
static KBDINFO os2_saved_kbd;
static int os2_have_saved_kbd = 0;

/*
 * Return one translated OS/2 keyboard character.  Hack is a character-key
 * game, so pure extended keys are ignored.  OS/2 reports Enter as CR while
 * Hack's historical tty code consistently tests for '\n'.
 */
static int
os2_getch()
{
	KBDKEYINFO key;
	USHORT rc;

	for(;;) {
		rc = KbdCharIn(&key, IO_WAIT, 0);
		if(rc)
			return(EOF);

		if(key.chChar == 0 || key.chChar == 0xe0)
			continue;

		if(key.chChar == '\r')
			return('\n');

		return((int)key.chChar);
	}
}

#define HACK_GETCH() os2_getch()
#define HACK_GETCHAR() os2_getch()
#else
#define HACK_GETCH() getch()
#define HACK_GETCHAR() getchar()
#endif



/*
 * Get initial state of terminal, set ospeed (for termcap routines)
 * and switch off tab expansion if necessary.
 * Called by startup() in termcap.c and after returning from ! or ^Z
 */
gettty(){
#ifdef OS2
	os2_saved_kbd.cb = sizeof(os2_saved_kbd);
	if(KbdGetStatus(&os2_saved_kbd, 0) == 0)
		os2_have_saved_kbd = 1;
#endif
	erase_char = '\b';
	kill_char = 21;		/* cntl-U */
	settty_needed = TRUE;
}

/* reset terminal to original state */
settty(s) char *s; {
	end_screen();
	clear_screen();
	if(s)
	   printf(s);
	fflush(stdout);
#ifdef OS2
	if(os2_have_saved_kbd)
		(void) KbdSetStatus(&os2_saved_kbd, 0);
#endif

	flags.echo = ON;
	flags.cbreak = OFF;
}

setctty(){
}


setftty(){
#ifdef OS2
	KBDINFO kbd;

	kbd.cb = sizeof(kbd);
	if(KbdGetStatus(&kbd, 0) == 0) {
		/* Low nibble selects echo and ASCII/binary input modes. */
		kbd.fsMask = (kbd.fsMask & 0xfff0) |
			KEYBOARD_ECHO_OFF | KEYBOARD_ASCII_MODE;
		(void) KbdSetStatus(&kbd, 0);
	}
#endif
	flags.cbreak = ON;
	flags.echo = OFF;
	start_screen();
}


/* fatal error */
/*VARARGS1*/
error(s,x,y) char *s; {
	if(settty_needed)
		settty((char *) 0);
	printf(s,x,y);
	putchar('\n');
	exit(1);
}

/*
 * Read a line closed with '\n' into the array char bufp[BUFSZ].
 * (The '\n' is not stored. The string is closed with a '\0'.)
 * Reading can be interrupted by an escape ('\033') - now the
 * resulting string is "\033".
 */
getlin(bufp)
register char *bufp;
{
	register char *obufp = bufp;
	register int c;

	flags.toplin = 2;		/* nonempty, no --More-- required */
	for(;;) {
		(void) fflush(stdout);
		if((c = HACK_GETCHAR()) == EOF) {
			*bufp = 0;
			return;
		}
		if(c == '\033') {
			*obufp = c;
			obufp[1] = 0;
			return;
		}
		if(c == erase_char || c == '\b') {
			if(bufp != obufp) {
				bufp--;
				putstr("\b \b"); /* putsym converts \b */
			} else	bell();
		} else if(c == '\n') {
			*bufp = 0;
			return;
		} else if(' ' <= c && c < '\177') {
				/* avoid isprint() - some people don't have it
				   ' ' is not always a printing char */
			*bufp = c;
			bufp[1] = 0;
			putstr(bufp);
			if(bufp-obufp < BUFSZ-1 && bufp-obufp < COLNO)
				bufp++;
		} else if(c == kill_char || c == '\177') { /* Robert Viduya */
				/* this test last - @ might be the kill_char */
			while(bufp != obufp) {
				bufp--;
				putstr("\b \b");
			}
		} else
			bell();
	}
}

getret() {
	cgetret("");
}

cgetret(s)
register char *s;
{
	putsym('\n');
	if(flags.standout)
		standoutbeg();
	putstr("Hit ");
	putstr(flags.cbreak ? "space" : "return");
	putstr(" to continue: ");
	if(flags.standout)
		standoutend();
	xwaitforspace(s);
}

char morc;	/* tell the outside world what char he used */

xwaitforspace(s)
register char *s;	/* chars allowed besides space or return */
{
register int c;

	morc = 0;
	while((c = readchar()) != '\n') {
	    if(flags.cbreak) {
		if(c == ' ') break;
		if(s && index(s,c)) {
			morc = c;
			break;
		}
		bell();
	    }
	}
}

char *
parse()
{
	static char Hinline[COLNO];
	register foo;

	flags.move = 1;
	if(!Invisible) curs_on_u(); else home();
#ifdef DGK
	/* provide feedback when the multi count gets large -dgk */
	while((foo = readchar()) >= '0' && foo <= '9') {
		multi = 10*multi+foo-'0';
		if (multi >= 10) {
			remember_topl();
			home();
			cl_end();
			printf("count: %d", multi);
		}
	}
#else
	while((foo = readchar()) >= '0' && foo <= '9')
		multi = 10*multi+foo-'0';
#endif DGK
	if(multi) {
		multi--;
		save_cm = Hinline;
	}
	Hinline[0] = foo;
	Hinline[1] = 0;
	if(foo == 'f' || foo == 'F'){
		Hinline[1] = HACK_GETCHAR();
#ifdef QUEST
		if(Hinline[1] == foo) Hinline[2] = HACK_GETCHAR(); else
#endif QUEST
		Hinline[2] = 0;
	}
	if(foo == 'm' || foo == 'M'){
		Hinline[1] = HACK_GETCHAR();
		Hinline[2] = 0;
	}
	clrlin();
	return(Hinline);
}

char
readchar() {
	char sym;

	(void) fflush(stdout);

	if((sym = HACK_GETCH()) == EOF) /*	^Z or -1	*/
		end_of_input();

	if(flags.toplin == 1)
		flags.toplin = 2;
	return((char) sym);
}

end_of_input()
{
	settty("End of input?\n");
	clearlocks();
	exit(0);
}
