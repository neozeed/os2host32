#include "jigsaw.h"
#include "globals.h"
#include <stdio.h>

/******************************************************************************/
/*                                                                            */
/* SendCommand	will attempt to post the required command and parameters to   */
/* the asynchronous drawing thread's message queue. The command will only     */
/* be posted if the queue exists.					      */
/*                                                                            */
/******************************************************************************/
BOOL SendCommand( usCommand, mp1, mp2)
USHORT usCommand;
MPARAM mp1;
MPARAM mp2;
{
  if( !hmqAsync)
      return( FALSE);

  switch( usCommand)
  {
    case UM_DIE:
    case UM_LEFTDOWN:
    case UM_LEFTUP:
    case UM_MOUSEMOVE:
    case UM_DRAW:
    case UM_HSCROLL:
    case UM_VSCROLL:
    case UM_ZOOM:
    case UM_REDRAW:
    case UM_SIZING:
    case UM_JUMBLE:
    case UM_LOAD:
    case UM_CHAR:
 
	return( WinPostQueueMsg( hmqAsync
			       , usCommand
			       , mp1
			       , mp2 ) );
	break;

    default:
	return( TRUE);
  }
}
 
 
/******************************************************************************/
/*                                                                            */
/* ClientWndProd is the window procedure associated with the client window.   */
/*                                                                            */
/******************************************************************************/
MRESULT EXPENTRY ClientWndProc(hwnd, msg, mp1, mp2)
HWND    hwnd;
USHORT  msg;
MPARAM  mp1;
MPARAM  mp2;
{
  CHAR  szTemp[128];
  ULONG ulPostCt;

  switch( msg)
  {
    case WM_CHAR:
      SendCommand( UM_CHAR, mp1, mp2);
      break;

    case WM_CLOSE:
      WinLoadString( habMain, NULL, TERMINATE, sizeof(szTemp), (PSZ)szTemp );
      if( WinMessageBox( HWND_DESKTOP
		       , hwndFrame
		       , szTemp
		       , szTitle
		       , 0
		       , MB_CUAWARNING | MB_YESNO | MB_DEFBUTTON2)
	       == MBID_YES)
	  WinPostMsg( hwnd, WM_QUIT, NULL, NULL);
      break;
 
    case WM_PAINT:
      return( WndProcPaint());
      break;
 
    /**************************************************************************/
    /*									      */
    /**************************************************************************/
    case WM_ERASEBACKGROUND:
      return( ( MRESULT)TRUE);
      break;
 
    /**************************************************************************/
    /* Process menu item commands, and commands generated from the keyboard   */
    /* via the accelerator table. Most are handled by the async thread        */
    /**************************************************************************/
    case WM_COMMAND:
      return( WndProcCommand( hwnd, msg, mp1, mp2));
      break;
 
    /**************************************************************************/
    /* Scrolling is handled by the async drawing thread. Simply pass on the   */
    /* command and parameters                                                 */
    /**************************************************************************/
    case WM_HSCROLL:
      SendCommand( UM_HSCROLL, mp2, 0);
      break;
 
    case WM_VSCROLL:
      SendCommand( UM_VSCROLL, mp2, 0);
      break;
 
    /************************************************************************/
    /* The client area is being resized.                                    */
    /************************************************************************/
    case WM_SIZE:
      DosResetEventSem( hevDrawOn, &ulPostCt);
      SendCommand( UM_SIZING, mp1, mp2);
      break;
 
    /**************************************************************************/
    /* Mouse commands are handled by the async thread. Simply send on the     */
    /* command and parameters.                                                */
    /**************************************************************************/
    case WM_BUTTON1DBLCLK:
    case WM_BUTTON1DOWN:
      if( hwnd != WinQueryFocus( HWND_DESKTOP, FALSE))
	  WinSetFocus( HWND_DESKTOP, hwnd);
      if( !fButtonDownMain)
      {
	  fButtonDownMain = TRUE;
	  SendCommand( UM_LEFTDOWN, mp1, 0);
      }
      return( ( MRESULT)TRUE);
      break;
 
    case WM_BUTTON1UP:
      if( !fButtonDownMain)
	  return( ( MRESULT)TRUE);
      if( SendCommand( UM_LEFTUP, mp1, 0))
	  fButtonDownMain = FALSE;
      else
	  WinAlarm( HWND_DESKTOP, WA_WARNING);
      return( ( MRESULT)TRUE);
      break;
 
    case WM_MOUSEMOVE:
      DosQueryEventSem( hevMouse, &ulPostCt);
      if( ulPostCt) {
	SendCommand( UM_MOUSEMOVE, mp1, 0);
        }
      return( WinDefWindowProc( hwnd, msg, mp1, mp2));
      break;

    /**************************************************************************/
    /* Default for the rest						      */
    /**************************************************************************/
    default:
      return( WinDefWindowProc( hwnd, msg, mp1, mp2));
  }
 
  return( FALSE);
}
 
