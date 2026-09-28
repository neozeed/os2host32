/*  Sarien - A Sierra AGI resource interpreter engine
 *  Copyright (C) 1999-2001, 2007 Stuart George and Claudio Matsuoka
 *  
 *  $Id: sound.c,v 1.31 2001/09/17 02:26:42 cmatsuoka Exp $
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; see docs/COPYING for further details.
 */

#include <stdio.h>
#include <string.h>

#ifdef __TURBOC__
#include <dos.h>		/* For sound(), nosound() */
#endif

#include "sarien.h"
#include "agi.h"
#include "rand.h"

#define USE_INTERPOLATION
#define USE_CHORUS


/* TODO: add support for variable sampling rate in the output device
 */

#ifdef USE_IIGS_SOUND

/**
 * Sarien sound envelope structure.
 */
struct sound_envelope {
	UINT8 bp;
	UINT8 inc_hi;
	UINT8 inc_lo;
};

struct sound_wavelist {
	UINT8 top;
	UINT8 addr;
	UINT8 size;
	UINT8 mode;
	UINT8 rel_hi;
	UINT8 rel_lo;
};

struct sound_instrument {
	struct sound_envelope env[8];
	UINT8 relseg;
	UINT8 priority;
	UINT8 bendrange;
	UINT8 vibdepth;
	UINT8 vibspeed;
	UINT8 spare;
	UINT8 wac;
	UINT8 wbc;
	struct sound_wavelist wal[8];
	struct sound_wavelist wbl[8];
};

struct sound_iigs_sample {
	UINT8 type_lo;
	UINT8 type_hi;
	UINT8 srate_lo;
	UINT8 srate_hi;
	UINT16 unknown[2];
	UINT8 size_lo;
	UINT8 size_hi;
	UINT16 unknown2[13];
};

#if 0
static struct sound_instrument *instruments;
static int num_instruments;
static UINT8 *wave;
#endif

#endif

static int playing;
static struct channel_info chn[NUM_CHANNELS];
static int endflag = -1;
static int playing_sound = -1;
static UINT8 *song;
static UINT8 env;

struct sound_driver *snd;

extern struct sound_driver sound_dummy;

static void stop_note(int i);
static void play_note(int i, int freq, int len, int vol);
static int freq_to_note(int freq);

static int freq_to_note_table[128] = {           // index -> note; value -> freq*1000 | last 3 digits are the fractional part (8175 => 8.175 Hz)
    8175,     8661,     9177,     9722,    10300,    10913,    11562,    12249,
   12978,    13750,    14567,    15433,    16351,    17323,    18354,    19445,
   20601,    21826,    23124,    24499,    25956,    27500,    29135,    30867,
   32703,    34647,    36708,    38890,    41203,    43653,    46249,    48999,
   51913,    55000,    58270,    61735,    65406,    69295,    73416,    77781,
   82406,    87307,    92498,    97998,   103826,   110000,   116540,   123470,
  130812,   138591,   146832,   155563,   164813,   174614,   184997,   195997,
  207652,   220000,   233081,   246941,   261625,   277182,   293664,   311126,
  329627,   349228,   369994,   391995,   415304,   440000,   466163,   493883,
  523251,   554365,   587329,   622253,   659255,   698456,   739988,   783990,
  830609,   880000,   932327,   987766,  1046502,  1108730,  1174659,  1244507,
 1318510,  1396912,  1479977,  1567981,  1661218,  1760000,  1864655,  1975533,
 2093004,  2217461,  2349318,  2489015,  2637020,  2793825,  2959955,  3135963,
 3322437,  3520000,  3729310,  3951066,  4186009,  4434922,  4698636,  4978031,
 5274040,  5587651,  5919910,  6271926,  6644875,  7040000,  7458620,  7902132,
 8372018,  8869844,  9397272,  9956063, 10548081, 11175303, 11839821, 12543853
};

#ifdef USE_PCM_SOUND

SINT16 *snd_buffer;
static SINT16 *waveform;

static SINT16 waveform_ramp[WAVEFORM_SIZE] = {
       0,   8,  16,  24,  32,  40,  48,  56,
      64,  72,  80,  88,  96, 104, 112, 120,
     128, 136, 144, 152, 160, 168, 176, 184,
     192, 200, 208, 216, 224, 232, 240, 255,
       0,-248,-240,-232,-224,-216,-208,-200,
    -192,-184,-176,-168,-160,-152,-144,-136,
    -128,-120,-112,-104, -96, -88, -80, -72,
    -64, -56, -48, -40,  -32, -24, -16,  -8   	/* Ramp up */
};

