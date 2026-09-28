/*  Sarien - A Sierra AGI resource interpreter engine
 *  Copyright (C) 1999-2001 Stuart George and Claudio Matsuoka
 *  
 *  $Id: pcvga.c,v 1.5 2002/11/16 01:20:21 cmatsuoka Exp $
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as pufblished by
 *  the Free Software Foundation; see docs/COPYING for further details.
 */

#define INCL_DOS
#define INCL_WIN
#define INCL_GPI
#define INCL_WINHEAP
#define INCL_WINDIALOGS
#define INCL_GPIPRIMITIVES
#define INCL_DOSPROCESS
#define INCL_DOSQUEUES
#define INCL_DOSERRORS

#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
//////////////////////////////////////////////////////////////
#define NUM_MASSES_X	320u
#define NUM_MASSES_Y	200u

HAB hab;
HAB habt;
HWND hwndFrame, hwndClient;
HDC hdc, hdcMemory;
HPS hps, hpsMemory;
HMTX hmtxLock;
TID tidMain;
TID tidTimer;
TID tidBeeper;

#define STACK            8192    /* Stack size for thread        */

BOOL ModelSuspended = FALSE;
HBITMAP hbm;
BITMAPINFOHEADER2 bmih;
BITMAPINFOHEADER2 bmp2data;
PBITMAPINFO2 pbmi;
BYTE RGBmap[32];
BYTE Bitmap[NUM_MASSES_X*NUM_MASSES_Y];
POINTL aptl[3] =
    { {0u, 0u}, {NUM_MASSES_X, NUM_MASSES_Y}, {0u, 0u} };

MRESULT EXPENTRY window_func(HWND, ULONG, MPARAM, MPARAM);
void Model(ULONG);
void PrepareGraphics(BYTE *);
void DisplayPlane(float **current);

#define CGA_00 0x000000
#define CGA_01 0x0000AA
#define CGA_02 0x00AA00
#define CGA_03 0x00AAAA
#define CGA_04 0xAA0000
#define CGA_05 0xAA00AA
#define CGA_06 0xAA5500
#define CGA_07 0xAAAAAA
#define CGA_08 0x555555
#define CGA_09 0x5555FF
#define CGA_10 0x55FF55
#define CGA_11 0x55FFFF
#define CGA_12 0xFF5555
#define CGA_13 0xFF55FF
#define CGA_14 0xFFFF55
#define CGA_15 0xFFFFFF

