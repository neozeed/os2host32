/* wndproc.c -- Window handling routines
   Created by Microsoft, IBM Corporation, 1990
*/
#define INCL_GPI
#define INCL_WIN
#include <os2.h>

#include "bio.h"
#include <time.h>
#include <stdio.h>

/* Read-only global variables */
extern HAB      hAB;
extern HWND     hwndApp;
extern HWND     hwndAppFrame, hwndKidFrame;
extern char     szAppName[];
extern PFNWP OldFrameWndProc;

/* Write-once Global variables */
LONG Color[] = {CLR_RED, CLR_GREEN, CLR_BLUE};
FONTMETRICS     tmFontInfo;
SHORT		cxLegendField;
SHORT		cxDateField;
/* parameter used when creating a device context for a memory device        */
PSZ         dcdatablk[9] = {(PSZ)0
                           ,(PSZ)"DISPLAY"
                           ,(PSZ)0
                           ,(PSZ)0
                           ,(PSZ)0
                           ,(PSZ)0
                           ,(PSZ)0
                           ,(PSZ)0
                           ,(PSZ)0
			   };

/* Read-Write global variables */
double          Born;
long            Day, SelectDay;
BOOL            bKid = TRUE;
BOOL            bBorn = TRUE;
RECTL           rclClient;
SHORT		LinesPerPage;

/*  BioWndProc() - Parent WndProc message processing routine.
*
*   Purpose:
*       WndProc callback function to handle all messages for parent window.
*
*   Arguments:
*       hWnd          - Handle of Window owning message
*       message       - Message itself
*       mp1           - Extra message-dependent info
*       mp2           - Extra message-dependent info
*
*   Globals (modified):
*       Born          - Birthdate in julian days.  Read from WIN.INI.
*	SelectDay     - Current day being tracked, day is highlighted.
*			Value is days from birthdate.
*			Initialized to current day in WM_CREATE processing.
*       daylight      - Defined by environment string TZ.  If no such string,
*       timezone        default TZ=PST8PDT used.  If daylight field is
*                       used, daylight time correction will occur.  See
*                       documentation of tzset() C run-time function.
*       Day           - Day number from date born which is top line being
*                       displayed.  Initially three days before SelectDay.
*       bKid          - Boolean indicating whether legend is visible.
*       bBorn         - Boolean indicating whether valid birtdate entered or
*                       defined in WIN.INI.  Nothing graphed until valid.
*	rclClient     - Size of client area defined by WM_SIZE message
*       LinesPerPage  - Number of system font lines on client area, defined
*                       by WM_SIZE message handling
*       Color[]       - Set of colored pens used to identify cycles.
*       tmFontInfo    - Text Metric structure defined during WM_CREATE 
*
*   Globals (referenced):
*       hAB           - Handle to the Anchor Block
*       hwndAppFrame  - Window handle of parent window's frame
*       hwndKidFrame  - Handle to child window used for showing/moving legend.
*	szAppName[]   - RC file program name (Biorhythm).
*
*   Description:
*       Handle all messages for the parent window.
*
*   Limits:
*       N/A
*
*/