static SINT16 waveform_square[WAVEFORM_SIZE] = {
     255, 230, 220, 220, 220, 220, 220, 220,
     220, 220, 220, 220, 220, 220, 220, 220,
     220, 220, 220, 220, 220, 220, 220, 220,
     220, 220, 220, 220, 220, 220, 220, 110,
    -255,-230,-220,-220,-220,-220,-220,-220,
    -220,-220,-220,-220,-220,-220,-220,-220,
    -220,-220,-220,-220,-220,-220,-220,-220,
    -220,-220,-220,-110,   0,   0,   0,   0   	/* Square */
};

static SINT16 waveform_mac[WAVEFORM_SIZE] = {
      45, 110, 135, 161, 167, 173, 175, 176,
     156, 137, 123, 110,  91,  72,  35,  -2,
     -60,-118,-142,-165,-170,-176,-177,-179,
    -177,-176,-164,-152,-117, -82, -17,  47,
      92, 137, 151, 166, 170, 173, 171, 169,
     151, 133, 116, 100,  72,  43,  -7, -57,
     -99,-141,-156,-170,-174,-177,-178,-179,
    -175,-172,-165,-159,-137,-114, -67, -19
};

#endif /* USE_PCM_SOUND */



#ifdef USE_IIGS_SOUND

static UINT16 period[] = {
	1024, 1085, 1149, 1218, 1290, 1367,
	1448, 1534, 1625, 1722, 1825, 1933
};

static struct agi_note play_sample[] = {
	{ 0xff, 0x7f, 0x18, 0x00, 0x7f },
	{ 0xff, 0xff, 0x00, 0x00, 0x00 },
	{ 0xff, 0xff, 0x00, 0x00, 0x00 },
	{ 0xff, 0xff, 0x00, 0x00, 0x00 }
};


static int note_to_period (int note)
{
	return 10 * (period[note % 12] >> (note / 12 - 3));
}

#endif /* USE_IIGS_SOUND */


void unload_sound (int resnum)
{
	if (game.dir_sound[resnum].flags & RES_LOADED) {
		if (game.sounds[resnum].flags & SOUND_PLAYING)
			/* FIXME: Stop playing */
			;

		/* Release RAW data for sound */
		free (game.sounds[resnum].rdata);
		game.sounds[resnum].rdata = NULL;
      		game.dir_sound[resnum].flags &= ~RES_LOADED;
	}
}


void decode_sound (int resnum)
{
#ifdef USE_IIGS_SOUND
	int type, size;
	SINT16 *buf;
	UINT8 *src;
	struct sound_iigs_sample *smp;

	_D ("(%d)", resnum);
	type = lohi_getword (game.sounds[resnum].rdata);

	if (type == AGI_SOUND_SAMPLE) {
		/* Convert sample data to 16 bit signed format
		 */
		smp = (struct sound_iigs_sample *)game.sounds[resnum].rdata;
		size = ((int)smp->size_hi << 8) + smp->size_lo;
		src = (UINT8 *)game.sounds[resnum].rdata;
		buf = calloc (1, 54 + (size << 1) + 100);	/* FIXME */
		memcpy (buf, src, 54);
		for (; size--; buf[size + 54] =
			((SINT16)src[size + 54] - 0x80) << 4); /* FIXME */
		game.sounds[resnum].rdata = (UINT8 *)buf;
		free (src);
	}
#endif /* USE_IIGS_SOUND */
}