/******************************************************************************/
/*									      */
/* WM_PAINT message							      */
/*									      */
/******************************************************************************/
MRESULT WndProcPaint(VOID)
{
  HRGN	 hrgnUpdt;
  ULONG  ulPostCt;
  SHORT  sRgnType;
 
  hrgnUpdt = GpiCreateRegion( hpsPaint, 0L, NULL);
  sRgnType = WinQueryUpdateRegion( hwndClient, hrgnUpdt);
  SendCommand( UM_DRAW, (MPARAM)hrgnUpdt, 0);
  DosQueryEventSem( hevLoadMsg, &ulPostCt);
  if( ulPostCt) {
    WinSetDlgItemText(hwndStatus, SID_STATUS, pszLoadMsg);
    }
  WinValidateRegion( hwndClient, hrgnUpdt, FALSE);
  return( FALSE);
}
 
/******************************************************************************/
/* Process menu item commands, and commands generated from the keyboard via   */
/* the accelerator table.  Most are handled by the async thread 	      */
/******************************************************************************/
MRESULT WndProcCommand(HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2)
{
  CHAR	    szTemp[128];
  ULONG     ulPostCt;

  switch( SHORT1FROMMP(mp1))
  {
    case MENU_SHOW:
        WinSetParent(hwndStatus, HWND_DESKTOP, TRUE);
        WinShowWindow(hwndStatus, TRUE);
        STATUS_SHOW(FALSE);
        STATUS_HIDE(TRUE);
        break;

    case MENU_HIDE:
        WinSetParent(hwndStatus, HWND_OBJECT, TRUE);
        WinShowWindow(hwndStatus, TRUE);
        STATUS_SHOW(TRUE);
        STATUS_HIDE(FALSE);
        break;

    case MENU_JUMBLE:
	DosResetEventSem( hevDrawOn, &ulPostCt);
	SendCommand( UM_JUMBLE, 0, 0);
	break;

    case MENU_LOAD:

        if (WinDlgBox(HWND_DESKTOP
                    , hwnd
                    , CheapWndProc
                    , (HMODULE)NULL
                    , IDD_OPEN
                    , NULL) == MBID_CANCEL) {
          return FALSE;
          }


        DosResetEventSem( hevDrawOn, &ulPostCt);
        SendCommand( UM_LOAD, (MPARAM)pli, 0);
	break;
    /**********************************************************************/
    /* EXIT command, menu item or F3 key pressed. Give the operator a	  */
    /* second chance, if confirmed post a WM_QUIT msg to the application  */
    /* msg queue. This will force the MAIN thread to terminate.           */
    /**********************************************************************/
    case MENU_EXIT:
      WinLoadString( habMain, NULL, TERMINATE, sizeof(szTemp), szTemp);
      if( WinMessageBox( HWND_DESKTOP
		       , hwndFrame
		       , szTemp
		       , szTitle
		       , 0
		       , MB_CUAWARNING | MB_YESNO | MB_DEFBUTTON2)
	    == MBID_YES)
	WinPostMsg( hwnd, WM_QUIT, NULL, NULL);
      break;
 
    /**********************************************************************/
    /* Pass on the rest to the async thread.				  */
    /**********************************************************************/
    case MENU_ZOOMIN:
      DosResetEventSem( hevDrawOn, &ulPostCt);
      SendCommand( UM_ZOOM, (MPARAM)ZOOM_IN, 0);
      break;
 
    case MENU_ZOOMOUT:
      DosResetEventSem( hevDrawOn, &ulPostCt);
      SendCommand( UM_ZOOM, (MPARAM)ZOOM_OUT , 0);
      break;

    /**********************************************************************/
    /* Unrecognised => default						  */
    /**********************************************************************/
    default:
      return( WinDefWindowProc(hwnd, msg, mp1, mp2));
  }
  return( FALSE);
}
 