MRESULT EXPENTRY BioWndProc( hWnd, message, mp1, mp2 )
HWND   hWnd;
USHORT message;
MPARAM  mp1;
MPARAM  mp2;
{
    SHORT	iDay, i;
    HPS 	hPS;
    HPS 	hMemPS;
    RECTL       rc;
    POINTL	ptlTextBox[5];
    SIZEL	pgsi;
    HDC		hdcMem;
    HBITMAP	hbm;
    BITMAPINFOHEADER bmih;
    POINTL	ptlPoints[3];

    switch( message )
    {

    case WM_CREATE:
	BioGetDate(hWnd);

	/* Put date of the day three lines down on display */
        Day = SelectDay - 3;

        /* Initially set elevator */
        iDay = (int)(Day/365);
	WinSendMsg( WinWindowFromID( WinQueryWindow(hWnd,QW_PARENT,FALSE),
				     FID_VERTSCROLL),
                    SBM_SETPOS, (MPARAM)MAKEULONG(iDay, 0), 0L );

        /* Get System font text metrics */
        hPS = WinGetPS( hWnd );
	GpiQueryFontMetrics( hPS, (LONG)sizeof tmFontInfo, &tmFontInfo );
	/* Get sizes of long strings to be used as yard sticks for sizing
	   windows and objects.  This is necessary because of new system
	   proportional fonts. */
	GpiQueryTextBox( hPS, 10L, "Emotional ", TXTBOX_COUNT, ptlTextBox );
	cxLegendField = (SHORT)ptlTextBox[TXTBOX_CONCAT].x;
	GpiQueryTextBox( hPS, 10L, "W 99-99-99", TXTBOX_COUNT, ptlTextBox );
	cxDateField = (SHORT)ptlTextBox[TXTBOX_CONCAT].x;
        WinReleasePS( hPS );
        break;

    case WM_CLOSE:
        WinPostMsg( hWnd, WM_QUIT, 0L, 0L );
        break;

    case WM_COMMAND:
        switch (LOUSHORT(mp1)) {
            case IDM_DATES:
                if (WinDlgBox( HWND_DESKTOP, hWnd, (PFNWP)BioDlg, NULL, IDD_DATE, NULL )) {
                   WinInvalidateRect( hWnd, NULL, FALSE );
                   iDay = (int)(Day/365);
                   WinSendMsg( WinWindowFromID( hwndAppFrame, FID_VERTSCROLL),
                               SBM_SETPOS, (MPARAM)MAKEULONG(iDay, 0), 0L );
                }
                break;

	    case IDM_LEGEND:
                if (bKid = !bKid) {
                   WinSendMsg( WinWindowFromID( hwndAppFrame, FID_MENU),
                               MM_SETITEMATTR,
			       (MPARAM)MAKEULONG( IDM_LEGEND, TRUE ),
                               (MPARAM)MAKEULONG( MIA_CHECKED, MIA_CHECKED) );
                   WinShowWindow( hwndKidFrame, TRUE );
                } else {
                   WinSendMsg( WinWindowFromID( hwndAppFrame, FID_MENU),
                               MM_SETITEMATTR,
			       (MPARAM)MAKEULONG( IDM_LEGEND, TRUE ),
                               (MPARAM)MAKEULONG( MIA_CHECKED, 0) );
                   WinShowWindow( hwndKidFrame, FALSE );
                }
                break;

	    case IDM_COPY:
		/* Get access to clipboard. */
		WinOpenClipbrd( hAB );
		/* Wipe the slate clean. */
		WinEmptyClipbrd( hAB );

		/* Bitmap header for bitmap the size of the window. */
		pgsi.cx = rclClient.xRight;
		pgsi.cy = rclClient.yTop;
		bmih.cbFix = 12;
		bmih.cx = (USHORT)pgsi.cx;
		bmih.cy = (USHORT)pgsi.cy;
		bmih.cPlanes = 1;
		bmih.cBitCount = 24;

		/* Get a memory dc. */
		hdcMem = DevOpenDC( hAB
			      , OD_MEMORY
			      , (PSZ)"*"
			      , 8L
			      , (PDEVOPENDATA)dcdatablk
			      , (HDC)NULL
			      );

		/* Get a memory PS that will be used to manipulate image */
		hMemPS = GpiCreatePS( hAB
				, hdcMem
				, (PSIZEL)&pgsi
				, (LONG)PU_PELS | GPIT_MICRO | GPIA_ASSOC
				);

		/* Create a bitmap to hold image */
		hbm = GpiCreateBitmap(hMemPS, (PBITMAPINFOHEADER2)&bmih, 0L, NULL, NULL);
		/* Select bitmap into PS */
		GpiSetBitmap( hMemPS, hbm );
		/* BitBlt window client area into memory bitmap */
		ptlPoints[0].x = 0;
		ptlPoints[0].y = 0;
		ptlPoints[1].x = pgsi.cx;
		ptlPoints[1].y = pgsi.cy;
		ptlPoints[2].x = 0;
		ptlPoints[2].y = 0;
		hPS = WinGetPS( hWnd );
		GpiBitBlt(hMemPS, hPS, 3L, ptlPoints, ROP_SRCCOPY, BBO_OR);
		WinReleasePS( hPS );

		/* Put bitmap into the clipboard. */
		WinSetClipbrdData( hAB, (ULONG)hbm, CF_BITMAP, CFI_HANDLE );

		/* Tidy up */
		WinCloseClipbrd( hAB );
		GpiSetBitmap( hMemPS, NULL );
		GpiDestroyPS( hMemPS );
		DevCloseDC( hdcMem );
                break;

            case IDM_ABOUT:
                WinDlgBox( HWND_DESKTOP, hWnd, (PFNWP)About, NULL,
                           IDD_ABOUT, NULL );
                break;

            default:
                break;
            }
            break;

    case WM_SIZE:
        WinQueryWindowRect( hWnd, &rclClient );
        LinesPerPage = (int)(rclClient.yTop / tmFontInfo.lMaxBaselineExt);
        WinSetWindowPos( hwndKidFrame, NULL, 10, 10, 0, 0, SWP_MOVE );
        break;

    case WM_CHAR:
	/* Convert keyboard to scroll bar messages to support scrolling,
	   paging, etc. with keyboard interface. */
	if ( (ULONG)mp1 & KC_KEYUP )
	    return WinDefWindowProc( hWnd, message, mp1, mp2 );
	switch (HIUSHORT( mp2 )) {
	    case VK_UP:
		mp2 = (MPARAM)MAKEULONG( 0, SB_LINEUP );
		break;
	    case VK_DOWN:
		mp2 = (MPARAM)MAKEULONG( 0, SB_LINEDOWN );
		break;
	    case VK_PAGEUP:
		mp2 = (MPARAM)MAKEULONG( 0, SB_PAGEUP );
		break;
	    case VK_PAGEDOWN:
		mp2 = (MPARAM)MAKEULONG( 0, SB_PAGEDOWN );
		break;
	    default:
		return WinDefWindowProc( hWnd, message, mp1, mp2 );
		break;
	}
	return WinSendMsg( hWnd, WM_VSCROLL, mp1, mp2 );
        break;

    case WM_VSCROLL:
        /* Don't allow any processing until valid birth date entered */
        if (!bBorn) break;

        /* Setup for scroll window - full width of client area is scrolled */
        WinCopyRect( hAB, &rc, &rclClient );
        switch (HIUSHORT(mp2)) {
          case SB_LINEUP:
            /* Update top day of display */
            Day--;
            rc.yTop = rclClient.yTop - tmFontInfo.lMaxBaselineExt;
            rc.yBottom = rclClient.yTop - (LinesPerPage-1) * tmFontInfo.lMaxBaselineExt + 1;
            WinScrollWindow( hWnd, 0, (SHORT)-tmFontInfo.lMaxBaselineExt, &rc,
                             NULL, NULL, NULL, SW_INVALIDATERGN );
            break;
          case SB_LINEDOWN:
            /* Update top day of display */
            Day++;
            rc.yTop = rclClient.yTop - 2*tmFontInfo.lMaxBaselineExt;
            rc.yBottom = rclClient.yTop - (LinesPerPage) * tmFontInfo.lMaxBaselineExt + 1;
            WinScrollWindow( hWnd, 0, (SHORT)tmFontInfo.lMaxBaselineExt, &rc,
                             NULL, NULL, NULL, SW_INVALIDATERGN );
            break;
          case SB_PAGEUP:
            Day -= (LinesPerPage-1);
            break;
          case SB_PAGEDOWN:
            Day += (LinesPerPage-1);
            break;
          case SB_SLIDERPOSITION:
            /* Set to birthday of each year because 100 year scale maps to
               default 100 position scroll bar */
            Day = (long)(LOUSHORT(mp2) * 365.25);
            break;
        default:
            return 0L;
      }
      /* Update scroll bar elevator */
      iDay = (int)(Day/365);
      WinSendMsg( WinWindowFromID( hwndAppFrame, FID_VERTSCROLL),
                  SBM_SETPOS, (MPARAM)MAKEULONG(iDay, 0), 0L );
      /* All but LINEUP/DOWN need full repaint of client area */
      if ((HIUSHORT(mp2) != SB_LINEUP) && (HIUSHORT(mp2) != SB_LINEDOWN ))
         WinInvalidateRect( hWnd, NULL, FALSE );
      WinUpdateWindow( hWnd );
      break;

    case WM_PAINT:
	APPPaint( hWnd );
	break;

    case WM_BUTTON1DOWN:
	/* Don't allow any processing until valid birthdate entered */
        if (!bBorn) break;

        /* Unhighlight previously selected line and highlight new line */
        WinCopyRect( hAB, &rc, &rclClient );
        hPS = WinGetPS( hWnd );
        for(i=0; i<2; i++) {
          /* Make sure line is visible before (un)highlighting */
          if ((SelectDay >= Day) && (SelectDay - Day < LinesPerPage-1)) {
             rc.yTop = rclClient.yTop - (int)(SelectDay - Day + 1) * tmFontInfo.lMaxBaselineExt;
             rc.yBottom = rc.yTop - tmFontInfo.lMaxBaselineExt + 1;
             WinInvertRect( hPS, &rc );
          }
          /* New line to highlight */
          SelectDay = Day + (rclClient.yTop - HIUSHORT(mp1)) / 
                      tmFontInfo.lMaxBaselineExt - 1;
        }
        WinReleasePS( hPS );
        break;

    /* Draw highlight on selected day */
    if ((SelectDay >= Day) && (SelectDay - Day < LinesPerPage - 1)) {
        rc.xRight = rclClient.xRight;
        rc.xLeft = rclClient.xLeft;
    }
    default:
        return WinDefWindowProc( hWnd, message, mp1, mp2 );
        break;
    }
    return( 0L );
}