void start_sound (int resnum, int flag)
{
	int i, type;
#ifdef USE_IIGS_SOUND
	struct sound_iigs_sample *smp;
#endif

	if (game.sounds[resnum].flags & SOUND_PLAYING)
		return;

	stop_sound ();

	if (game.sounds[resnum].rdata == NULL)
		return;

	type = lohi_getword (game.sounds[resnum].rdata);

	if (type != AGI_SOUND_SAMPLE &&
		type != AGI_SOUND_MIDI &&
		type != AGI_SOUND_4CHN)
		return;

	game.sounds[resnum].flags |= SOUND_PLAYING;
	game.sounds[resnum].type = type;
	playing_sound = resnum;
	song = (UINT8 *)game.sounds[resnum].rdata;

	switch (type) {
#ifdef USE_IIGS_SOUND
	case AGI_SOUND_SAMPLE:
		_D (_D_WARN "IIGS sample");
		smp = (struct sound_iigs_sample *)game.sounds[resnum].rdata;
		for (i = 0; i < NUM_CHANNELS; i++) {
			chn[i].type = type;
			chn[i].flags = 0;
			chn[i].ins = (SINT16 *)&game.sounds[resnum].rdata[54];
			chn[i].size = ((int)smp->size_hi << 8) + smp->size_lo;
			chn[i].ptr = &play_sample[i];
			chn[i].timer = 0;
			chn[i].vol = 0;
			chn[i].end = 0;
		}
		break;
	case AGI_SOUND_MIDI:
		_D (_D_WARN "IIGS MIDI sequence");

		for (i = 0; i < NUM_CHANNELS; i++) {
			chn[i].type = type;
			chn[i].flags = AGI_SOUND_LOOP | AGI_SOUND_ENVELOPE;
			chn[i].ins = waveform;
			chn[i].size = WAVEFORM_SIZE;
			chn[i].vol = 0;
			chn[i].end = 0;
		}

		chn[0].timer = *(song + 2);
		chn[0].ptr = (struct agi_note *)(song + 3);
		break;
#endif
	case AGI_SOUND_4CHN:
		/* Initialize channel info */
		for (i = 0; i < NUM_CHANNELS; i++) {
			chn[i].type = type;
			chn[i].flags = AGI_SOUND_LOOP;
			if (env) {
				chn[i].flags |= AGI_SOUND_ENVELOPE;
				chn[i].adsr = AGI_SOUND_ENV_ATTACK;
			}
#ifdef USE_PCM_SOUND
			chn[i].ins = waveform;
			chn[i].size = WAVEFORM_SIZE;
#endif
			chn[i].ptr = (struct agi_note *)(song +
				(song[i << 1] | (song[(i << 1) + 1] << 8)));
			chn[i].timer = 0;
			chn[i].vol = 0;
			chn[i].end = 0;
		}
		break;
	}

#ifdef USE_PCM_SOUND
	memset (snd_buffer, 0, BUFFER_SIZE << 1);
#endif
	endflag = flag;

	/* Nat Budin reports that the flag should be reset when sound starts
	 */
	setflag (endflag, FALSE);

	/* FIXME: should wait for sound time instead of setting the flag
	 *	  immediately
	 */
	if (opt.nosound) {
		setflag (endflag, TRUE);
		stop_sound ();
	}
}


void stop_sound ()
{
	int i;

	endflag = -1;
	for (i = 0; i < NUM_CHANNELS; i++)
		stop_note (i);

	if (playing_sound != -1) {
		game.sounds[playing_sound].flags &= ~SOUND_PLAYING;
		playing_sound = -1;
	}
}


int init_sound ()
{
	int r = -1;

#ifdef USE_PCM_SOUND
	snd_buffer = calloc (2, BUFFER_SIZE);
#endif

	__init_sound ();

	env = FALSE;

#ifdef USE_PCM_SOUND
	switch (opt.soundemu) {
	case SOUND_EMU_NONE:
		waveform = waveform_ramp;
		env = TRUE;
		break;
	case SOUND_EMU_AMIGA:
	case SOUND_EMU_PC:
		waveform = waveform_square;
		break;
	case SOUND_EMU_MAC:
		waveform = waveform_mac;
		break;
	}
#endif

	report ("Initializing sound:\n");

	if (opt.nosound)
		snd = &sound_dummy;

#ifdef USE_PCM_SOUND
	if ((r = snd->init (snd_buffer)) != 0) {
		snd = &sound_dummy;
		opt.nosound = TRUE;
	}
#endif

	report ("sound: %s configured\n", snd->description);

	report ("sound: envelopes ");
	if (env) {
		report ("enabled (decay=%d, sustain=%d)\n",
			ENV_DECAY, ENV_SUSTAIN);
	} else {
		report ("disabled\n");
	}

#ifdef USE_IIGS_SOUND
	/*load_instruments ("demo.sys");*/
#endif

	return r;
}


void deinit_sound (void)
{
	_D ("()");
	snd->deinit ();
#ifdef USE_PCM_SOUND
	free (snd_buffer);
#endif
}


static void stop_note (int i)
{
	chn[i].adsr = AGI_SOUND_ENV_RELEASE;
}