static unsigned char cga_map[16] = {
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

static unsigned int key_from;
static int pm_keypress;

static int draw_frame;
static int draw_frame_skip;

#define KEY_ENTER	0x0D
#define KEY_UP		0x4800
#define	KEY_DOWN	0x5000
#define KEY_LEFT	0x4B00
#define KEY_RIGHT	0x4D00
#define KEY_BACKSPACE	0x08
#define	KEY_ESCAPE	0x1B
#define KEY_ENTER	0x0D


PSZ szQueueName = "\\QUEUES\\SARIEN.QUE";
HQUEUE hqSpecialQue = 0;
#define QUE_CONVERT_ADDRESS 0x00000004
//////////////////////////////////////////////////////////////

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

typedef struct {
	int freq;
	int duration;
	}QueueBeep;


void Beeper(){

PID pidOwner=0;
REQUESTDATA Request	= {0};
//PSZ	DataBuffer 		= "";
int* DataBuffer;
BYTE	ElemPrty		= 0;
ULONG	ulDataLen	= 0;
int rc;
QueueBeep mybeep;

rc=DosOpenQueue(&pidOwner,&hqSpecialQue,szQueueName);
if(rc!=NO_ERROR)	{
		printf("Error with thread openeing Queue %s\n",szQueueName);
		exit(-1);
		}
for(;;)	{
	rc=DosReadQueue(hqSpecialQue,
		&Request,
		&ulDataLen,
		(PVOID) &DataBuffer,
		0L,
		DCWW_WAIT,
		&ElemPrty,
		0L);
	if(rc!=ERROR_QUE_EMPTY)	{

	mybeep.freq=(int)DataBuffer;
	*DataBuffer++;
	mybeep.duration=(int)DataBuffer;	//9	100
		DosBeep(mybeep.freq/7,mybeep.duration/100);
		}
//	DosSleep(32L);
	 }
}

void SendBeep(int freq,int duration)	{
QueueBeep mybeep;
mybeep.freq=freq;
mybeep.duration=duration;

DosWriteQueue (hqSpecialQue,
		12345L,
		sizeof(freq),
		freq,
		0L);
DosWriteQueue (hqSpecialQue,
		12345L,
		sizeof(duration),
		duration,
		0L);
}

void Timer(){
for(;;)   {
	DosSleep(20L);
	clock_ticks++;
	}
}

static void pc_timer ()
{
	static UINT32 cticks = 0;

	while (cticks == clock_ticks){DosSleep(5);}
	cticks = clock_ticks;
	//draw_frame++;
}


int init_machine ()	//(int argc, char **argv)
{
	gfx = &gfx_pcvga;
	opt.cgaemu = TRUE;
	return err_OK;
}


int deinit_machine ()
{
	return err_OK;
}


static int pc_init_vidmode ()
{
	int i;

	clock_count = 0;
	clock_ticks = 0;
	pm_keypress = 0;

	screen_buffer = calloc (GFX_WIDTH, GFX_HEIGHT);

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
  int x, y;
  int disp_val;

  for (y = 0; y < NUM_MASSES_Y; y++)
  {
    for (x = 0; x < GFX_WIDTH; x++)
    {
      disp_val = ((int) screen_buffer[y*GFX_WIDTH+x]);	//+ 16);
      if (disp_val > 32) disp_val = 32;
      else if (disp_val < 0) disp_val = 0;
      Bitmap[((NUM_MASSES_Y-y)*(GFX_WIDTH))-(GFX_WIDTH-x)] = RGBmap[disp_val];
    }
  }
	// skip frame happens here
	// for some reason it constantly stalls.
	// have to keep hitting enter :(
	//      if(draw_frame>draw_frame_skip) {

  DosRequestMutexSem(hmtxLock, SEM_INDEFINITE_WAIT);

  /* This is the key to the speed. Instead of doing a GPI call to set the
     color and a GPI call to set the pixel for EACH pixel, we get by
     with only two GPI calls. */
  GpiSetBitmapBits(hpsMemory, 0L, (LONG) (NUM_MASSES_Y-2), &Bitmap[0], pbmi);
  GpiBitBlt(hps, hpsMemory, 3L, aptl, ROP_SRCCOPY, BBO_AND);

  DosReleaseMutexSem(hmtxLock);
}


static void pc_put_pixels(int x, int y, int w, UINT8 *p)
{
  UINT8 *s;
  for (s = &screen_buffer[y * 320 + x]; w--; *s++ = *p++);
}


static int pc_keypress ()
{
return (pm_keypress);
}


static int pc_get_key ()
{
if(pm_keypress) {
	pm_keypress = 0;
	return key_from;
	}
else
return 0;
}


int OLDmain (int argc, char *argv[]);
//////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
  HMQ hmq;
  QMSG qmsg;
  ULONG flFlags;
  unsigned char class[]="MyClass";
  int rc;

  flFlags = FCF_TITLEBAR |
            FCF_MINBUTTON |
            FCF_TASKLIST |
            FCF_SYSMENU;

  if ((hab = WinInitialize(0)) == 0)
  {
    printf("Error doing WinInitialize()\n");
    exit(1);
  }
  if ((hmq = WinCreateMsgQueue(hab, 0)) == (HMQ) NULL)
  {
    printf("Error doing WinCreateMsgQueue()\n");
    exit(1);
  }
  if (!WinRegisterClass(hab, (PSZ) class, (PFNWP) window_func,
                        CS_SIZEREDRAW, 0))
  {
    printf("Error doing WinRegisterClass()\n");
    exit(1);
  }
  if ((hwndFrame = WinCreateStdWindow(HWND_DESKTOP, WS_VISIBLE, &flFlags,
                                  (PSZ) class, (PSZ) "Sarien 0.8.0-j2me",
                                  WS_VISIBLE, 0, 0, &hwndClient)) == 0)
  {
    printf("Error doing WinCreateStdWindow()\n");
    exit(1);
  }
  WinSetWindowPos(hwndFrame, 0L,
                  (SHORT) (60),
                  (SHORT) (WinQuerySysValue(HWND_DESKTOP,
				            SV_CYSCREEN) - (NUM_MASSES_Y+60)),
                  (SHORT) GFX_WIDTH,
		  (SHORT) NUM_MASSES_Y + WinQuerySysValue(HWND_DESKTOP,
			SV_CYTITLEBAR) - 2, SWP_SIZE | SWP_MOVE);

  /* Work out mapping from bitmap color table to our logical palette. */
  PrepareGraphics(RGBmap);

      /* Create a semaphore to control access to the memory image
         presentation space. Only one thread can perform Gpi operations
         on it at a time. */
      rc=DosCreateMutexSem("\\sem32\\Lock", &hmtxLock, 0, FALSE);
	if(rc!=NO_ERROR)	{
	printf("DosCreateMutexSem \\sem32\\Lock returned error\n");
	exit (-1);
	}

      DosCreateQueue(&hqSpecialQue,QUE_FIFO,szQueueName);
	if(rc!=NO_ERROR)	{
	printf("DosCreateQueue %s returned error\n",szQueueName);
	exit (-1);
	}

      /* Create a thread to run the system model. */
	DosCreateThread(&tidMain,OLDmain, 0UL, 0UL, STACK);
	DosCreateThread(&tidTimer,Timer, 0UL, 0UL, STACK);
	DosCreateThread(&tidBeeper,Beeper, 0UL, 0UL, STACK);

/*	draw_frame=1000;
	draw_frame_skip=2;
	if(argc>1)
		draw_frame_skip=atoi(argv[1]);	*/

  while (WinGetMsg(hab, &qmsg, (HWND) NULL, 0, 0))
  {
    WinDispatchMsg(hab, &qmsg);
  }
  WinDestroyWindow(hwndFrame);
  WinDestroyMsgQueue(hmq);
  WinTerminate(hab);
}
//////////////////////////////////////////////////////////////


