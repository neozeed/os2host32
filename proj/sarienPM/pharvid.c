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

char * gptr;	//pointer to vga ram

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

	screen_buffer = calloc (GFX_WIDTH, GFX_HEIGHT);
printf("GFX_WIDTH %d, GFX_HEIGHT %d\n",GFX_WIDTH, GFX_HEIGHT);

	r.h.ah = 0;
	r.h.al = 13;
	int86(0x10, &r, &r);

/* Get PM pointer to VGA screen */
	DosMapRealSeg(0xa000,64000,&rseg);
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


	/* set colors */
//	_setcolor(palette);
	outp(0x3c8, 0);
	for (i = 0; i < 32 * 3; i++)
		outp(0x3c9, palette[i]);


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
	unsigned int h, w, p;
#if 0
	if (x1 >= GFX_WIDTH)  x1 = GFX_WIDTH  - 1;
	if (y1 >= GFX_HEIGHT) y1 = GFX_HEIGHT - 1;
	if (x2 >= GFX_WIDTH)  x2 = GFX_WIDTH  - 1;
	if (y2 >= GFX_HEIGHT) y2 = GFX_HEIGHT - 1;

	h = y2 - y1 + 1;
	w = x2 - x1 + 1;
	p = GFX_WIDTH * y1 + x1;

	while (h--) {
		/* Turbo C wants 0xa0000000 in seg:ofs format */
		_fmemcpy ((UINT8 far *)gptr + p, screen_buffer + p, w);
		//memcpy(gptr+p,screen_buffer+p,w);
		p += 320;
	}
#else
	memcpy(gptr,screen_buffer,64000);
#endif
}


static void pc_put_pixels(int x, int y, int w, UINT8 *p)
{
	UINT8 *s;
 	for (s = &screen_buffer[y * 320 + x]; w--; *s++ = *p++);
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
	