/*  KidWndProc() - Child WndProc handling legend display.
*
*   Purpose:
*       WndProc callback function to handle all messages for legend child.
*
*   Arguments:
*       hWnd          - Handle of Window owning message
*       message       - Message itself
*       mp1           - Extra message-dependent info
*       mp2           - Extra message-dependent info
*
*   Globals (referenced):
*       hwndApp       - Window handle of parent window's client area
*       tmFontInfo    - Text Metric structure defined during WM_CREATE 
*       Color[]       - Set of colored pens used to identify cycles.
*
*   Description:
*       Display legend information relating graph line styles to each
*       cyle: physical, emotional and intellectual.  Notifies parent
*       to hide child if child window is instructed to close by user.
*/

/* Read-only global variables */
extern HWND     hwndApp;

MRESULT EXPENTRY KidWndProc( hWnd, message, mp1, mp2 )
HWND    hWnd;
USHORT  message;
MPARAM  mp1;
MPARAM  mp2;
{
    HPS         hPS;
    RECTL       rc;
    POINTL      ptl;
    SHORT	i;

    switch( message )
    {
    case WM_CHAR:
	/* Convert keyboard to scroll bar messages to support scrolling,
	   paging, etc. with keyboard interface. */
	WinSendMsg( hwndApp, message, mp1, mp2 );
        break;

        case WM_PAINT:
            hPS = WinBeginPaint( hWnd, NULL, NULL );

	    /* Erase client area */
            WinQueryWindowRect( hWnd, &rc );
	    WinFillRect( hPS, &rc, CLR_PALEGRAY );

            ptl.x = 0;
            ptl.y = tmFontInfo.lMaxDescender;
	    GpiCharStringAt( hPS, &ptl, 8L, (PCH)"Physical" );
            ptl.y += tmFontInfo.lMaxBaselineExt;
            GpiCharStringAt( hPS, &ptl, 9L, (PCH)"Emotional" );
            ptl.y += tmFontInfo.lMaxBaselineExt;
            GpiCharStringAt( hPS, &ptl, 9L, (PCH)"Intellect" );

            for (i=0; i<3; i++ ) {
                GpiSetColor( hPS, Color[i] );
		ptl.x = cxLegendField;
                ptl.y = i * tmFontInfo.lMaxBaselineExt +
                        tmFontInfo.lMaxBaselineExt/2;
                GpiMove( hPS, &ptl );
		ptl.x = rc.xRight - tmFontInfo.lAveCharWidth;
                GpiLine( hPS, &ptl );
            }

            WinEndPaint( hPS );
            break;

	case WM_BUTTON1UP:
	    /* Quick way to make Legend window disappear using mouse. */
	    WinPostMsg( hwndApp, WM_COMMAND, (MPARAM)MAKEULONG(IDM_LEGEND, 0), 0L );
            break;

	case WM_TRANSLATEACCEL:
	    /* Change window handle.  Child window's frame will block
	       ALT handling, so bypass frame.  Return window handle
	       of main window's client area.  Now the ALT key message
	       handling will be passed on up the chain of windows to
	       the main window's frame for proper ALT key message
	       handling.  The main window's menu bar will highlight
	       even if the child window has the focus. */
	    return WinDefWindowProc( hwndApp, message, mp1, mp2 );
	    break;

        default:
            return WinDefWindowProc( hWnd, message, mp1, mp2 );
            break;
    }
    return( 0L );
}
/*
*/