MRESULT EXPENTRY
window_func(HWND handle, ULONG mess, MPARAM parm1, MPARAM parm2)
{
  int i;
  LONG j;
  SIZEL sizl;
  LONG OS2palette[33];


  switch(mess)
  {
    case WM_CREATE:
      /* Create presentation space for screen. */
      hdc = WinOpenWindowDC(handle);
      sizl.cx = 0;
      sizl.cy = 0;
      hps = GpiCreatePS(hab, hdc, &sizl,
		PU_PELS | GPIT_MICRO | GPIA_ASSOC | GPIF_DEFAULT);

      /* Create presentation space for memory image of screen. */
      hdcMemory = DevOpenDC(hab, OD_MEMORY, (PSZ) "*", 0L, 0L, 0L);
      sizl.cx = 0;
      sizl.cy = 0;
      hpsMemory = GpiCreatePS(hab, hdcMemory, &sizl,
		PU_PELS | GPIT_MICRO | GPIA_ASSOC | GPIF_DEFAULT);

      /* Create bitmap for memory image of screen. */
      memset(&bmih, 0, sizeof(bmih));
      bmih.cbFix = sizeof(bmih);
      bmih.cx = NUM_MASSES_X;
      bmih.cy = NUM_MASSES_Y;
      bmih.cPlanes = 1;
      bmih.cBitCount = 8;
      hbm = GpiCreateBitmap(hpsMemory, &bmih, 0L, NULL, NULL);
      GpiSetBitmap(hpsMemory, hbm);

      /* Set up gray-scale palette for screen image. */
      for (i = 0; i < 32; i++)
      {
        j = i << 3;
        OS2palette[i] = CGA_00;	//(j << 16) | (j << 8) | j;
      }
	OS2palette[0]=CGA_00; 
	OS2palette[1]=CGA_11;
	OS2palette[2]=CGA_11;
	OS2palette[3]=CGA_11;
	OS2palette[4]=CGA_13;
	OS2palette[5]=CGA_13;
	OS2palette[6]=CGA_13;
	OS2palette[7]=CGA_15;
	OS2palette[8]=CGA_00; 
	OS2palette[9]=CGA_11;
	OS2palette[10]=CGA_11;
	OS2palette[11]=CGA_11;
	OS2palette[12]=CGA_13;
	OS2palette[13]=CGA_13;
	OS2palette[14]=CGA_13;
	OS2palette[15]=CGA_15;

        OS2palette[16]=CGA_00;
        OS2palette[17]=CGA_11;
        OS2palette[18]=CGA_11;
        OS2palette[19]=CGA_11;
        OS2palette[20]=CGA_13;
        OS2palette[21]=CGA_13;
        OS2palette[22]=CGA_13;
        OS2palette[23]=CGA_15;
        OS2palette[24]=CGA_00;
        OS2palette[25]=CGA_11;
        OS2palette[26]=CGA_11;
        OS2palette[27]=CGA_11;
        OS2palette[28]=CGA_13;
        OS2palette[29]=CGA_13;
        OS2palette[30]=CGA_13;
        OS2palette[31]=CGA_15;

        OS2palette[32] = 0x00ffffffL;


      GpiCreateLogColorTable(hpsMemory, (ULONG) LCOL_PURECOLOR,
		(LONG) LCOLF_CONSECRGB, (LONG) 0L, (LONG) 33L, (PLONG) OS2palette);
      GpiSetBackMix(hpsMemory, BM_OVERPAINT);

      /* Take the input focus. */
      WinFocusChange(HWND_DESKTOP, handle, 0L);
      break;

    case WM_ERASEBACKGROUND:
    case WM_PAINT:
      /* Copy the memory image of the screen out to the real screen. */
	//This gets called when you move the window, or something pops up
         DosRequestMutexSem(hmtxLock, SEM_INDEFINITE_WAIT);
         WinBeginPaint(handle, hps, NULL);
         GpiBitBlt(hps, hpsMemory, 3L, aptl, ROP_SRCCOPY, BBO_AND);
         WinEndPaint(hps);
         DosReleaseMutexSem(hmtxLock);
      break;

    case WM_CHAR:
      if (SHORT1FROMMP(parm1) & KC_KEYUP)
        break;
pm_keypress=1;

      switch (SHORT2FROMMP(parm2))
      {

      	case VK_LEFT:
	key_from=KEY_LEFT;
	break;
	case VK_RIGHT:
	key_from=KEY_RIGHT;
	break;
	case VK_UP:
	key_from=KEY_UP;
	break;
	case VK_DOWN:
	key_from=KEY_DOWN;
	break;
	case VK_ESC:
	key_from=KEY_ESCAPE;
	break;

	default:
	key_from=SHORT1FROMMP(parm2);
	break;

      }
      break;
    default:
      return WinDefWindowProc(handle, mess, parm1, parm2);
  }
  return (MRESULT) FALSE;
}


