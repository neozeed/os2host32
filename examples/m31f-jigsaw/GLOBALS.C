#include "jigsaw.h"

/*-------------------------- general definitions -----------------------------*/

HAB       habMain          = NULL;        /* main thread anchor block handle  */
HMQ	  hmqMain          = NULL;	  /* main thread queue handle	      */
HWND      hwndFrame        = NULL;        /* frame control handle             */
HWND	  hwndStatus       = NULL;	  /* status dialog handle	      */
HWND	  hwndClient       = NULL;	  /* client area handle		      */
HWND      hwndZoomScrollBar= NULL;        /* zoom scroll bar handle           */
HWND      hwndMenu         = NULL;        /* Menu handle                      */
HWND      hwndStatusOption = NULL;        /* status menu handle               */
HDC	  hdcClient        = NULL; 	  /* window dc handle		      */
HPS	  hpsClient        = NULL; 	  /* client area Gpi ps handle	      */
SIZEL	  sizlMaxClient;   		  /* max client area size	      */
HPS       hpsPaint         = NULL;        /* ps for use in Main Thread        */
HRGN	  hrgnInvalid      = NULL;	  /* handle to the invalid region     */

HAB	  habAsync         = NULL;	  /* async thread anchor block handle */
HMQ	  hmqAsync         = NULL;	  /* async thread queue handle	      */
TID       tidAsync;                       /* async thread id                  */
SEL	  selStack;        		  /* async thread stack selector      */
SHORT	  sPrty            = -1;	  /* async thread priority 	      */

HWND	  hwndHorzScroll   = NULL;	  /* horizontal scroll bar window     */
HWND	  hwndVertScroll   = NULL;	  /* vertical scroll bar window	      */

POINTS	  ptsScrollPos,
          ptsOldScrollPos;
POINTS	  ptsScrollMax,
          ptsHalfScrollMax;
POINTS	  ptsScrollLine;
POINTS	  ptsScrollPage;
 
MATRIXLF  matlfIdentity    = { UNITY, 0, 0, 0, UNITY, 0, 0, 0, 1 };
LONG	  lScale; 		          /* current zoom level		      */
POINTL	  ptlScaleRef;		          /* scalefactor, detects size change */
 
POINTL	  ptlOffset;
POINTL	  ptlBotLeft       = { 0, 0};
POINTL	  ptlTopRight      = { 3000, 3000};
POINTL	  ptlMoveStart;		       /* model space point at start of move  */
LONG	  lLastSegId;		       /* last segment id assigned to a piece */
LONG	  lPickedSeg       = 0L;       /* seg id of piece selected for drag   */
POINTL	  ptlOffStart;		       /* segment xform xlate at move start   */
RECTL	  rclBounds;		       /* pict bounding box in model coords.  */
POINTL	  ptlOldMouse      = {0L, 0L}; /* current mouse posn		      */
POINTL	  ptlMouse         = {0L, 0L}; /* current mouse posn		      */
BOOL	  fButtonDownMain  = FALSE;    /* only drag if mouse down	      */
BOOL	  fButtonDownAsync = FALSE;    /* only drag if mouse down	      */

POINTL	  ptlUpdtRef;
POINTL	  aptlUpdt[3];
BOOL	  fUpdtFirst;
BOOL	  fFirstLoad       = TRUE;

/*-------------------------- segment list ------------------------------------*/

PSEGLIST pslHead           = NULL;            /* head of the list             */
PSEGLIST pslTail           = NULL;            /* tail of the list             */
PSEGLIST pslPicked         = NULL;            /* picked segment's list member */
 
/*-------------------------- bitmap-related data -----------------------------*/

LOADINFO li;
PLOADINFO pli = &li;

HPS		   hpsBitmapFile      = NULL; /* bitmap straight from the
                                                 file                         */
HDC		   hdcBitmapFile      = NULL;
HBITMAP 	   hbmBitmapFile      = NULL;

BITMAPINFOHEADER2  bmp2BitmapFile;
PBITMAPINFOHEADER2 pbmp2BitmapFile    = &bmp2BitmapFile;
BITMAPINFOHEADER2  bmp2BitmapFileRef;
PBITMAPINFOHEADER2 pbmp2BitmapFileRef = &bmp2BitmapFileRef;

HPS		   hpsBitmapSize      = NULL; /* bitmap sized to the current
                                                 size                         */
HDC		   hdcBitmapSize      = NULL;
HBITMAP 	   hbmBitmapSize      = NULL;

HPS		   hpsBitmapBuff      = NULL; /* image composed here, copied to
                                                 scrn                         */
HDC		   hdcBitmapBuff      = NULL;
HBITMAP 	   hbmBitmapBuff      = NULL;

HPS		   hpsBitmapSave      = NULL; /* save part of screen during
                                                 dragging                     */
HDC		   hdcBitmapSave      = NULL;
HBITMAP 	   hbmBitmapSave      = NULL;

BITMAPINFOHEADER2  bmp2BitmapSave;
PBITMAPINFOHEADER2 pbmp2BitmapSave    = &bmp2BitmapSave;

DEVOPENSTRUC dop = { NULL
		     , "DISPLAY"
		     , NULL
		     , NULL
		     , NULL
		     , NULL
		     , NULL
		     , NULL
		     , NULL };


/*--------------------------- Miscellaneous ----------------------------------*/
 

HMTX    hmtxSzFmt;

CHAR	szFmt[50];		       /* buffer used by sprintf()	      */
CHAR    szZoomFact[10];

HEV     hevTerminate;
HEV     hevDrawOn;
HEV     hevMouse;
HEV     hevLoadMsg;
HEV     hevKillDraw;

PSZ     pszStartupMsg  = "Welcome to Jigsaw";
PSZ	pszLoadMsg     = "Patience, preparing puzzle pieces ...";
PSZ     pszError       = "Error reading file.";
PSZ	pszBlankMsg    = "";

SWCNTRL swctl       = { 0, 0, 0, 0, 0, SWL_VISIBLE, SWL_JUMPABLE, NULL, 0 };
HSWITCH hsw;			       /* handle to a switch list entry       */
char	szTitle[80];		       /* Title bar text		      */

BOOL	fErrMem     = FALSE;	       /* set if alloc async stack fails      */
 
LONG	lByteAlignX, lByteAlignY;      /* memory alignment constants	      */
 

 