static void play_note (int i, int freq, int len, int vol)
{
if(i==0)
	SendBeep(freq,len);
/*
	chn[i].freq = freq;
	chn[i].vol = vol; 
	chn[i].env = 0x10000;
	chn[i].adsr = AGI_SOUND_ENV_ATTACK;
*/
}


int freq_to_note(int freq)
{
	int d = 0;
	int ld = 99999999;
	int res = -1;
	int i = 0;
	for (i = 0; i < 128; i++) {
		d = abs(freq_to_note_table[i] - (freq * 1000));
		if (d < ld) {
			ld = d;
			res = i;
		}
	}

	if (res == -1) {
		res = 0;
	}
	else {
		res = 127 - res + 35;
		if (res > 127)
			res = 127;
	}

	return res;
}


#ifdef USE_IIGS_SOUND

void play_midi_sound ()
{
	UINT8 *p;
	UINT8 parm1, parm2;
	static UINT8 cmd, ch;

	playing = 1;

	if (chn[0].timer > 0) {
		chn[0].timer -= 2;
		return;
	}

	p = (UINT8*)chn[0].ptr;

	if (*p & 0x80) {
		cmd = *p++;
		ch = cmd & 0x0f;
		cmd >>= 4;
	}

	switch (cmd) {
	case 0x08:
		parm1 = *p++;
		parm2 = *p++;
		if (ch < NUM_CHANNELS)
			stop_note (ch);
		break;
	case 0x09:
		parm1 = *p++;
		parm2 = *p++;
		if (ch < NUM_CHANNELS)
			play_note (ch, note_to_period (parm1), 127);
		break;
	case 0x0b:
		parm1 = *p++;
		parm2 = *p++;
		_D (_D_WARN "controller %02x, ch %02x, val %02x",
			parm1, ch, parm2);
		break;
	case 0x0c:
		parm1 = *p++;
#if 0
		if (ch < NUM_CHANNELS) {
			chn[ch].ins = (UINT16 *)&wave[waveaddr[parm1]];
			chn[ch].size = wavesize[parm1];
		}
		_D (_D_WARN "set patch %02x (%d,%d), ch %02x",
			parm1, waveaddr[parm1], wavesize[parm1], ch);
#endif
		break;
	}

	chn[0].timer = *p++;
	chn[0].ptr = (struct agi_note *)p;

	if (*p >= 0xfc) {
		_D (_D_WARN "end of sequence");
		playing = 0;
		return;
	}
}

void play_sample_sound ()
{
	play_note (0, 11025 * 10, 200);
	playing = 1;
}

#endif /* USE_IIGS_SOUND */


void play_agi_sound ()
{
	int i, freq;

//	for (playing = i = 0; i < (opt.soundemu == SOUND_EMU_PC ? 1 : 4); i++) {
	for (playing = i = 0; i < 4; i++) {
		playing |= !chn[i].end;

		if (chn[i].end)
			continue;

		if ((--chn[i].timer) <= 0) {
			stop_note (i);
			freq = ((chn[i].ptr->frq_0 & 0x3f) << 4)
				| (int)(chn[i].ptr->frq_1 & 0x0f);

			chn[i].timer = (((int)chn[i].ptr->dur_hi << 8) | chn[i].ptr->dur_lo);

			if (freq) {
				UINT8 v = chn[i].ptr->vol & 0x0f;
				if (chn[i].timer == 0xffff) {
					play_note (i, freq * 10, 300, v == 0xf ? 0 : 0xff - (v << 1));
				}
				else {
					play_note (i, freq * 10, (chn[i].timer / 3) * 50, v == 0xf ? 0 : 0xff - (v << 1));
				}
			}

			if (chn[i].timer == 0xffff) {
				chn[i].end = 1;
				chn[i].vol = 0;
				chn[i].env = 0;
			}

			chn[i].timer = chn[i].timer * 10 / 28;
			chn[i].ptr++;
		}
	}
}


void play_sound ()
{
	int i;

	if (endflag == -1)
		return;

		play_agi_sound ();

	if (!playing) {
		for (i = 0; i < NUM_CHANNELS; chn[i++].vol = 0);

		if (endflag != -1)
			setflag (endflag, TRUE);

		if (playing_sound != -1)
			game.sounds[playing_sound].flags &= ~SOUND_PLAYING;
		playing_sound = -1;
		endflag = -1;
	}
}