void
PrepareGraphics(BYTE *RGBmap)
{
  POINTL coords;
  int x, y;

  /* Give thread access to Gpi. */
  habt = WinInitialize(0);

  /* Determine mapping from logical color value to bitmap color table
     index.  Anybody know a more direct way??? */
  DosRequestMutexSem(hmtxLock, SEM_INDEFINITE_WAIT);
  for (x = 0, y = 0; x < 33; x++)
  {
      GpiSetColor(hpsMemory, (LONG) x);
      coords.x = x;
      coords.y = y;
      GpiSetPel(hpsMemory, &coords);
  }
  bmp2data.cbFix = 16L;
  GpiQueryBitmapInfoHeader(hbm, &bmp2data);
  DosAllocMem((PPVOID)&pbmi, sizeof(BITMAPINFO2) +
		(sizeof(RGB2) * (1 << bmp2data.cPlanes) *
                (1 << bmp2data.cBitCount)),
                PAG_COMMIT | PAG_READ | PAG_WRITE);
  pbmi->cbFix = bmp2data.cbFix;
  pbmi->cx = bmp2data.cx;
  pbmi->cy = bmp2data.cy;
  pbmi->cPlanes = bmp2data.cPlanes;
  pbmi->cBitCount = bmp2data.cBitCount;
  pbmi->ulCompression = bmp2data.ulCompression;
  pbmi->cbImage = bmp2data.cbImage;
  pbmi->cxResolution = bmp2data.cxResolution;
  pbmi->cyResolution = bmp2data.cyResolution;
  pbmi->cclrUsed = bmp2data.cclrUsed;
  pbmi->cclrImportant = bmp2data.cclrImportant;
  pbmi->usUnits = bmp2data.usUnits;
  pbmi->usReserved = bmp2data.usReserved;
  pbmi->usRecording = bmp2data.usRecording;
  pbmi->usRendering = bmp2data.usRendering;
  pbmi->cSize1 = bmp2data.cSize1;
  pbmi->cSize2 = bmp2data.cSize2;
  pbmi->ulColorEncoding = bmp2data.ulColorEncoding;
  pbmi->ulIdentifier = bmp2data.ulIdentifier;
  GpiQueryBitmapBits(hpsMemory, 0L, NUM_MASSES_Y-2, &Bitmap[0], pbmi);
  DosReleaseMutexSem(hmtxLock);
  for (x = 0; x < 33; x++)
  {
    RGBmap[x] = Bitmap[x];
  }
}

	