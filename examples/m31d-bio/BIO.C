/*  Biorhythm - Utility to compute personal biorhythm charts.
*
*   Created by Microsoft, IBM Corporation, 1990
*
*	DISCLAIMER OF WARRANTIES.  The following [enclosed] code is 
*	sample code created by Microsoft Corporation and/or IBM 
*	Corporation. This sample code is not part of any standard 
*	Microsoft or IBM product and is provided to you solely for 
*	the purpose of assisting you in the development of your 
*	applications.  The code is provided "AS IS", without 
*	warranty of any kind.  Neither Microsoft nor IBM shall be 
*	liable for any damages arising out of your use of the sample 
*	code, even if they have been advised of the possibility of 
*	such damages.
*
*   Purpose:
*       Program entry point, initialization and GetMessage loop.
*
*   Arguments:
*       None
*
*   Globals (modified):
*       hAB           - Handle to the Anchor Block
*       hMsgQ         - Handle to the application's message queue
*       hwndAppFrame  - Window handle of parent window's frame
*       hwndKidFrame  - Window handle of parent window's frame
*       hwndApp       - Window handle of parent window's client area
*       hwndKid       - Window handle of child window's client area
*       szAppName[10] - RC file program name (Biorhythm).
*       szKidName[10] - RC file child window name (Legend).
*
*   Globals (referenced):
*       tmFontInfo    - Text Metric structure defined during WM_CREATE 
*
*   Description:
*       The theory of biorhythms states that life consists of three cycles,
*       physical, emotional and intellectual of 23, 28 and 33 days,
*       respectively.  The cycles each begin on the date of birth.
*
*   Limits:
*       The intended use of this program is for the 20th and 21st centuries.
*       The calculations of biorhythms will not be accurate outside of this
*       range due to formulae used to compute days between dates.
*
*/

#define INCL_WIN
#include <os2.h>

#include <stddef.h>

#include "bio.h"

/* Write-once global variables */
HAB     hAB;
HMQ     hMsgQ;
HWND    hwndApp, hwndKid;
HWND    hwndAppFrame, hwndKidFrame;
char    szAppName[10];
char    szKidName[10];
ULONG	AppCtlData = FCF_STANDARD | FCF_VERTSCROLL | FCF_NOBYTEALIGN & ~FCF_SHELLPOSITION;
ULONG	KidCtlData = FCF_TITLEBAR;
PFNWP	OldFrameWndProc;

/* Read-only global variables */
extern  FONTMETRICS     tmFontInfo;
extern	SHORT		cxLegendField;
extern	SHORT		cxDateField;

BOOL cdecl main( VOID )
{
    QMSG        qMsg;
    SHORT       dx, dy, x, y;
    SHORT	cxSizeBorder;
    SHORT	cySizeBorder;
    SHORT	cxBorder;
    SHORT	cyBorder;

    /* Standard initialization.  Get anchor block and message queue. */
    hAB   = WinInitialize(NULL);
    hMsgQ = WinCreateMsgQueue( hAB, 0 );

    /* Get string constants for parent and child window registration
       and creation from resource string table. */
    WinLoadString( hAB, NULL, IDS_APPNAME, sizeof(szAppName), szAppName );
    WinLoadString( hAB, NULL, IDS_KIDNAME, sizeof(szKidName), szKidName );

    /* Register parent window.  Terminate if error. */
    if ( !WinRegisterClass( hAB, szAppName, (PFNWP)BioWndProc,
	    CS_CLIPCHILDREN | CS_SIZEREDRAW, NULL ) )
        return( FALSE );

    /* Register child window.  Terminate if error. */
    if ( !WinRegisterClass( hAB, szKidName, (PFNWP)KidWndProc, 0L, NULL ) )
        return( FALSE );

    /* Create a parent window of class szAppName */
    hwndAppFrame = WinCreateStdWindow(
        HWND_DESKTOP,
	0L,
	&AppCtlData,
        szAppName,
	NULL,
        0L,
        NULL,
        ID_BIO,
        (HWND FAR *)&hwndApp
        );

    /* Create a child window of class KidClass */
    hwndKidFrame = WinCreateStdWindow(
        hwndApp,
	FS_BORDER,
	&KidCtlData,
        szKidName,
        szKidName,
        0L,
        NULL,
        0,
        (HWND FAR *)&hwndKid
        );

    /* Subclass frame so that minimum window size can be controled */
    OldFrameWndProc = WinSubclassWindow( hwndAppFrame, (PFNWP)FrameWndProc );

    /* Get the size of the screen and border.  Used to place and size window */
    cxSizeBorder =  (SHORT)WinQuerySysValue( HWND_DESKTOP, SV_CXSIZEBORDER );
    cySizeBorder =  (SHORT)WinQuerySysValue( HWND_DESKTOP, SV_CYSIZEBORDER );
    cxBorder	 =  (SHORT)WinQuerySysValue( HWND_DESKTOP, SV_CXBORDER );
    cyBorder	 =  (SHORT)WinQuerySysValue( HWND_DESKTOP, SV_CYBORDER );
    x		 =  (SHORT)WinQuerySysValue( HWND_DESKTOP, SV_CXSCREEN );
    y		 =  (SHORT)WinQuerySysValue( HWND_DESKTOP, SV_CYSCREEN );

    /* Calculate width and height of child window.  Must be able to
       display three lines and wide enough for text and corresponding colored
       line.  Must take into account titlebar and border vertical sizes. */
    dx = cxLegendField * 2;
    dy = (SHORT)(tmFontInfo.lMaxBaselineExt*3 +
         WinQuerySysValue( HWND_DESKTOP, SV_CYTITLEBAR ) +
         WinQuerySysValue( HWND_DESKTOP, SV_CYBORDER ) * 2);

    /* Place and size parent and child windows, then make them visible.
       WinCreateStdWindow does not include position and size arguments.
       Parent window is thin, but full screen high.  Child window is placed
       10 pixels over and up from the parent window's lower left corner. */
    WinSetWindowPos( hwndAppFrame, NULL,
		     x-(3*cxDateField)+cxSizeBorder,
		     -cySizeBorder,
		     (3*cxDateField),
		     y+2*cySizeBorder,
		     SWP_MOVE | SWP_SIZE | SWP_ACTIVATE | SWP_SHOW );
    WinSetWindowPos( hwndKidFrame, NULL, 10, 10, dx, dy,
		     SWP_MOVE | SWP_SIZE | SWP_ACTIVATE | SWP_SHOW );

    /* Get messages from application queue and dispatch them for processing */
    while( WinGetMsg( hAB, &qMsg, (HWND)NULL, 0, 0 ) )
    {
        WinDispatchMsg( hAB, &qMsg );
    }

    /* Clean up.  All child windows will be destoyed automatically */
    WinDestroyWindow( hwndAppFrame );
    WinDestroyMsgQueue( hMsgQ );
    WinTerminate( hAB );
}