/*  FrameWndProc() - Subclass routine for frame.
*
*   Purpose:
*	Handle WM_QUERYTRACKINFO message so that a minimum horizontal and
*	vertical window size can be controlled.  This minimum size keeps
*	the tabulated data from overlapping and leaves at least 4 rows
*	of data visible.
*
*   Arguments:
*       hWnd          - Handle of Window owning message
*       message       - Message itself
*       mp1           - Extra message-dependent info
*       mp2           - Extra message-dependent info
*
*   Globals (referenced):
*	OldFrameWndProc - Original Frame Window procedure.
*
*   Limits:
*       N/A
*
*/

MRESULT EXPENTRY FrameWndProc( hWnd, message, mp1, mp2 )
HWND    hWnd;
USHORT  message;
MPARAM  mp1;
MPARAM  mp2;
{
    switch( message )
    {
	case WM_QUERYTRACKINFO:
	    (*OldFrameWndProc)( hWnd, message, mp1, mp2 );
	    /* Limit vertical and horizontal minimum size.  Must take into
	       account menu, title, border and font widths and heights for
	       device independence. */
	    ((PTRACKINFO)mp2)->ptlMinTrackSize.x = cxDateField * 2 + cxDateField/2;
	    ((PTRACKINFO)mp2)->ptlMinTrackSize.y =
		tmFontInfo.lMaxBaselineExt * 5 +
		WinQuerySysValue( HWND_DESKTOP, SV_CYMENU ) +
		WinQuerySysValue( HWND_DESKTOP, SV_CYTITLEBAR ) +
		WinQuerySysValue( HWND_DESKTOP, SV_CYSIZEBORDER ) * 2 +
		WinQuerySysValue( HWND_DESKTOP, SV_CYBORDER ) * 2;
	    return (MRESULT)TRUE;
	    break;

        default:
	    return (*OldFrameWndProc)( hWnd, message, mp1, mp2 );
            break;
    }
    return( 0L );
}
/*
*/

VOID BioGetDate(HWND hWnd)
{
    SHORT	year, month;
    double      day;

    /* Read in birth date from OS2.INI.  Error value is 12-31-1899,
       which is out of range for valid entries. */
    year = PrfQueryProfileInt( HINI_USERPROFILE, szAppName, "Year", 1899 );
    month = PrfQueryProfileInt( HINI_USERPROFILE, szAppName, "Month", 12 );
    day = (double)PrfQueryProfileInt( HINI_USERPROFILE, szAppName, "Day", 31 );

    /* Compute date of birth in julian days */
    Born = julian( year, month, day );

    /* Get time zone environment information */
    tzset();
    /*
	    System clock starts 1-1-1970.  Get julian date then and how many
	    days have elapsed since, so that number of days since birth date
	    can be determined
    */
    SelectDay  = (long)(julian( 1970, 1, 1.0 ) +
       (double)((time(NULL) - timezone + (long)daylight*3600)/86400) -
       Born );

    /* If no valid OS2.INI info then automatically bring up dialog box */
    if (year < 1900) {
       bBorn = FALSE;
       WinPostMsg( hWnd, WM_COMMAND, (MPARAM)MAKEULONG(IDM_DATES, 0), 0L );
    }
}
/*
*/