/******************************************************************************/
/* Dialog window processing routine                                           */
/******************************************************************************/
MRESULT EXPENTRY CheapWndProc(hwnd, msg, mp1, mp2)
HWND   hwnd;
USHORT msg;
MPARAM mp1;
MPARAM mp2;
{
    switch (msg) {
    case WM_INITDLG:
        WinSetFocus(HWND_DESKTOP, WinWindowFromID(hwnd, EID_NAME));
        WinSendDlgItemMsg(hwnd, EID_NAME, EM_SETTEXTLIMIT,
                MPFROMSHORT((SHORT) sizeof(pli->szFileName)), (MPARAM) 0L);
        return ((MRESULT) TRUE);
        break;

    case WM_COMMAND:
        if (LOUSHORT(mp1) == MBID_OK) {
            ULONG ulDummy;

            WinQueryDlgItemText(hwnd, EID_NAME, sizeof(pli->szFileName),
                    pli->szFileName);

            DosOpen(pli->szFileName, &(pli->hf), &ulDummy, 0, 0x0001,
                    0x00000001, 0x0010, 0x00000000);
        }

        WinDismissDlg(hwnd, LOUSHORT(mp1));
        break;

    default:
        return WinDefDlgProc(hwnd, msg, mp1, mp2);
    }

    return (MRESULT)0L;
}

/******************************************************************************/
/* Dialog window processing routine                                           */
/******************************************************************************/
MRESULT EXPENTRY StatusDlgProc(hwnd, msg, mp1, mp2)
HWND   hwnd;
USHORT msg;
MPARAM mp1;
MPARAM mp2;
{
    ULONG  ulPostCt;
    LONG   lTempScale;

    switch (msg) {
        case WM_INITDLG:
            WinSendMsg( hwndMenu,
                        MM_SETITEMATTR,
                        MPFROM2SHORT( MENU_SHOW, TRUE),
                        MPFROM2SHORT( MIA_DISABLED, MIA_DISABLED));
            hwndZoomScrollBar = WinWindowFromID(hwnd, HID_ZOOM);

            WinSendMsg(hwndZoomScrollBar, SBM_SETSCROLLBAR,
                    MPFROMSHORT((USHORT) 0),
                    MPFROM2SHORT((USHORT) 0, (USHORT) ZOOM_MAX));
            WinSendMsg(hwndZoomScrollBar, SBM_SETTHUMBSIZE,
                    MPFROM2SHORT((USHORT) 1, (USHORT) (ZOOM_MAX + 1)), 0L);
            WinEnableWindow(hwndZoomScrollBar, FALSE);
            WinSetDlgItemText(hwnd, SID_STATUS, pszStartupMsg);
            sprintf(szZoomFact, "1:1");
            WinSetDlgItemText(hwnd, SID_ZOOMFACT, szZoomFact);

            break;

        case WM_HSCROLL:
            /*
             * if BMP is being loaded ingnore zoom messages
             */
            DosQueryEventSem(hevLoadMsg, &ulPostCt);
            if (ulPostCt) {
                break;
                }

            switch (SHORT2FROMMP(mp2)) {

                case SB_SLIDERTRACK:
                    lTempScale = (LONG) -SHORT1FROMMP(mp2);
                    DisplayZoomFactor(lTempScale);
                    break;

                case SB_SLIDERPOSITION:
                    lTempScale = (LONG) -SHORT1FROMMP(mp2);
                    if (lScale != lTempScale) {
                        lScale = lTempScale;
                        DosResetEventSem( hevDrawOn, &ulPostCt);
                        DosPostEventSem( hevKillDraw);
                        DisplayZoomFactor(lScale);
                        SendCommand( UM_ZOOM, (MPARAM)ZOOM_IN, 0);
                        }
                    break;

                case SB_LINELEFT:
                    if (lScale != 0) {
                        lScale++;
                        DosResetEventSem( hevDrawOn, &ulPostCt);
                        DosPostEventSem( hevKillDraw);
                        DisplayZoomFactor(lScale);
                        SendCommand( UM_ZOOM, (MPARAM)ZOOM_IN, 0);
                        }
                    break;

                case SB_LINERIGHT:
                    if (lScale != -ZOOM_MAX) {
                        lScale--;
                        DosResetEventSem( hevDrawOn, &ulPostCt);
                        DosPostEventSem( hevKillDraw);
                        DisplayZoomFactor(lScale);
                        SendCommand( UM_ZOOM, (MPARAM)ZOOM_OUT , 0);
                        }
                    break;
                }
            break;

        case WM_COMMAND:

            switch (SHORT1FROMMP(mp1)) {

                case 1: // hide dialog box
                  WinSetParent(hwndStatus, HWND_OBJECT, TRUE);
                  WinShowWindow(hwndStatus, TRUE);
                  STATUS_SHOW(TRUE);
                  STATUS_HIDE(FALSE);
                  break;
                }
            break;

        default:
            return WinDefDlgProc(hwnd, msg, mp1, mp2);
        }

    return (MRESULT)0L;
}
