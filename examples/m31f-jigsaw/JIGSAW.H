#define INCL_BITMAPFILEFORMAT
 
#define INCL_DOSMEMMGR
#define INCL_DOSINFOSEG
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES

#define INCL_DEV
#define INCL_BASE

#define INCL_WINSYS
#define INCL_WININPUT
#define INCL_WINMENUS
#define INCL_WINDIALOGS
#define INCL_WINFRAMEMGR
#define INCL_WINPOINTERS
#define INCL_WINWINDOWMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINRECTANGLES
#define INCL_WINSCROLLBARS
#define INCL_WINSWITCHLIST
#define INCL_WINENTRYFIELDS


#define INCL_GPIPATHS
#define INCL_GPILCIDS
#define INCL_GPIREGIONS
#define INCL_GPICONTROL
#define INCL_GPIBITMAPS
#define INCL_GPIPRIMITIVES
#define INCL_GPITRANSFORMS
 
#define INCL_ERRORS

#include <os2.h>

#define CALLBACK EXPENTRY

#define APPMENU             1   /* app menu */
#define APPTABLE            1   /* app accel table */
#define APPICON             1

#define SM_EXIT             1   /* menu item ids */
#define SM_ZOOM             10
#define SM_HELP             20
#define SM_FILE 	    30
#define MENU_RESUME         2
#define MENU_EXIT           3
#define MENU_PRINT          11
#define MENU_STATUS         12
#define MENU_SHOW           13
#define MENU_HIDE           14
#define MENU_ZOOMIN         15
#define MENU_ZOOMOUT	    16
#define MENU_BYTEALIGN	    17
#define MENU_HELP_EXTENDED  21
#define MENU_HELP_INDEX     22
#define MENU_HELP_KEYS	    23
#define MENU_LOAD           31
#define MENU_JUMBLE	    40
#define MENU_DUMMY          9999
 
 
#define TITLEBAR            1    /* string table ids */
#define TERMINATE           2
#define XFORMFAIL           3
#define HELP_EXTENDED       4
#define HELP_INDEX          5
#define HELP_KEYS           6

#define IDD_OPEN            256
#define IDD_STATUS          257
#define IDD_NULL            0

#define EID_NAME            258

#define SID_ZOOMFACT        259
#define SID_STATUS          260

#define HID_ZOOM            261

#define ADD_HEAD_SEG        1
#define ADD_TAIL_SEG        2
#define DEL_SEG             3
#define MAKE_TAIL_SEG       4
#define MAKE_HEAD_SEG       5
#define PICTURE_CREATE	    0
#define PICTURE_UPDATE	    1
#define ZOOM_MAX            8
#define ZOOM_IN             1
#define ZOOM_OUT            -1
#define UNITY	            65536L
#define STACKSIZE           4096 	  /* async thread stack size	      */


/*----------------------- helper macros --------------------------------------*/

#define ROUND_DOWN_MOD( arg, mod)	    \
    {					    \
	if( arg < 0)			    \
	{				    \
	    arg -= mod - 1;		    \
	    arg += arg % mod;		    \
	} else				    \
	    arg -= arg % mod;		    \
    }

#define ROUND_UP_MOD( arg, mod) 	    \
    {					    \
	if( arg < 0)			    \
	{				    \
	    arg -= arg % mod;		    \
	} else				    \
	{				    \
	    arg += mod - 1;		    \
	    arg -= arg % mod;		    \
	}				    \
    }

/*
 * macro will convert BITMAPFILEHEADER2 to BITMAPFILEHEADER
 */
#define PBFH1(A)           ((PBITMAPFILEHEADER) (A))
/*
 * macro will convert BITMAPINFOHEADER2 to BITMAPINFOHEADER
 */
#define PBMP1(A)           ((PBITMAPINFOHEADER) (A))

/* macro will adjust structure type depending on wiether old or new
 * BITMAPINFOHEADER is read in from a BMP file.
 */
#define ADJUSTED_PBMP(A)    (((A)->cbFix == sizeof(BITMAPINFOHEADER)) \
                                    ? PBMP1(A) : (PVOID)(A))




