/*  Sarien - A Sierra AGI resource interpreter engine
 *  Copyright (C) 1999-2001 Stuart George and Claudio Matsuoka
 *  
 *  $Id: pcvga.c,v 1.5 2002/11/16 01:20:21 cmatsuoka Exp $
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; see docs/COPYING for further details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <graph.h>
#include <dos.h>

#define INCL_DOSPROCESS
#include <phapi.h>

char *textptr;	//pointer to the text buffer
char textbuf[4000];

char * gptr;	//pointer to cga ram

REALPTR old_real_timer_tick;
PIHANDLER old_prot_timer_tick;
void _interrupt _far new_prot_timer_tick(REGS16);
#define IRQ0          0x08


#include "sarien.h"
#include "graphics.h"

extern struct gfx_driver *gfx;
extern struct sarien_options opt;

UINT8	*exec_name;
static UINT8	*screen_buffer;

static int	pc_init_vidmode	(void);
static int	pc_deinit_vidmode	(void);
static void	pc_put_block		(int, int, int, int);
static void	pc_put_pixels		(int, int, int, UINT8 *);
static void	pc_timer		(void);
static int	pc_get_key		(void);
static int	pc_keypress		(void);


#define TICK_SECONDS 18

static struct gfx_driver gfx_pcvga = {
	pc_init_vidmode,
	pc_deinit_vidmode,
	pc_put_block,
	pc_put_pixels,
	pc_timer,
	pc_keypress,
	pc_get_key
};

static UINT8 cga_map[16] = {
	0x00,		/*  0 - black */
	0x01,		/*  1 - blue */
	0x01,		/*  2 - green */
	0x01,		/*  3 - cyan */
	0x02,		/*  4 - red */
	0x02,		/*  5 - magenta */
	0x02,		/*  6 - brown */
	0x03,		/*  7 - gray */
	0x00,		/*  8 - dark gray */
	0x01,		/*  9 - light blue */
	0x01,		/* 10 - light green */
	0x01,		/* 11 - light cyan */
	0x02,		/* 12 - light red */
	0x02,		/* 13 - light magenta */
	0x02,		/* 14 - yellow */
	0x03		/* 15 - white */
};

static void pc_timer ()
{
	static UINT32 cticks = 0;
//printf("clockticks %d\n",clock_ticks);
	while (cticks == clock_ticks);
	cticks = clock_ticks;
}


int init_machine (int argc, char **argv)
{
  int rseg;
/* Get PM pointer to text screen */
  DosMapRealSeg(0xb800,4000,&rseg);
  textptr=MAKEP(rseg,0);

/* save text screen */
	memcpy(textbuf,textptr,4000);

	gfx = &gfx_pcvga;
printf("init_machine\n");
	return err_OK;
}


int deinit_machine ()
{
	union REGS r;

    /* restore old timer tick routines */
    DosSetRealProtVec(IRQ0, old_prot_timer_tick, old_real_timer_tick,NULL, NULL);

	memset(&r,0x0,sizeof(r));
	r.h.ah = 0;
	r.h.al = 3;
	int86(0x10, &r, &r);

	memcpy(textptr,textbuf,4000);

printf("deinit_machine\n");
	return err_OK;
}


static int pc_init_vidmode ()
{
	int i;
	union REGS r;
	int rseg;
	unsigned short _cs, _ds;

	clock_count = 0;
	clock_ticks = 0;

	screen_buffer = calloc (GFX_WIDTH / 4 , GFX_HEIGHT);
printf("GFX_WIDTH %d, GFX_HEIGHT %d\n",GFX_WIDTH/4, GFX_HEIGHT);

	opt.cgaemu = TRUE;

	memset (&r, 0x0, sizeof(union REGS));
	r.x.ax = 0x4;
	int86(0x10, &r, &r);

/* Get PM pointer to VGA screen */
	DosMapRealSeg(0xb800,16384,&rseg);
	gptr=MAKEP(rseg,0);


    /* VERY IMPORTANT -- Lock down our code and data segments
       because we don't want them paged out when running under
       Windows 3.0. */

    _asm	mov	_cs,cs
    _asm	mov	_ds,ds
    DosLockSegPages(_cs);
    DosLockSegPages(_ds);

    /* install our new timer tick routine */
    DosSetPassToProtVec(IRQ0, (PIHANDLER)new_prot_timer_tick,
        &old_prot_timer_tick, &old_real_timer_tick);

printf("pc_init_vidmode\n");
	return err_OK;
}


static int pc_deinit_vidmode ()
{
	free (screen_buffer);

	return err_OK;
}


/* blit a block onto the screen */
static void pc_put_block (int x1, int y1, int x2, int y2)
{
	unsigned int i, h, w, p, p2;
	UINT8 far *fbuffer;
	UINT8 *sbuffer;

	if (x1 >= GFX_WIDTH)  x1 = GFX_WIDTH  - 1;
	if (y1 >= GFX_HEIGHT) y1 = GFX_HEIGHT - 1;
	if (x2 >= GFX_WIDTH)  x2 = GFX_WIDTH  - 1;
	if (y2 >= GFX_HEIGHT) y2 = GFX_HEIGHT - 1;

	y1 &= ~1;			/* Always start at an even line */

	h = y2 - y1 + 1;
	w = (x2 - x1 + 1) / 4 + 1;
	p = 40 * y1 + x1 / 4;		/* Note: (GFX_WIDTH / 4) * (y1 / 2) */
	p2 = p + 40 * y1;

	/* Write to the interlaced CGA framebuffer */

	//fbuffer = (UINT8 far *)0xb8000000 + p;
	fbuffer = gptr+p;
	sbuffer = screen_buffer + p2;
	for (i = 0; i < h; i += 2) {
		_fmemcpy (fbuffer, sbuffer, w);
		fbuffer += 80;
		sbuffer += 160;
	}
#if 1
	//fbuffer = (UINT8 far *)0xb8002000 + p;
	fbuffer = gptr+p+0x2000;
	sbuffer = screen_buffer + p2 + 80;
	for (i = 1; i < h; i += 2) {
		_fmemcpy (fbuffer, sbuffer, w);
		fbuffer += 80;
		sbuffer += 160;
	}
#endif
}


static void pc_put_pixels(int x, int y, int w, UINT8 *p)
{
	UINT8 *s, mask, val, shift, c;

 	for (s = &screen_buffer[80 * y + x / 4]; w; w--, x++, p++) {
		shift = (x & 3) * 2;

		/* Sorry, no transparent colors */
		c = *p > 15 ? 0 : cga_map[*p];

		mask = 0xc0 >> shift;
		val = (c & 0x03) << (6 - shift);
		*s = (*s & ~mask) | val;
		
		if ((x % 4) == 3)
			s++;
	}
}


static int pc_keypress ()
{
	return !!kbhit();
}


static int pc_get_key ()
{
	union REGS r;
	UINT16 key;


	memset (&r, 0, sizeof(union REGS));
	int86 (0x16, &r, &r);
	key = r.h.al == 0 ? (r.h.ah << 8) : r.h.al;

	return key;
}


void interrupt far new_prot_timer_tick(REGS16 r)
{
	clock_ticks++;
    /* chain to the old interrupt */
    DosChainToRealIntr(old_real_timer_tick);
    /*NOTREACHED*/
}
	