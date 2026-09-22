/*
 * HELLO.C -- A simple program which calls the OpenDlg library
 *
 * Created by Microsoft, IBM Corporation 1990
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
 */
#define INCL_PM
#include <os2.h>
#include "..\opendlg.h"
#include <string.h>
#include "hello.h"
/*
 * Globals
 */
HAB     hAB;
HMQ     hMqHello;
HWND    hWndHello;
HWND    hWndHelloFrame;
CHAR    szClassName[]	= "Hello World";
CHAR	szMessage[]	= " - File Dialog Sample";
CHAR	szExtension[]	= "\\*";
CHAR	szHelp[]	= "Help would go here.";
DLF	vdlf;
HFILE	vhFile;
/*
 * Main routine...initializes window and message queue
 */
int main( ) {
    QMSG qmsg;
    ULONG ctldata;

    hAB = WinInitialize(NULL);

    hMqHello = WinCreateMsgQueue(hAB, 0);

    if (!WinRegisterClass( hAB, (PCH)szClassName, (PFNWP)HelloWndProc,
		CS_SIZEREDRAW, 0))
        return( 0 );

    /* Create the window */
    ctldata = FCF_STANDARD & ~(FCF_ACCELTABLE);
    hWndHelloFrame = WinCreateStdWindow( HWND_DESKTOP, WS_VISIBLE, &ctldata,
                                         szClassName, szMessage,
                                         0L, (HMODULE)NULL, ID_RESOURCE,
					 &hWndHello );

    if (hWndHelloFrame == (HWND)NULL)
	return 0;
    WinShowWindow( hWndHelloFrame, TRUE );

    /* Poll messages from event queue */
    while( WinGetMsg( hAB, (PQMSG)&qmsg, (HWND)NULL, 0, 0 ) )
        WinDispatchMsg( hAB, (PQMSG)&qmsg );

    /* Clean up */
    WinDestroyWindow( hWndHelloFrame );
    WinDestroyMsgQueue( hMqHello );
    WinTerminate( hAB );
}

MRESULT EXPENTRY HelloWndProc(hWnd, msg, mp1, mp2)
/*
 * This routine processes WM_COMMAND, WM_PAINT.  It passes
 * everything else to the Default Window Procedure.
 */
HWND hWnd;
USHORT msg;
MPARAM mp1;
MPARAM mp2;
{
    HPS		hPS;
    POINTL	pt;
    CHARBUNDLE	cb;
    RECTL	rcl;

    switch (msg) {

	case WM_COMMAND:
	    switch (LOUSHORT(mp1)) {

		case IDM_OPEN: /* Demonstrate Open... dialog call */
		    SetupDLF( &vdlf
			    , DLG_OPENDLG
			    , &vhFile
			    , szExtension
			    , NULL
			    , "Open Title"
			    , szHelp );
	 	    DlgFile(hWndHelloFrame, &vdlf);
		    break;

		case IDM_SAVE: /* Demonstrate Save As... dialog call */
		    SetupDLF( &vdlf
			    , DLG_SAVEDLG
			    , &vhFile
			    , szExtension
			    , NULL
			    , "Save Title"
			    , szHelp);
		    strcpy((PSZ)vdlf.szOpenFile, (PSZ)"foo.bar");
		    DlgFile(hWndHelloFrame, &vdlf);
		    break;

		case IDM_ABOUT:
		    WinDlgBox(HWND_DESKTOP, hWnd, AboutDlgProc,
			      NULL, IDD_ABOUT, NULL);
		    return 0;

		default: break;
	    }
	    break;

	case WM_PAINT:
	    /* Open the presentation space */
	    hPS = WinBeginPaint(hWnd, NULL, &rcl);

	    /* Fill the background with Dark Blue */
	    WinFillRect(hPS, &rcl, CLR_DARKBLUE);

	    /* Write "Hello World" in Red */
	    pt.x = pt.y = 0L;
	    cb.lColor = CLR_RED;
	    GpiSetAttrs(hPS, PRIM_CHAR, CBB_COLOR, 0L, &cb);
	    GpiCharStringAt(hPS, &pt, (LONG)sizeof(szClassName)-1, szClassName);

	    /* Finish painting */
	    WinEndPaint(hPS);
	    break;

	default:
	    return WinDefWindowProc(hWnd, msg, mp1, mp2); break;
    }
    return 0L;
}

MRESULT EXPENTRY AboutDlgProc(hDlg, msg, mp1, mp2)
/*
    About... dialog procedure
*/
HWND hDlg;
USHORT msg;
MPARAM mp1;
MPARAM mp2;
{
    switch(msg) {
	case WM_COMMAND:
	    switch(LOUSHORT(mp1)) {
		case DID_OK: WinDismissDlg(hDlg, TRUE); break;
		default: break;
	    }
	default: return WinDefDlgProc(hDlg, msg, mp1, mp2);
    }
    return FALSE;
}