#define STATUS_SHOW(A)                                           \
{                                                                \
    if (A) {                                                     \
        WinSendMsg( hwndStatusOption,                            \
                    MM_SETITEMATTR,                              \
                    MPFROM2SHORT( MENU_SHOW, TRUE),              \
                    MPFROM2SHORT( MIA_DISABLED, ~MIA_DISABLED)); \
        }                                                        \
    else {                                                       \
        WinSendMsg( hwndStatusOption,                            \
                    MM_SETITEMATTR,                              \
                    MPFROM2SHORT( MENU_SHOW, TRUE),              \
                    MPFROM2SHORT( MIA_DISABLED, MIA_DISABLED));  \
        }                                                        \
}


#define STATUS_HIDE(A)                                           \
{                                                                \
    if (A) {                                                     \
        WinSendMsg( hwndStatusOption,                            \
                    MM_SETITEMATTR,                              \
                    MPFROM2SHORT( MENU_HIDE, TRUE),              \
                    MPFROM2SHORT( MIA_DISABLED, ~MIA_DISABLED)); \
        }                                                        \
    else {                                                       \
        WinSendMsg( hwndStatusOption,                            \
                    MM_SETITEMATTR,                              \
                    MPFROM2SHORT( MENU_HIDE, TRUE),              \
                    MPFROM2SHORT( MIA_DISABLED, MIA_DISABLED));  \
        }                                                        \
}

/*----------------------- inter-thread messages ------------------------------*/
 
#define UM_DIE	      WM_USER+1        /* instruct async thread to terminate  */
#define UM_DRAW       WM_USER+2        /* draw the current picture	      */
#define UM_VSCROLL    WM_USER+3        /* perform scroll by recalculating the */
                                       /* default viewing transform           */
#define UM_HSCROLL    WM_USER+4        /* perform scroll by recalculating the */
                                       /* default viewing transform           */
#define UM_SIZING     WM_USER+5        /* perform sizing by recalculating the */
                                       /* default viewing transform           */
#define UM_ZOOM       WM_USER+6        /* zoom the picture by recalculating   */
                                       /* the default viewing transform       */
#define UM_REDRAW     WM_USER+8        /* redraw the entire client window     */
#define UM_JUMBLE     WM_USER+9        /* scatter the pieces on the window    */
#define UM_LOAD       WM_USER+10       /* load a bitmap from disk	      */
#define UM_LEFTDOWN   WM_USER+11       /* correlate and prepare to drag       */
#define UM_MOUSEMOVE  WM_USER+12       /* remove, reposition, and redraw      */
#define UM_LEFTUP     WM_USER+13       /* stop dragging 		      */
#define UM_CHAR       WM_USER+14       /* a keyboard key		      */
#define UM_LAST       WM_USER+15
 
 
/*------------------------- correlation parameters ---------------------------*/
 
#define HITS	1L		       /* maximum number of hits to return    */
#define DEPTH	2L		       /* max depth of seg calls to return    */
 
/*-------------------------- old-style bitmap header -------------------------*/

typedef struct {
    USHORT    wType;
    ULONG     dwSize;
    int       xHotspot;
    int       yHotspot;
    ULONG     dwBitsOffset;
    USHORT    bmWidth;
    USHORT    bmHeight;
    USHORT    bmPlanes;
    USHORT    bmBitcount;
} RCBITMAP;
typedef RCBITMAP FAR *PRCBITMAP;

/*-------------------------- bitmap-related data -----------------------------*/
typedef struct _LOADINFO {	       /* li				      */
    HFILE   hf;
    CHAR    szFileName[CCHMAXPATH];
} LOADINFO, FAR *PLOADINFO;       /* pli				      */



/*-------------------------- segment list ------------------------------------*/

typedef struct _SEGLIST {	       /* sl				      */
  LONG			lSegId;
  struct _SEGLIST FAR * pslPrev;
  struct _SEGLIST FAR * pslNext;
  POINTL		ptlLocation;   /* piece location, world coordinates   */
  RECTL 		rclCurrent;    /* segment bounding box, model coords  */
  RECTL 		rclBitBlt;     /* segment bounding box, world coords  */
  POINTL		aptlBitBlt[4]; /* BitBlt parameters		      */
  LONG			lAdjacent[8];  /* adjacent segments in original pict  */
  struct _SEGLIST FAR * pslNextIsland; /* next member in island list	      */
  BOOL			fIslandMark;   /* used for marking island members     */
  POINTL		aptlSides[12]; /* control points for piece outline    */
  HDC			hdcHole;       /* used to punch hole in target	      */
  HPS			hpsHole;
  HBITMAP		hbmHole;
  HDC			hdcFill;       /* used to fill hole		      */
  HPS			hpsFill;
  HBITMAP		hbmFill;
  POINTL		ptlModelXlate; /* lM31 and lM31 for piece	      */
  BOOL			fVisible;      /* TRUE iff should be drawn	      */
} SEGLIST ;
typedef SEGLIST FAR *PSEGLIST;	       /* psl				      */
typedef PSEGLIST FAR *PPSEGLIST;       /* ppsl				      */


/*------------------------- Function Prototypes ------------------------------*/

extern VOID	         CalcBounds( VOID);
extern VOID              CalcSize( MPARAM mp1, MPARAM mp2);
extern VOID              CalcTransform( HWND hwnd);
extern VOID              CheckPsl( PSEGLIST psl);
extern MRESULT EXPENTRY  ClientWndProc( HWND hwnd, USHORT msg, MPARAM mp1,
                                 MPARAM  mp2);
extern PSEGLIST          Correlate( PPOINTL pptl);
extern BOOL              CreateBitmapHdcHps( PHDC phdc, PHPS phps);
extern BOOL              CreatePicture( SHORT sUpdate);
extern BOOL              DoDraw( HPS hps, HRGN hrgn, BOOL fPaint);
extern VOID              DoHorzScroll( VOID);
extern VOID              DoVertScroll( VOID);
extern VOID              DrawPiece( HPS hps, PSEGLIST psl, BOOL fFill);
extern BOOL              DumpPicture( VOID);
extern VOID              Finalize( VOID);
extern BOOL              Initialize( VOID);
extern VOID              Jumble( VOID);
extern VOID              LeftDown( MPARAM mp);
extern VOID              LeftUp( VOID);
extern VOID              Load( PLOADINFO pli);
extern VOID cdecl        main( VOID);
extern VOID              MarkIsland( PSEGLIST pslMark, BOOL fMark);
/*
VOID              MessageInt( HWND, INT, PCH);
*/
extern VOID              MouseMove( MPARAM mp);
extern VOID              MyMessageBox( HWND hWnd, PSZ psz);
extern VOID FAR          NewThread( VOID);
extern BOOL              PrepareBitmap( VOID);
extern BOOL              ReadBitmap( HFILE hfile);
extern VOID              Redraw( VOID);
extern VOID              ReportError( HAB hab);
extern VOID              ResetScrollBars( VOID);
extern BOOL              SegListCheck( INT iLoc);
extern PSEGLIST          SegListGet( LONG lSeg);
extern PSEGLIST          SegListUpdate( USHORT usOperation, PSEGLIST pslUpdate);
extern BOOL              SendCommand( USHORT usCommand, MPARAM mp1, MPARAM mp2);
extern VOID              SetDVTransform( FIXED fx11, FIXED fx12, FIXED fx21,
                                 FIXED fx22,
                         FIXED l31,FIXED l32, FIXED lType);
extern VOID              SetRect( PSEGLIST psl);
extern VOID              ToBottom( PSEGLIST pslDown);
extern VOID              ToggleMenuItem( USHORT usMenuMajor, USHORT usMenuMinor,
                         PBOOL pfFlag);
extern VOID              DisplayZoomFactor(LONG lScaleFactor);
extern MRESULT           WndProcCommand( HWND hwnd, USHORT msg, MPARAM mp1,
                                 MPARAM mp2);
extern MRESULT           WndProcCreate( HWND hwnd);
extern MRESULT           WndProcPaint( VOID);
/*
MRESULT           WndProcSize( MPARAM, MPARAM);
*/
extern VOID              Zoom(VOID);
extern MRESULT EXPENTRY  CheapWndProc(HWND hwnd, USHORT  msg, MPARAM  mp1,
                                 MPARAM mp2);
extern MRESULT EXPENTRY  CheapWndProc(HWND hwnd, USHORT msg, MPARAM mp1,
                                 MPARAM mp2);
extern MRESULT EXPENTRY  StatusDlgProc(HWND hwnd, USHORT msg, MPARAM mp1,
                                 MPARAM mp2);
