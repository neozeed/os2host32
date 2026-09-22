/*************************  JIGSAW.C *************************************\
 *
 * PROGRAM NAME: JIGSAW
 * ------------- 
 *
 * Created by Microsoft, IBM Corporation, 1990
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
 * REVISION HISTORY:
 * -----------------  
 *	Original version, 1988
 *	Updated for flat model, 1990
 *
 * WHAT THIS PROGRAM DOES: 
 * ----------------------- 
 *   This program provides a jigsaw puzzle, based on a decomposition 
 *   of an arbitrary bitmap loaded from a file.  The user can jumble the
 *   pieces, then drag them individually by means of the mouse.  The image
 *   can be zoomed in and out and scrolled up/down and left/right.
 *
 *   JIGSAW uses GpiBitBlt with clip paths to create a collection of picture
 *   fragments which are the puzzle pieces.  In earlier versions of the 
 *   program, each of these pieces was associated with a single retain-mode
 *   graphics segment.  The retain-mode technique, however, proved to be 
 *   too slow, so subsequent versions of the program used retain-mode APIs
 *   for fewer and fewer operations.  The current version eliminates 
 *   retain-mode graphics altogether.  Instead, the drawing data for each 
 *   piece is stored in _SEGLIST data structure defined in JIGSAW.H.  
 *   This structure contains all the data needed to draw a piece, including 
 *   pointers to the previous and next pieces.  The _SEGLIST nodes are 
 *   arranged in drawing priority order, so the picture can be reconstructed
 *   by traversing the list in sequence, drawing each piece as its 
 *   corresponding structure is encountered.  Where the comments in the 
 *   rest of the program refer to a "segment," they are simply referring to
 *   a piece of the puzzle as defined by a record in this data structure.
 *
 *   To retain responsiveness to user requests, the real work is done in a 
 *   second thread, with work requests transmitted from the main thread in 
 *   the form of messages.  This arrangement makes it possible for the user
 *   to override lengthy drawing operations with a higher-priority request
 *   (eg. program termination, magnification change, etc.).
 *                                                                          
 *   Individual pieces are made to "move" by changing their model transforms.
 *   Scrolling and zooming of the whole picture is done by changing the  
 *   default viewing transform.  The points in model space associated with
 *   each piece (control points for the bounding curve, corners of the 
 *   bounding box, etc.) are converted via GpiConvert into points in device 
 *   space prior to use with GpiBitBlt, etc.
 *
 *
 * WHAT THIS PROGRAM DEMONSTRATES: 
 * ------------------------------- 
 *   Illustrates the use of GPI 
 *   Illustrates the use of off-screen bitmaps
 *
 * API CALLS FEATURED: 
 * ------------------- 
 *      GpiBeginPath
 *      GpiEndPath
 *      GpiFillPath
 *      GpiSetClipPath
 *      GpiSetClipRegion
 *
 *      GpiCreateBitmap
 *      GpiDeleteBitmap
 *      GpiSetBitmap
 *      GpiSetBitmapBits
 *      GpiBitBlt
 * 
 *      GpiConvert
 *
 *      GpiCreateRegion
 *      GpiCombineRegion
 *      GpiSetRegion
 *      GpiDestroyRegion
 *      GpiQueryRegionBox
 * 
 *      GpiSetAttrMode
 *      GpiSetColor
 *
 *      GpiQueryDefaultViewMatrix
 *      GpiSetDefaultViewMatrix 
 *
 *
 * WHAT YOU NEED TO COMPILE AND LINK THIS PROGRAM: 
 * ----------------------------------------------- 
 *
 *      REQUIRED FILES: 
 *      --------------- 
 *      JIGSAW.MAK
 *      JIGSAW.C
 *      JIGSAW.H
 *      JIGSAW.RC
 *      JIGSAW.DEF
 *      JIGSAW.ICO
 *      GLOBALS.C
 *      GLOBALS.H
 *      STATWND.DLG
 *      STATWND.H
 *      CHEAP.DLG
 *      OPENDLG.H
 *      CHEAP.H
 *      MISC.C
 *      PROCS.C
 *
 *      REQUIRED LIBRARIES: 
 *      ------------------- 
 *
 *       OS2386.LIB
 *       LIBC.LIB
 *
 *      REQUIRED PROGRAMS: 
 *      ------------------ 
 *
 *       Microsoft C386 Compiler
 *       Microsoft LINK386 Linker
 *       Resource Compiler
 *
 *
\*************************************************************************/

#include "jigsaw.h"
#include "opendlg.h"
#include "globals.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/******************************************************************************/
/*                                                                            */
/* Main thread will initialize the process for PM services and process	      */
/* the application message queue until a WM_QUIT message is received. It will */
/* then destroy all PM resources and terminate. Any error during	      */
/* initialization will be reported and the process terminated.                */
/*                                                                            */
/******************************************************************************/
VOID cdecl main(VOID)
{
  QMSG	qmsg;
 
  if( Initialize())
      while( WinGetMsg( habMain, &qmsg, NULL, NULL, NULL))
	  WinDispatchMsg( habMain, &qmsg);
  else
      ReportError( habMain);
  Finalize();
}
 
 
/******************************************************************************/
/*                                                                            */
/* The Initialize function will initialize the PM interface,		      */
/* create an application message queue, a standard frame window and a new     */
/* thread to control drawing operations.  It will also initialize static      */
/* strings.                                                                   */
/*                                                                            */
/******************************************************************************/
BOOL Initialize(VOID)
{
  ULONG    flCreate;
  PID	   pid;
  TID	   tid;
  MENUITEM mi;



  /*
   * create all semaphores for mutual exclusion and event timing
   */
  if (DosCreateMutexSem(NULL, &hmtxSzFmt,    DC_SEM_SHARED, FALSE) ||
      DosCreateEventSem(NULL, &hevDrawOn,    DC_SEM_SHARED, FALSE) ||
      DosCreateEventSem(NULL, &hevMouse,     DC_SEM_SHARED, FALSE) ||
      DosCreateEventSem(NULL, &hevLoadMsg,   DC_SEM_SHARED, FALSE) ||
      DosCreateEventSem(NULL, &hevTerminate, DC_SEM_SHARED, FALSE) ||
      DosCreateEventSem(NULL, &hevKillDraw,  DC_SEM_SHARED, FALSE)) {
      return (FALSE);
      }

  WinShowPointer( HWND_DESKTOP, TRUE);
  habMain = WinInitialize( NULL);
  if( !habMain)
      return( FALSE);
 
  hmqMain = WinCreateMsgQueue( habMain,0);
  if( !hmqMain)
      return( FALSE);
 
  WinLoadString( habMain, NULL, TITLEBAR, sizeof(szTitle), szTitle);
  if( !WinRegisterClass( habMain
		       , (PCH)szTitle
		       , (PFNWP)ClientWndProc
		       , CS_SIZEREDRAW
		       , 0 ))
      return( FALSE);
 
  flCreate =   (FCF_STANDARD | FCF_VERTSCROLL | FCF_HORZSCROLL)
	     & ~(ULONG)FCF_TASKLIST;
  hwndFrame = WinCreateStdWindow( HWND_DESKTOP
				, WS_VISIBLE
				, &flCreate
				, szTitle
				, szTitle
				, WS_VISIBLE
				, NULL
				, APPMENU
                                , &hwndClient);

  if( !hwndFrame)
      return( FALSE);




  sizlMaxClient.cx = WinQuerySysValue( HWND_DESKTOP, SV_CXFULLSCREEN);
  sizlMaxClient.cy = WinQuerySysValue( HWND_DESKTOP, SV_CYFULLSCREEN);

  lByteAlignX = WinQuerySysValue( HWND_DESKTOP, SV_CXBYTEALIGN);
  lByteAlignY = WinQuerySysValue( HWND_DESKTOP, SV_CYBYTEALIGN);

  hdcClient = WinOpenWindowDC( hwndClient);
  hpsClient = GpiCreatePS( habMain
			 , hdcClient
			 , &sizlMaxClient
			 , GPIA_ASSOC | PU_PELS );
  if( !hpsClient)
      return( ( MRESULT) TRUE);
  GpiSetAttrMode( hpsClient, AM_PRESERVE);

  hwndHorzScroll = WinWindowFromID( hwndFrame, FID_HORZSCROLL);

  hwndVertScroll = WinWindowFromID( hwndFrame, FID_VERTSCROLL);

  hpsPaint = GpiCreatePS( habMain, NULL, &sizlMaxClient, PU_PELS);
 
  hrgnInvalid = GpiCreateRegion( hpsClient, 0L, NULL);

  hwndStatus = WinLoadDlg(HWND_DESKTOP, hwndClient, StatusDlgProc,
          NULL, IDD_STATUS, NULL);

  WinQueryWindowProcess( hwndFrame, &pid, &tid);
  swctl.hwnd	  = hwndFrame;
  swctl.idProcess = pid;
  strcpy( swctl.szSwtitle, szTitle);
  hsw = WinAddSwitchEntry( &swctl);

  hwndMenu = WinWindowFromID( hwndFrame, FID_MENU);
  WinSendMsg( hwndMenu
	    , MM_QUERYITEM
	    , MPFROM2SHORT( MENU_STATUS, FALSE)
	    , MPFROMP( (PMENUITEM)&mi));
  hwndStatusOption = mi.hwndSubMenu;


  STATUS_SHOW(FALSE);
  STATUS_HIDE(TRUE);

  if(DosCreateThread(&tidAsync,
                     (PFNTHREAD) NewThread,
                     NULL,
                     0,
                     STACKSIZE )) {
      return( FALSE);
      }                               /* create async thread                 */
  if( !CreateBitmapHdcHps( &hdcBitmapFile, &hpsBitmapFile))
      return( FALSE);
  if( !CreateBitmapHdcHps( &hdcBitmapSize, &hpsBitmapSize))
      return( FALSE);
  if( !CreateBitmapHdcHps( &hdcBitmapBuff, &hpsBitmapBuff))
      return( FALSE);
  if( !CreateBitmapHdcHps( &hdcBitmapSave, &hpsBitmapSave))
      return( FALSE);
 
  return( TRUE);
}
 
/******************************************************************************/
/*                                                                            */
/* Finalize will destroy the asynchronous drawing thread, all Presentation    */
/* Manager resources, and terminate the process.                              */
/*                                                                            */
/******************************************************************************/
VOID Finalize(VOID)
{
  ULONG ulPostCt;

  if( tidAsync)
  {
      DosResetEventSem( hevDrawOn, &ulPostCt);
      DosPostEventSem( hevTerminate);
  }

  while( pslHead != NULL )
  {
    GpiSetBitmap( pslHead->hpsFill, NULL);
    GpiDeleteBitmap( pslHead->hbmFill);
    GpiDestroyPS( pslHead->hpsFill);
    DevCloseDC( pslHead->hdcFill);

    GpiSetBitmap( pslHead->hpsHole, NULL);
    GpiDeleteBitmap( pslHead->hbmHole);
    GpiDestroyPS( pslHead->hpsHole);
    DevCloseDC( pslHead->hdcHole);

    SegListUpdate( DEL_SEG, pslHead);
  }

  if( hrgnInvalid)
      GpiDestroyRegion( hpsClient, hrgnInvalid);
  if( hpsClient)
  {
      GpiAssociate( hpsClient, NULL);
      GpiDestroyPS( hpsClient);
  }
  if( hpsPaint)
      GpiDestroyPS( hpsPaint);

  if( hpsBitmapFile)
  {
      GpiSetBitmap( hpsBitmapFile, NULL);
      GpiDeleteBitmap( hbmBitmapFile);
      GpiDestroyPS( hpsBitmapFile);
      DevCloseDC( hdcBitmapFile);
  }
  if( hpsBitmapSize)
  {
      GpiSetBitmap( hpsBitmapSize, NULL);
      GpiDeleteBitmap( hbmBitmapSize);
      GpiDestroyPS( hpsBitmapSize);
      DevCloseDC( hdcBitmapSize);
  }
  if( hpsBitmapBuff)
  {
      GpiSetBitmap( hpsBitmapBuff, NULL);
      GpiDeleteBitmap( hbmBitmapBuff);
      GpiDestroyPS( hpsBitmapBuff);
      DevCloseDC( hdcBitmapBuff);
  }
  if( hpsBitmapSave)
  {
      GpiSetBitmap( hpsBitmapSave, NULL);
      GpiDeleteBitmap( hbmBitmapSave);
      GpiDestroyPS( hpsBitmapSave);
      DevCloseDC( hdcBitmapSave);
  }


  if( hwndStatus != NULL) {
      WinDestroyWindow( hwndStatus);
      }
  if( hwndFrame)
      WinDestroyWindow( hwndFrame);
  if( hmqMain)
      WinDestroyMsgQueue( hmqMain);
  if( habMain)
      WinTerminate( habMain);

  DosExit( EXIT_PROCESS, 0);
}
 
 
/******************************************************************************/
/*                                                                            */
/* ReportError	will display the latest error information for the required    */
/* thread. No resources to be loaded if out of memory error.                  */
/*                                                                            */
/******************************************************************************/
VOID ReportError( hab)
HAB hab;
{
  PERRINFO  perriBlk;
  PSZ	    pszErrMsg;
  USHORT *  TempPtr;
 
  if( !hwndFrame)
      return;
  if( !fErrMem)
  {
      perriBlk = WinGetErrorInfo(hab);
      if( !perriBlk)
          return;
      SELECTOROF( pszErrMsg) = SELECTOROF(perriBlk);
      SELECTOROF( TempPtr)   = SELECTOROF(perriBlk);
      OFFSETOF( TempPtr)     = perriBlk->offaoffszMsg;
      OFFSETOF( pszErrMsg)   = *TempPtr;
      WinMessageBox( HWND_DESKTOP
		   , hwndFrame
		   , pszErrMsg
		   , szTitle
		   , 0
		   , MB_CUACRITICAL | MB_ENTER);
      WinFreeErrorInfo( perriBlk);
  } else
      WinMessageBox( HWND_DESKTOP
		   , hwndFrame
		   , "ERROR - Out Of Memory"
		   , szTitle
		   , 0
		   , MB_CUACRITICAL | MB_ENTER);
}
 
 
/******************************************************************************/
/* Reset the scroll bars to be in the middle of their range		      */
/******************************************************************************/
VOID ResetScrollBars(VOID)
{
    RECTL     rclClient;

    WinQueryWindowRect( hwndClient, &rclClient);
    ptsScrollMax.x = (SHORT)(rclClient.xRight - rclClient.xLeft);
    ptsHalfScrollMax.x = ptsScrollMax.x >> 1;
    ptsScrollPage.x = ptsScrollMax.x >> 3;
    ROUND_DOWN_MOD( ptsScrollPage.x, (SHORT)lByteAlignX);
    ptsScrollLine.x = ptsScrollMax.x >> 5;
    ROUND_DOWN_MOD( ptsScrollLine.x, (SHORT)lByteAlignX);
    ptsScrollPos.x = ptsHalfScrollMax.x;
    ptsOldScrollPos.x = ptsHalfScrollMax.x;
    WinSendMsg( hwndHorzScroll
	      , SBM_SETSCROLLBAR
	      , MPFROMSHORT( ptsScrollPos.x)
	      , MPFROM2SHORT( 1, ptsScrollMax.x) );
    ptsScrollMax.y = (SHORT)(rclClient.yTop - rclClient.yBottom);
    ptsHalfScrollMax.y = ptsScrollMax.y >> 1;
    ptsScrollPage.y = ptsScrollMax.y >> 3;
    ROUND_DOWN_MOD( ptsScrollPage.y, (SHORT)lByteAlignY);
    ptsScrollLine.y = ptsScrollMax.y >> 5;
    ROUND_DOWN_MOD( ptsScrollLine.y, (SHORT)lByteAlignY);
    ptsScrollPos.y = ptsHalfScrollMax.y;
    ptsOldScrollPos.y = ptsHalfScrollMax.y;
    WinSendMsg( hwndVertScroll
	      , SBM_SETSCROLLBAR
	      , MPFROMSHORT( ptsScrollPos.y)
	      , MPFROM2SHORT( 1, ptsScrollMax.y) );
}


/******************************************************************************/
/* Load a bitmap							      */
/******************************************************************************/
VOID Load( pli)
PLOADINFO pli;
{
    ULONG     ulPostCt;

    /*
     * disable status window scrollbar
     */
    WinEnableWindow(hwndZoomScrollBar, FALSE);

    WinSetDlgItemText(hwndStatus, SID_STATUS, pszLoadMsg);
    DosPostEventSem( hevLoadMsg);

    if( hbmBitmapFile)
    {
	GpiSetBitmap( hpsBitmapFile, NULL);
	GpiDeleteBitmap( hbmBitmapFile);
    }

    if( !ReadBitmap( pli->hf) )
    {
      MyMessageBox( hwndClient, pszError);
      DosResetEventSem( hevLoadMsg, &ulPostCt);
      return;
    }

    strcpy( swctl.szSwtitle, szTitle);
    strcat( swctl.szSwtitle, ": ");
    strcat( swctl.szSwtitle, pli->szFileName);
    WinChangeSwitchEntry( hsw, &swctl);
    WinSetWindowText( hwndFrame, swctl.szSwtitle);
    ResetScrollBars();

    if(   fFirstLoad
       || (   (ADJUSTED_PBMP(pbmp2BitmapFile)->cx >
               ADJUSTED_PBMP(pbmp2BitmapFileRef)->cx)
           || (ADJUSTED_PBMP(pbmp2BitmapFile)->cy >
               ADJUSTED_PBMP(pbmp2BitmapFileRef)->cy)
           || (ADJUSTED_PBMP(pbmp2BitmapFile)->cPlanes !=
               ADJUSTED_PBMP(pbmp2BitmapFileRef)->cPlanes)
           || (ADJUSTED_PBMP(pbmp2BitmapFile)->cBitCount !=
               ADJUSTED_PBMP(pbmp2BitmapFileRef)->cBitCount) ) )
    {
      if( !fFirstLoad)
	DumpPicture();
      if( !PrepareBitmap() )
      {
	MyMessageBox( hwndClient, pszError);
	DosResetEventSem( hevLoadMsg, &ulPostCt);
	return;
      }
      CreatePicture( PICTURE_CREATE);
      bmp2BitmapFileRef = bmp2BitmapFile;
    } else
    {
      CreatePicture( PICTURE_UPDATE);
    }

    lScale = 0;

    CalcBounds();
    ptlScaleRef.x = ptlScaleRef.y = 0L;
    CalcTransform( hwndClient);

    fFirstLoad = FALSE;
    DosResetEventSem( hevLoadMsg, &ulPostCt);

    WinEnableWindow(hwndZoomScrollBar, TRUE);
    DisplayZoomFactor(lScale);
    WinSetDlgItemText(hwndStatus, SID_STATUS, pszBlankMsg);
}

/******************************************************************************/
/* Throw the pieces around the screen.					      */
/******************************************************************************/
VOID Jumble(VOID)
{
  LONG	    lWidth, lHeight;
  DATETIME  date;
  POINTL    ptl;
  RECTL     rclClient;
  PSEGLIST  psl;

  if( WinQueryWindowRect( hwndClient, &rclClient) )
  {
    lWidth  = rclClient.xRight - rclClient.xLeft;
    lHeight = rclClient.yTop   - rclClient.yBottom;
    if( (lWidth > 0) && (lHeight > 0) )
    {
      DosGetDateTime( &date);
      srand( (USHORT)date.hundredths);
      for( psl = pslHead; psl != NULL; psl = psl->pslNext)
      {
	psl->pslNextIsland = psl;    /* reset island pointer		      */
	psl->fIslandMark = FALSE;    /* clear island mark		      */
	ptl.x = rclClient.xLeft   + (rand() % lWidth);
	ptl.y = rclClient.yBottom + (rand() % lHeight);
	GpiConvert( hpsClient, CVTC_DEVICE, CVTC_MODEL, 1L, &ptl);
	ptl.x = 50 * (ptl.x / 50) - 250;
	ptl.y = 50 * (ptl.y / 50) - 250;
	psl->ptlModelXlate.x = ptl.x - psl->ptlLocation.x;
	psl->ptlModelXlate.y = ptl.y - psl->ptlLocation.y;
	SetRect( psl);
      }
    }
  }
}

/******************************************************************************/
/*                                                                            */
/*                                                                            */
/*                                                                            */
/******************************************************************************/
VOID ToBottom( pslDown)
PSEGLIST pslDown;
{
  BOOL	    fFirst;
  PSEGLIST  psl;

  for( psl = pslDown, fFirst = TRUE
     ; (psl != pslDown) || fFirst
     ; psl = psl->pslNextIsland, fFirst = FALSE )
    SegListUpdate( MAKE_HEAD_SEG, psl);       /* at head => lowest priority   */
}


/******************************************************************************/
/*                                                                            */
/* NewThread is the asynchronous drawing thread. It is responsible for all    */
/* drawing.  It will initialize its PM interface and create an application    */
/* message queue.  It will then monitor its message queue and process any     */
/* commands received.							      */
/*                                                                            */
/******************************************************************************/
VOID FAR NewThread(VOID)
{
  QMSG	    qmsgAsync, qmsgPeek;
  BOOL	    fDone;
  POINTL    aptlDraw[3];
  USHORT    usChar, fsKeyFlags;
  PSEGLIST  psl;
  ULONG     ulPostCt;
 
  /****************************************************************************/
  /* Initialize the PM interface.  If it fails, terminate both threads.       */
  /****************************************************************************/
  habAsync = WinInitialize( NULL);
  if( !habAsync)
  {
      WinPostMsg( hwndClient, WM_QUIT, NULL, NULL);
      DosExit( EXIT_THREAD, 0);
  }
 
  /****************************************************************************/
  /* Create a message queue.  If it fails, terminate both threads.	      */
  /****************************************************************************/
  hmqAsync = WinCreateMsgQueue( habAsync, 150);
  if( !hmqAsync)
  {
      WinPostMsg( hwndClient, WM_QUIT, NULL, NULL);
      WinTerminate( habAsync);
      DosExit( EXIT_THREAD, 0);
  }
 
  DosSetPrty( PRTYS_THREAD, PRTYC_NOCHANGE, sPrty, (TID)NULL);
 
 
  while( TRUE)
  {
    WinGetMsg( habAsync, &qmsgAsync, NULL, 0, 0);

    if( WinPeekMsg( habAsync, &qmsgPeek, NULL, UM_DIE, UM_DIE, PM_NOREMOVE))
	qmsgAsync = qmsgPeek;

    if( WinPeekMsg( habAsync, &qmsgPeek, NULL, UM_SIZING, UM_LOAD, PM_NOREMOVE))
    {
	DosResetEventSem( hevDrawOn, &ulPostCt);
	DosResetEventSem( hevMouse, &ulPostCt);
    }
    else
    {
	DosPostEventSem( hevDrawOn);
	DosPostEventSem( hevMouse);
    }
    if( (qmsgAsync.msg < UM_SIZING) || (qmsgAsync.msg > UM_LOAD))
	DosPostEventSem( hevMouse);
    else
	DosResetEventSem( hevMouse, &ulPostCt);

 
    /**************************************************************************/
    /* process the commands                                                   */
    /**************************************************************************/
    switch( qmsgAsync.msg)
    {
 
      /************************************************************************/
      case UM_CHAR:
	fsKeyFlags = (USHORT)SHORT1FROMMP(qmsgAsync.mp1);
	usChar	   = (USHORT)SHORT1FROMMP(qmsgAsync.mp2);
	if(   (fsKeyFlags & KC_CHAR)
	   && ((usChar == 'b') || (usChar == 'B')))
	{
	  if( psl = Correlate( &ptlMouse))
	  {
	    ToBottom( psl);
	    Redraw();
	  }
	}
	break;

      /************************************************************************/
      case UM_LOAD:
	Load( (PLOADINFO)qmsgAsync.mp1);
	Redraw();
	break;

      /************************************************************************/
      case UM_JUMBLE:
	Jumble();
	Redraw();
	break;

      /************************************************************************/
      case UM_REDRAW:
	Redraw();
	break;

      /************************************************************************/
      /* DRAW will use the passed region containing the invalidated area of   */
      /* the screen, repaint it and then destroy the region.		      */
      /************************************************************************/
      case UM_DRAW:

	if( qmsgAsync.mp1)
	{
	  DoDraw( hpsBitmapBuff, (HRGN)qmsgAsync.mp1, TRUE);
	  GpiQueryRegionBox( hpsClient, (HRGN)qmsgAsync.mp1, (PRECTL)aptlDraw);
	  GpiDestroyRegion( hpsClient, (HRGN)qmsgAsync.mp1);
	  WinMapWindowPoints( hwndClient, HWND_DESKTOP, aptlDraw, 3);
	  ROUND_DOWN_MOD( aptlDraw[0].x, lByteAlignX);	  /* round down       */
	  ROUND_DOWN_MOD( aptlDraw[0].y, lByteAlignY);	  /* round down       */
	  ROUND_UP_MOD(   aptlDraw[1].x, lByteAlignX);	  /* round up	      */
	  ROUND_UP_MOD(   aptlDraw[1].y, lByteAlignY);	  /* round up	      */
	  WinMapWindowPoints( HWND_DESKTOP, hwndClient, aptlDraw, 3);
	  aptlDraw[2] = aptlDraw[0];
	  GpiBitBlt( hpsClient
		   , hpsBitmapBuff
		   , 3L
		   , aptlDraw
		   , ROP_SRCCOPY
		   , BBO_IGNORE );
	}
        break;
 
 
      /************************************************************************/
      /* Get new scroll posn from command ( i.e. +/-1 +/-page) or new	      */
      /* absolute position from parameter, update scroll posn, change the     */
      /* transform and update the thumb posn.  Finally update the window.     */
      /************************************************************************/
      case UM_HSCROLL:
	switch( SHORT2FROMMP( qmsgAsync.mp1) )
	{
            case SB_LINEUP:
		ptsScrollPos.x -= ptsScrollLine.x;
                break;
            case SB_LINEDOWN:
		ptsScrollPos.x += ptsScrollLine.x;
                break;
	    case SB_SLIDERTRACK:
            case SB_SLIDERPOSITION:
		for( fDone = FALSE; !fDone ;)
		{
		  if( WinPeekMsg( habAsync
				, &qmsgPeek
				, NULL
				, UM_HSCROLL
				, UM_HSCROLL
				, PM_NOREMOVE))
		      if(   (SHORT2FROMMP( qmsgPeek.mp1) == SB_SLIDERTRACK)
			  ||(SHORT2FROMMP( qmsgPeek.mp1) == SB_SLIDERPOSITION))
			  WinPeekMsg( habAsync
				    , &qmsgAsync
				    , NULL
				    , UM_HSCROLL
				    , UM_HSCROLL
				    , PM_REMOVE);
		      else
			  fDone = TRUE;
		  else
		      fDone = TRUE;
		}
		ptsScrollPos.x = SHORT1FROMMP( qmsgAsync.mp1);
		ROUND_DOWN_MOD( ptsScrollPos.x, (SHORT)lByteAlignX);
                break;
            case SB_PAGEUP:
		ptsScrollPos.x -= ptsScrollPage.x;
                break;
            case SB_PAGEDOWN:
		ptsScrollPos.x += ptsScrollPage.x;
                break;
            case SB_ENDSCROLL:
                break;
            default:
                break;
	}
	DoHorzScroll();
        break;
 
      case UM_VSCROLL:
	switch( SHORT2FROMMP( qmsgAsync.mp1) )
	{
            case SB_LINEUP:
		ptsScrollPos.y -= ptsScrollLine.y;
                break;
            case SB_LINEDOWN:
		ptsScrollPos.y += ptsScrollLine.y;
                break;
	    case SB_SLIDERTRACK:
            case SB_SLIDERPOSITION:
		for( fDone = FALSE; !fDone ;)
		{
		  if( WinPeekMsg( habAsync
				, &qmsgPeek
				, NULL
				, UM_VSCROLL
				, UM_VSCROLL
				, PM_NOREMOVE))
		      if(   (SHORT2FROMMP( qmsgPeek.mp1) == SB_SLIDERTRACK)
			  ||(SHORT2FROMMP( qmsgPeek.mp1) == SB_SLIDERPOSITION))
			  WinPeekMsg( habAsync
				    , &qmsgAsync
				    , NULL
				    , UM_VSCROLL
				    , UM_VSCROLL
				    , PM_REMOVE);
		      else
			  fDone = TRUE;
		  else
		      fDone = TRUE;
		}
		ptsScrollPos.y = SHORT1FROMMP( qmsgAsync.mp1);
		ROUND_DOWN_MOD( ptsScrollPos.y, (SHORT)lByteAlignY);
                break;
            case SB_PAGEUP:
		ptsScrollPos.y -= ptsScrollPage.y;
                break;
            case SB_PAGEDOWN:
		ptsScrollPos.y += ptsScrollPage.y;
                break;
            case SB_ENDSCROLL:
                break;
            default:
                break;
	}
	DoVertScroll();
        break;
 
      /************************************************************************/
      /* the window is being resized					      */
      /************************************************************************/
      case UM_SIZING:
	CalcBounds();
	CalcTransform( hwndClient);
        break;
 
      /************************************************************************/
      /* adjust zoom factor                                                   */
      /************************************************************************/
      case UM_ZOOM:
	if( WinPeekMsg( habAsync
		      , &qmsgPeek
		      , NULL
		      , UM_SIZING
		      , UM_LOAD
		      , PM_NOREMOVE))
	    DosResetEventSem( hevDrawOn, &ulPostCt);
	else {
	    DosPostEventSem( hevDrawOn);
            }

        WinSetDlgItemText(hwndStatus, SID_STATUS, "Zooming");
        Zoom();
        WinSetDlgItemText(hwndStatus, SID_STATUS, "");
        break;

      /************************************************************************/
      /* Button down will cause a correlate on the picture to test for a hit. */
      /* Any selected segment will be highlighted and redrawn as dynamic.     */
      /************************************************************************/
      case UM_LEFTDOWN:
	if( !fButtonDownAsync)
	{
	    fButtonDownAsync = TRUE;
	    LeftDown( qmsgAsync.mp1);
	}
        break;
 
      /************************************************************************/
      /* if a segment is being dragged it will be redrawn in a new posn       */
      /************************************************************************/
      case UM_MOUSEMOVE:
#ifdef fred
	if( !fButtonDownAsync)
	    break;
#endif
	for( fDone = FALSE; !fDone ;)
	{
	  if( WinPeekMsg( habAsync	      /* look through first button-up */
			, &qmsgPeek
			, NULL
			, UM_MOUSEMOVE
			, UM_LEFTUP
			, PM_NOREMOVE))
	      if( qmsgPeek.msg == UM_MOUSEMOVE) /* only collapse move msgs    */
		  WinPeekMsg( habAsync
			    , &qmsgAsync
			    , NULL
			    , UM_MOUSEMOVE
			    , UM_MOUSEMOVE
			    , PM_REMOVE);
	      else
		  fDone = TRUE;
	  else
	      fDone = TRUE;
	}
	MouseMove( qmsgAsync.mp1);	/* process last move before button-up */
        break;
 
      /************************************************************************/
      /* if a segment is being dragged it will be redrawn as normal	      */
      /************************************************************************/
      case UM_LEFTUP:
	if( fButtonDownAsync)
	{
	    LeftUp();
	    fButtonDownAsync = FALSE;
	}
        break;
 
      /************************************************************************/
      /* destroy resources and terminate				      */
      /************************************************************************/
      case UM_DIE:
	WinDestroyMsgQueue( hmqAsync);
	WinTerminate( habAsync);
        DosExit( EXIT_THREAD, 0);
        break;
 
      /************************************************************************/
      default:
        break;
    }
  }
}
 
/******************************************************************************/
/*									      */
/******************************************************************************/
VOID CalcSize( mp1, mp2)
MPARAM mp1;
MPARAM mp2;
{
  ptsScrollMax.y = SHORT2FROMMP( mp2);
  ptsHalfScrollMax.y = ptsScrollMax.y >> 1;
  ptsScrollPage.x = ptsScrollMax.x >> 3;
  ROUND_DOWN_MOD( ptsScrollPage.x, (SHORT)lByteAlignX);
  ptsScrollLine.x = ptsScrollMax.x >> 5;
  ROUND_DOWN_MOD( ptsScrollLine.x, (SHORT)lByteAlignX);
  ptsScrollPos.y = (SHORT)(
			   (  (LONG)ptsScrollPos.y
			    * (LONG)SHORT2FROMMP(mp2)
			   )/ (LONG)SHORT2FROMMP(mp1)
			  );
  ptsOldScrollPos.y = (SHORT)(
			      (  (LONG)ptsOldScrollPos.y
			       * (LONG)SHORT2FROMMP(mp2)
			      )/ (LONG)SHORT2FROMMP(mp1)
			     );
  WinSendMsg( hwndVertScroll
	    , SBM_SETSCROLLBAR
	    , MPFROMSHORT( ptsScrollPos.y)
	    , MPFROM2SHORT( 1, ptsScrollMax.y) );

  ptsScrollMax.x = SHORT1FROMMP( mp2);
  ptsHalfScrollMax.x = ptsScrollMax.x >> 1;
  ptsScrollPage.y = ptsScrollMax.y >> 3;
  ROUND_DOWN_MOD( ptsScrollPage.y, (SHORT)lByteAlignY);
  ptsScrollLine.y = ptsScrollMax.y >> 5;
  ROUND_DOWN_MOD( ptsScrollLine.y, (SHORT)lByteAlignY);
  ptsScrollPos.x = (SHORT)(
			   (  (LONG)ptsScrollPos.x
			    * (LONG)SHORT1FROMMP(mp2)
			   )/(LONG)SHORT1FROMMP(mp1)
			  );
  ptsOldScrollPos.x = (SHORT)(
			      (  (LONG)ptsOldScrollPos.x
			       * (LONG)SHORT1FROMMP(mp2)
			      )/ (LONG)SHORT1FROMMP(mp1)
			     );
  WinSendMsg( hwndHorzScroll
	    , SBM_SETSCROLLBAR
	    , MPFROMSHORT( ptsScrollPos.x)
	    , MPFROM2SHORT( 1, ptsScrollMax.x) );
}

/******************************************************************************/
/* button down will cause one segment to be indicated and made dynamic	      */
/******************************************************************************/
VOID LeftDown( mp)
MPARAM mp;
{
  POINTL    ptl;
  HRGN	    hrgn, hrgnUpdt, hrgnUpdtDrag;
  RECTL     rcl;
  CHAR	    pszMsg[40];
  PSZ	    psz1, psz2;
  BOOL	    fFirst;
  PSEGLIST  psl;

  ptl.x = (LONG)(SHORT)SHORT1FROMMP( mp);
  ptl.y = (LONG)(SHORT)SHORT2FROMMP( mp);

  /****************************************************************************/
  /****************************************************************************/
  pslPicked = Correlate( &ptl);
  if( pslPicked)
    lPickedSeg	 = pslPicked->lSegId;
  else
  {
    fButtonDownAsync = FALSE;
    return;
  }
  if( (lPickedSeg < 1) || (lPickedSeg > lLastSegId) )
  {

    DosRequestMutexSem( hmtxSzFmt, SEM_INDEFINITE_WAIT);

    sprintf( szFmt, "Segment id out of range: %x", lPickedSeg);
    for( psz1 = szFmt, psz2 = pszMsg; *psz2++ = *psz1++; )
	;

    DosReleaseMutexSem( hmtxSzFmt);

    MyMessageBox( hwndClient, pszMsg);
    fButtonDownAsync = FALSE;
    return;
  }

  /****************************************************************************/
  ptlOffStart = pslPicked->ptlModelXlate;
  ptlMoveStart = ptl;
  GpiConvert( hpsClient, CVTC_DEVICE, CVTC_MODEL, 1L, &ptlMoveStart);
  ptlMoveStart.x = (ptlMoveStart.x / 50) * 50;
  ptlMoveStart.y = (ptlMoveStart.y / 50) * 50;
  ptlUpdtRef = ptlMoveStart;
  GpiConvert( hpsClient, CVTC_MODEL, CVTC_DEVICE, 1L, &ptlUpdtRef);

  /****************************************************************************/
  hrgnUpdt = GpiCreateRegion( hpsClient, 0L, NULL);
  for( psl = pslPicked, fFirst = TRUE
     ; (psl != pslPicked) || fFirst
     ; psl = psl->pslNextIsland, fFirst = FALSE )
  {
    rcl = psl->rclCurrent;   /* get model space bounding box of piece	      */
    GpiConvert( hpsClient, CVTC_MODEL, CVTC_DEVICE, 2L, (PPOINTL)&rcl);
    rcl.xRight++;	     /* adjust rectangle for conversion to dev space  */
    rcl.yTop++;
    rcl.xRight	+= 2;				 /* should not need	      */
    rcl.yTop	+= 2;				 /* should not need	      */
    rcl.xLeft	-= 4;				 /* should not need	      */
    rcl.yBottom -= 4;				 /* should not need	      */
    hrgn = GpiCreateRegion( hpsClient, 1L, &rcl);
    GpiCombineRegion( hpsClient, hrgnUpdt, hrgnUpdt, hrgn, CRGN_OR);
    GpiDestroyRegion( hpsClient, hrgn);
    psl->fVisible = FALSE;
  }

  GpiQueryRegionBox( hpsClient, hrgnUpdt, (PRECTL)aptlUpdt);
  WinMapWindowPoints( hwndClient, HWND_DESKTOP, aptlUpdt, 3);
  ROUND_DOWN_MOD( aptlUpdt[0].x, lByteAlignX);		  /* round down       */
  ROUND_DOWN_MOD( aptlUpdt[0].y, lByteAlignY);		  /* round down       */
  ROUND_UP_MOD(   aptlUpdt[1].x, lByteAlignX);		  /* round up	      */
  ROUND_UP_MOD(   aptlUpdt[1].y, lByteAlignY);		  /* round up	      */
  WinMapWindowPoints( HWND_DESKTOP, hwndClient, aptlUpdt, 3);
  hrgnUpdtDrag = GpiCreateRegion( hpsBitmapBuff, 1L, (PRECTL)aptlUpdt);

  aptlUpdt[2] = aptlUpdt[0];
  DoDraw( hpsBitmapBuff, hrgnUpdtDrag, TRUE);
  GpiDestroyRegion( hpsClient, hrgnUpdt);
  GpiDestroyRegion( hpsBitmapBuff, hrgnUpdtDrag);
  GpiBitBlt( hpsBitmapSave
	   , hpsBitmapBuff
	   , 3L
	   , aptlUpdt
	   , ROP_SRCCOPY
	   , BBO_IGNORE );

  /****************************************************************************/
  for( psl = pslPicked, fFirst = TRUE
     ; (psl != pslPicked) || fFirst
     ; psl = psl->pslNextIsland, fFirst = FALSE )
  {
    psl->fVisible = TRUE;
    DrawPiece( hpsBitmapBuff, psl, TRUE);
  }
  GpiBitBlt( hpsClient
	   , hpsBitmapBuff
	   , 3L
	   , aptlUpdt
	   , ROP_SRCCOPY
	   , BBO_IGNORE );
  WinSetCapture( HWND_DESKTOP, hwndClient);
}



 
/******************************************************************************/
/*                                                                            */
/* move the segment							      */
/*                                                                            */
/******************************************************************************/
VOID MouseMove( mp)
MPARAM mp;
{
  RECTL     rcl;
  POINTL    ptl, ptlModel, ptlDevice;
  POINTL    aptlUpdtRef[3], aptlUpdtNew[3];
  PSEGLIST  psl;
  BOOL	    fFirst;

  ptl.x = (LONG)(SHORT)SHORT1FROMMP( mp);
  ptl.y = (LONG)(SHORT)SHORT2FROMMP( mp);

  /****************************************************************************/
  /* clip mouse coords to client window 				      */
  /****************************************************************************/
  WinQueryWindowRect( hwndClient, &rcl);
  if (rcl.xLeft > ptl.x)
    ptl.x = rcl.xLeft;
  if (rcl.xRight <= ptl.x)
    ptl.x = rcl.xRight;
  if (rcl.yBottom > ptl.y)
    ptl.y = rcl.yBottom;
  if (rcl.yTop <= ptl.y)
    ptl.y = rcl.yTop;
  ptlMouse = ptl;

  if( !lPickedSeg || !pslPicked || !fButtonDownAsync)
    return;

  ptlModel = ptl;
  GpiConvert( hpsClient, CVTC_DEVICE, CVTC_MODEL, 1L, &ptlModel);
  ptlModel.x = 50 * (ptlModel.x / 50);
  ptlModel.y = 50 * (ptlModel.y / 50);
  if( (ptlModel.x == ptlOldMouse.x) && (ptlModel.y == ptlOldMouse.y))
    return;
  ptlOldMouse.x = ptlModel.x;
  ptlOldMouse.y = ptlModel.y;
  ptlDevice = ptlModel;
  GpiConvert( hpsClient, CVTC_MODEL, CVTC_DEVICE, 1L, &ptlDevice);

  GpiBitBlt( hpsBitmapBuff
	   , hpsBitmapSave
	   , 3L
	   , aptlUpdt
	   , ROP_SRCCOPY
	   , BBO_IGNORE );
  aptlUpdtRef[0] = aptlUpdt[0];
  aptlUpdtRef[1] = aptlUpdt[1];

  aptlUpdt[0].x += ptlDevice.x - ptlUpdtRef.x;
  aptlUpdt[0].y += ptlDevice.y - ptlUpdtRef.y;
  aptlUpdt[1].x += ptlDevice.x - ptlUpdtRef.x;
  aptlUpdt[1].y += ptlDevice.y - ptlUpdtRef.y;
  WinMapWindowPoints( hwndClient, HWND_DESKTOP, aptlUpdt, 3);
  ROUND_DOWN_MOD( aptlUpdt[0].x, lByteAlignX);		  /* round down       */
  ROUND_DOWN_MOD( aptlUpdt[0].y, lByteAlignY);		  /* round down       */
  ROUND_UP_MOD(   aptlUpdt[1].x, lByteAlignX);		  /* round up	      */
  ROUND_UP_MOD(   aptlUpdt[1].y, lByteAlignY);		  /* round up	      */
  WinMapWindowPoints( HWND_DESKTOP, hwndClient, aptlUpdt, 3);
  aptlUpdt[2] = aptlUpdt[0];
  ptlUpdtRef = ptlDevice;
  GpiBitBlt( hpsBitmapSave
	   , hpsBitmapBuff
	   , 3L
	   , aptlUpdt
	   , ROP_SRCCOPY
	   , BBO_IGNORE );


  pslPicked->ptlModelXlate.x = ptlOffStart.x + ptlModel.x - ptlMoveStart.x;
  pslPicked->ptlModelXlate.y = ptlOffStart.y + ptlModel.y - ptlMoveStart.y;

  for( psl = pslPicked, fFirst = TRUE
     ; (psl != pslPicked) || fFirst
     ; psl = psl->pslNextIsland, fFirst = FALSE )
  {
    psl->ptlModelXlate = pslPicked->ptlModelXlate;
    DrawPiece( hpsBitmapBuff, psl, TRUE);
  }

  WinUnionRect( habMain
	      , (PRECTL)aptlUpdtNew
	      , (PRECTL)aptlUpdt
	      , (PRECTL)aptlUpdtRef);
  WinMapWindowPoints( hwndClient, HWND_DESKTOP, aptlUpdtNew, 2);
  ROUND_DOWN_MOD( aptlUpdtNew[0].x, lByteAlignX);	  /* round down       */
  ROUND_DOWN_MOD( aptlUpdtNew[0].y, lByteAlignY);	  /* round down       */
  ROUND_UP_MOD(   aptlUpdtNew[1].x, lByteAlignX);	  /* round up	      */
  ROUND_UP_MOD(   aptlUpdtNew[1].y, lByteAlignY);	  /* round up	      */
  WinMapWindowPoints( HWND_DESKTOP, hwndClient, aptlUpdtNew, 2);
  aptlUpdtNew[2] = aptlUpdtNew[0];
  GpiBitBlt( hpsClient
	   , hpsBitmapBuff
	   , 3L
	   , aptlUpdtNew
	   , ROP_SRCCOPY
	   , BBO_IGNORE );
}
 
 
/******************************************************************************/
/*									      */
/* The dragged segment is being unselected.  Return it to its normal state.   */
/*									      */
/******************************************************************************/
VOID LeftUp(VOID)
{
  PSEGLIST   psl, pslTemp;
  POINTL     ptlShift;
  BOOL	     fFirst;
  LONG	     l;

  if( !lPickedSeg || !pslPicked)
    return;

  for( psl = pslPicked, fFirst = TRUE
     ; (psl != pslPicked) || fFirst
     ; psl = psl->pslNextIsland, fFirst = FALSE )
  {

    SetRect( psl);
    SegListUpdate( MAKE_TAIL_SEG, psl);       /* at tail => highest priority  */
    psl->fIslandMark = TRUE;		      /* mark as island member	      */
  }
  ptlShift = pslPicked->ptlModelXlate;

  for( psl = pslHead; psl != NULL; psl = psl->pslNext)
    if( !psl->fIslandMark)
      for( l = 0; l < 8; l++)
	if( pslPicked->lAdjacent[l] == psl->lSegId)
	  if(	(ptlShift.x == psl->ptlModelXlate.x)
	     && (ptlShift.y == psl->ptlModelXlate.y))
	  {
	    DosBeep( 600, 100);
	    DosBeep( 1200, 50);
	    MarkIsland( psl, TRUE);	      /* mark the whole new island    */
	    pslTemp = psl->pslNextIsland;     /* swap island ptrs	      */
	    psl->pslNextIsland = pslPicked->pslNextIsland;
	    pslPicked->pslNextIsland = pslTemp;
	  }
  MarkIsland( pslPicked, FALSE);	      /* unmark the island	      */

  pslPicked = NULL;
  lPickedSeg = NULL;

  WinSetCapture( HWND_DESKTOP, (HWND)NULL);
}
 
 
/******************************************************************************/
/*                                                                            */
/* DoHorzScroll will horizontally scroll the current contents of	      */
/* the client area and redraw the invalidated area			      */
/*                                                                            */
/******************************************************************************/
VOID DoHorzScroll(VOID)
{
  POINTL    aptlClient[3];
  HRGN	    hrgn;
  MATRIXLF  matlf;
 
  if( ptsScrollPos.x > ptsScrollMax.x)	   /* clip to range of scroll param   */
      ptsScrollPos.x = ptsScrollMax.x;
  if( ptsScrollPos.x < 0)
      ptsScrollPos.x = 0;
 
  if( ptsOldScrollPos.x != ptsScrollPos.x) /* only process change in position */
      WinSendMsg( hwndHorzScroll
		, SBM_SETPOS
		, MPFROM2SHORT( ptsScrollPos.x, 0)
		, MPFROMLONG( NULL));
 
  /****************************************************************************/
  /* Scroll the window the reqd amount, using bitblt'ing (ScrollWindow)       */
  /* if any of the screen still in view, and paint into uncovered region;     */
  /* else repaint the whole client area.				      */
  /****************************************************************************/
  hrgn = GpiCreateRegion( hpsClient, 0L, NULL);
  WinQueryWindowRect( hwndClient, (PRECTL)aptlClient);
  if( abs( ptsScrollPos.x - ptsOldScrollPos.x) <= ptsScrollMax.x)
  {
      WinScrollWindow( hwndClient
		     , ptsOldScrollPos.x - ptsScrollPos.x
		     , 0
		     , NULL
		     , NULL
		     , hrgn
		     , NULL
		     , 0);
  } else
  {
      GpiSetRegion( hpsClient, hrgn, 1L, (PRECTL)aptlClient);
  }
  /****************************************************************************/
  /* adjust the default view matrix					      */
  /****************************************************************************/
  GpiQueryDefaultViewMatrix( hpsClient, 9L, &matlf );
  matlf.lM31 -= ptsScrollPos.x - ptsOldScrollPos.x;
  GpiSetDefaultViewMatrix( hpsClient, 9L, &matlf, TRANSFORM_REPLACE);

  DoDraw( hpsClient, hrgn, TRUE);     /* paint into the client area	      */
  ptsOldScrollPos.x = ptsScrollPos.x;
  GpiDestroyRegion( hpsClient, hrgn);

  aptlClient[2] = aptlClient[0];
  GpiBitBlt( hpsBitmapBuff	      /* update the off-screen client image   */
	   , hpsClient
	   , 3L
	   , aptlClient
	   , ROP_SRCCOPY
	   , BBO_IGNORE );
}
 
/******************************************************************************/
/*                                                                            */
/* DoVertScroll will vertically scroll the current contents of		      */
/* the client area and redraw the invalidated area			      */
/*                                                                            */
/******************************************************************************/
VOID DoVertScroll(VOID)
{
  POINTL    aptlClient[3];
  HRGN	    hrgn;
  MATRIXLF  matlf;
 
  if( ptsScrollPos.y > ptsScrollMax.y)
      ptsScrollPos.y = ptsScrollMax.y;
  if( ptsScrollPos.y < 0)
      ptsScrollPos.y = 0;
 
  if( ptsOldScrollPos.y != ptsScrollPos.y)
      WinSendMsg( hwndVertScroll
		, SBM_SETPOS
		, MPFROM2SHORT( ptsScrollPos.y, 0)
		, MPFROMLONG( NULL));
 
  /****************************************************************************/
  /* Scroll the window the reqd amount, using bitblt'ing (ScrollWindow)       */
  /* if any of the screen still in view, and paint into uncovered region;     */
  /* else repaint the whole client area.				      */
  /****************************************************************************/
  hrgn = GpiCreateRegion( hpsClient, 0L, NULL);
  WinQueryWindowRect( hwndClient, (PRECTL)aptlClient);
  if( abs( ptsScrollPos.y - ptsOldScrollPos.y) <= ptsScrollMax.y)
  {
      WinScrollWindow( hwndClient
		     , 0
		     , ptsScrollPos.y - ptsOldScrollPos.y
		     , NULL
		     , NULL
		     , hrgn
		     , NULL
		     , 0);
  } else
  {
      GpiSetRegion( hpsClient, hrgn, 1L, (PRECTL)aptlClient);
  }
  GpiQueryDefaultViewMatrix( hpsClient, 9L, &matlf );
  matlf.lM32 += ptsScrollPos.y - ptsOldScrollPos.y;
  GpiSetDefaultViewMatrix( hpsClient, 9L, &matlf, TRANSFORM_REPLACE);

  DoDraw( hpsClient, hrgn, TRUE);
  ptsOldScrollPos.y = ptsScrollPos.y;
  GpiDestroyRegion( hpsClient, hrgn);

  aptlClient[2] = aptlClient[0];
  GpiBitBlt( hpsBitmapBuff
	   , hpsClient
	   , 3L
	   , aptlClient
	   , ROP_SRCCOPY
	   , BBO_IGNORE );
}
 
/******************************************************************************/
/*                                                                            */
/* toggle a flag and update the menu check-box				      */
/*                                                                            */
/******************************************************************************/
VOID ToggleMenuItem( usMenuMajor, usMenuMinor, pfFlag)
USHORT usMenuMajor;
USHORT usMenuMinor;
PBOOL  pfFlag;
{
  MENUITEM mi;

  WinSendMsg( WinWindowFromID( hwndFrame, FID_MENU)
	    , MM_QUERYITEM
	    , MPFROM2SHORT( usMenuMajor, FALSE)
	    , MPFROMP( (PMENUITEM)&mi));

  if( *pfFlag)
  {
    *pfFlag = FALSE;
    WinSendMsg( mi.hwndSubMenu
	      , MM_SETITEMATTR
	      , MPFROM2SHORT( usMenuMinor, TRUE)
	      , MPFROM2SHORT( MIA_CHECKED, ~MIA_CHECKED) );
  }
  else
  {
    *pfFlag = TRUE;
    WinSendMsg( mi.hwndSubMenu
	      , MM_SETITEMATTR
	      , MPFROM2SHORT( usMenuMinor, TRUE)
	      , MPFROM2SHORT( MIA_CHECKED, MIA_CHECKED) );
  }
}

/******************************************************************************/
/*                                                                            */
/* adjust zoom factor and recalc the picture transform, then do a redraw of   */
/* whole screen 							      */
/*                                                                            */
/******************************************************************************/
VOID Zoom( VOID )
{
  ULONG  ulPostKillDraw, ulPostCt;

  DosQueryEventSem(hevKillDraw, &ulPostKillDraw);

  CalcBounds();
  DosQueryEventSem(hevKillDraw, &ulPostCt);
  if (ulPostKillDraw != ulPostCt) {
    DosResetEventSem(hevKillDraw, &ulPostCt);
    return;
    }
  CalcTransform( hwndClient);
  DosQueryEventSem(hevKillDraw, &ulPostCt);
  if (ulPostKillDraw != ulPostCt) {
    DosResetEventSem(hevKillDraw, &ulPostCt);
    return;
    }
  Redraw();
}
 
/******************************************************************************/
/*                                                                            */
/* Check the segment list for obvious errors.				      */
/*									      */
/******************************************************************************/
BOOL SegListCheck( iLoc)
INT iLoc;
{
  PSEGLIST   psl;
  CHAR	     pszMsg[50];
  PSZ	     psz1, psz2;

  pszMsg[0] = '\0';
  for( psl = pslHead; psl != NULL; psl = psl->pslNext)
    if( (psl->lSegId < 1) || (psl->lSegId > lLastSegId) )
    {

      DosRequestMutexSem( hmtxSzFmt, SEM_INDEFINITE_WAIT);

      sprintf( szFmt, "Bad head segment list, location %d", iLoc);
      for( psz1 = szFmt, psz2 = pszMsg; *psz2++ = *psz1++; )
	  ;

      DosReleaseMutexSem( hmtxSzFmt);

      MyMessageBox( hwndClient, pszMsg);
      return( FALSE);
    }
  for( psl = pslTail; psl != NULL; psl = psl->pslPrev)
    if( (psl->lSegId < 1) || (psl->lSegId > lLastSegId) )
    {

      DosRequestMutexSem( hmtxSzFmt, SEM_INDEFINITE_WAIT);

      sprintf( szFmt, "Bad head segment list, location %d", iLoc);
      for( psz1 = szFmt, psz2 = pszMsg; *psz2++ = *psz1++; )
	  ;

      DosReleaseMutexSem( hmtxSzFmt);

      MyMessageBox( hwndClient, pszMsg);
      return( FALSE);
    }
  return( TRUE);
}

/******************************************************************************/
/*                                                                            */
/* DumpPicture will free the list and segment store for the picture	      */
/*                                                                            */
/******************************************************************************/
BOOL DumpPicture(VOID)
{
  while( pslHead != NULL )
  {
    GpiSetBitmap( pslHead->hpsFill, NULL);
    GpiDeleteBitmap( pslHead->hbmFill);
    GpiDestroyPS( pslHead->hpsFill);
    DevCloseDC( pslHead->hdcFill);

    GpiSetBitmap( pslHead->hpsHole, NULL);
    GpiDeleteBitmap( pslHead->hbmHole);
    GpiDestroyPS( pslHead->hpsHole);
    DevCloseDC( pslHead->hdcHole);

    SegListUpdate( DEL_SEG, pslHead);
  }

  if( hbmBitmapSize)
  {
      GpiSetBitmap( hpsBitmapSize, NULL);
      GpiDeleteBitmap( hbmBitmapSize);
  }
  if( hbmBitmapBuff)
  {
      GpiSetBitmap( hpsBitmapBuff, NULL);
      GpiDeleteBitmap( hbmBitmapBuff);
  }
  if( hbmBitmapSave)
  {
      GpiSetBitmap( hpsBitmapSave, NULL);
      GpiDeleteBitmap( hbmBitmapSave);
  }

  return( TRUE);
}

/******************************************************************************/
/*                                                                            */
/* Draw the picture into segment store. 				      */
/*                                                                            */
/******************************************************************************/
BOOL CreatePicture( sUpdate)
SHORT sUpdate;
{
  POINTL             ptl, aptlSides[12], aptlControl[12];
  SEGLIST            sl;
  PSEGLIST           psl;
  LONG	             l, lMinor, lNeighbor, alFuzz[36][4];
  SIZEL              sizl;
  BITMAPINFOHEADER2  bmp2;
  PBITMAPINFOHEADER2 pbmp2 = &bmp2;
  DATETIME           date;
  ULONG              ulPostCt;

  /****************************************************************************/
  /* compute some fuzz for the control points				      */
  /****************************************************************************/
  DosGetDateTime( &date);
  srand( (USHORT)date.hundredths);
  for( l = 0; l < 36; l++)
    for( lMinor = 0; lMinor < 4; lMinor++)
      alFuzz[l][lMinor] = 50 * (rand() % 10);

  /****************************************************************************/
  /* reset the default viewing transform to identity			      */
  /****************************************************************************/
  SetDVTransform( (FIXED)UNITY
		, (FIXED)0
		, (FIXED)0
		, (FIXED)UNITY
		, 0L
		, 0L
		, TRANSFORM_REPLACE);

  /****************************************************************************/
  /* draw the pieces							      */
  /****************************************************************************/
  lLastSegId = 0;
  for( ptl.x = ptlBotLeft.x; ptl.x < ptlTopRight.x; ptl.x += 500)
  {
    DosQueryEventSem( hevTerminate, &ulPostCt);
    if( ulPostCt)
      break;
    for( ptl.y = ptlBotLeft.y; ptl.y < ptlTopRight.y; ptl.y += 500)
    {
      DosQueryEventSem( hevTerminate, &ulPostCt);
      if( ulPostCt)
	break;
      lLastSegId++;

      /************************************************************************/
      /* compute the piece outline control points			      */
      /************************************************************************/
      aptlControl[0].x = 250L;
      aptlControl[0].y = 500L;
      aptlControl[1].x = 250;
      aptlControl[1].y = -500L;
      aptlControl[2].x = 500L;
      aptlControl[2].y = 0L;

      aptlControl[3].x = 0L;
      aptlControl[3].y = 250L;
      aptlControl[4].x = 1000L;
      aptlControl[4].y = 250L;
      aptlControl[5].x = 500L;
      aptlControl[5].y = 500L;

      aptlControl[6].x = 250L;
      aptlControl[6].y = 0L;
      aptlControl[7].x = 250L;
      aptlControl[7].y = 1000L;
      aptlControl[8].x = 0L;
      aptlControl[8].y = 500L;

      aptlControl[9].x	= 500L;
      aptlControl[9].y	= 250L;
      aptlControl[10].x = -500L;
      aptlControl[10].y = 250L;
      aptlControl[11].x = 0L;
      aptlControl[11].y = 0L;

      if( ptl.y == ptlBotLeft.y)
      {
	aptlControl[0].y = 0L;
	aptlControl[1].y = 0L;
      }

      if( (ptl.x + 500) == ptlTopRight.x)
      {
	aptlControl[3].x = 500L;
	aptlControl[4].x = 500L;
      }

      if( (ptl.y + 500) == ptlTopRight.y)
      {
	aptlControl[6].y = 500L;
	aptlControl[7].y = 500L;
      }

      if( ptl.x == ptlBotLeft.x)
      {
	aptlControl[ 9].x = 0L;
	aptlControl[10].x = 0L;
      }

      /************************************************************************/
      /* compute the adjacent segments					      */
      /************************************************************************/
      sl.lAdjacent[0] = lLastSegId - 7;
      sl.lAdjacent[1] = lLastSegId - 6;
      sl.lAdjacent[2] = lLastSegId - 5;
      sl.lAdjacent[3] = lLastSegId - 1;
      sl.lAdjacent[4] = lLastSegId + 1;
      sl.lAdjacent[5] = lLastSegId + 5;
      sl.lAdjacent[6] = lLastSegId + 6;
      sl.lAdjacent[7] = lLastSegId + 7;
      if( ptl.x == ptlBotLeft.x)
      {
	sl.lAdjacent[0] = 0;
	sl.lAdjacent[1] = 0;
	sl.lAdjacent[2] = 0;
      }
      if( ptl.y == ptlBotLeft.y)
      {
	sl.lAdjacent[0] = 0;
	sl.lAdjacent[3] = 0;
	sl.lAdjacent[5] = 0;
      }
      if( (ptl.x + 500) == ptlTopRight.x)
      {
	sl.lAdjacent[5] = 0;
	sl.lAdjacent[6] = 0;
	sl.lAdjacent[7] = 0;
      }
      if( (ptl.y + 500) == ptlTopRight.y)
      {
	sl.lAdjacent[2] = 0;
	sl.lAdjacent[4] = 0;
	sl.lAdjacent[7] = 0;
      }

      /************************************************************************/
      /* throw in some fuzz						      */
      /************************************************************************/
      if( sl.lAdjacent[3])
      {
	aptlControl[0].y  -= alFuzz[lLastSegId - 1][0];
	aptlControl[1].y  += alFuzz[lLastSegId - 1][1];
      }

      if( sl.lAdjacent[1])
      {
	aptlControl[9].x  -= alFuzz[lLastSegId - 1][2];
	aptlControl[10].x += alFuzz[lLastSegId - 1][3];
      }

      if( lNeighbor = sl.lAdjacent[4])
      {
	aptlControl[7].y  -= alFuzz[lNeighbor - 1][0];
	aptlControl[6].y  += alFuzz[lNeighbor - 1][1];
      }

      if( lNeighbor = sl.lAdjacent[6])
      {
	aptlControl[4].x  -= alFuzz[lNeighbor - 1][2];
	aptlControl[3].x  += alFuzz[lNeighbor - 1][3];
      }

      /************************************************************************/
      /* compute the piece control points in world coordinates		      */
      /************************************************************************/
      for( l=0; l<12; l++)
      {
	aptlSides[l].x = ptl.x + aptlControl[l].x;
	aptlSides[l].y = ptl.y + aptlControl[l].y;
	sl.aptlSides[l] = aptlSides[l];
      }

      /************************************************************************/
      /* compute the dimensions of the matching rects for BitBlt	      */
      /************************************************************************/
      sl.rclBitBlt.xLeft   = ptl.x - 250;
      sl.rclBitBlt.yBottom = ptl.y - 250;
      sl.rclBitBlt.xRight  = ptl.x + 750;
      sl.rclBitBlt.yTop    = ptl.y + 750;
      if( ptl.x == ptlBotLeft.x)
	sl.rclBitBlt.xLeft += 250;
      if( ptl.y == ptlBotLeft.y)
	sl.rclBitBlt.yBottom += 250;
      if( (ptl.x + 500) == ptlTopRight.x)
	sl.rclBitBlt.xRight -= 250;
      if( (ptl.y + 500) == ptlTopRight.y)
	sl.rclBitBlt.yTop -= 250;

      /************************************************************************/
      /* store the piece location					      */
      /************************************************************************/
      sl.ptlLocation = ptl;

      /************************************************************************/
      /* create the masks						      */
      /************************************************************************/
      if( sUpdate == PICTURE_CREATE)
      {
	sizl.cx = 2 + ((ADJUSTED_PBMP(pbmp2BitmapFile)->cx
		       * (sl.rclBitBlt.xRight - sl.rclBitBlt.xLeft))
		       / (ptlTopRight.x - ptlBotLeft.x));
	sizl.cy = 2 + ((ADJUSTED_PBMP(pbmp2BitmapFile)->cy
		       * (sl.rclBitBlt.yTop - sl.rclBitBlt.yBottom))
		       / (ptlTopRight.y - ptlBotLeft.y));

	bmp2    = bmp2BitmapFile;
	ADJUSTED_PBMP(pbmp2)->cx = LOUSHORT( sizl.cx);
	ADJUSTED_PBMP(pbmp2)->cy = LOUSHORT( sizl.cy);

	sl.hdcHole = DevOpenDC( habMain
			      , OD_MEMORY
			      , "*"
			      , 3L
			      , (PDEVOPENDATA)&dop
			      , NULL);
	sl.hpsHole = GpiCreatePS( habMain
				, sl.hdcHole
				, &sizl
				, PU_PELS | GPIA_ASSOC | GPIT_MICRO );
	sl.hbmHole = GpiCreateBitmap( sl.hpsHole
				    , pbmp2
				    , 0L
				    , NULL
				    , NULL);
	GpiSetBitmap( sl.hpsHole, sl.hbmHole);


	sl.hdcFill = DevOpenDC( habMain
			      , OD_MEMORY
			      , "*"
			      , 3L
			      , (PDEVOPENDATA)&dop
			      , NULL);
	sl.hpsFill = GpiCreatePS( habMain
				, sl.hdcFill
				, &sizl
				, PU_PELS | GPIA_ASSOC | GPIT_MICRO );
	sl.hbmFill = GpiCreateBitmap( sl.hpsFill
				    , pbmp2
				    , 0L
				    , NULL
				    , NULL);
	GpiSetBitmap( sl.hpsFill, sl.hbmFill);
      }


      sl.fVisible	 = TRUE;
      sl.lSegId 	 = lLastSegId;
      sl.fIslandMark	 = FALSE;
      sl.ptlModelXlate.x = sl.ptlModelXlate.y = 0L;
      if( sUpdate == PICTURE_CREATE)
      {
	sl.pslNext	   = NULL;
	sl.pslPrev	   = NULL;
	SetRect( &sl);
	psl = SegListUpdate( ADD_TAIL_SEG, &sl);
      } else
      {
	psl = SegListGet( lLastSegId);
	psl->fIslandMark = FALSE;
	for( l=0; l<12; l++)
	  psl->aptlSides[l] = aptlSides[l];
	psl->ptlModelXlate = sl.ptlModelXlate;
	SetRect( psl);
      }
      psl->pslNextIsland = psl; 	/* point to self ==> island of one    */
    }
  }
  return( TRUE);
}
 
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
VOID CheckPsl( psl)
PSEGLIST  psl;
{
  SHORT   s;

  for( s=2; s<12; s+=3)
    if( !WinPtInRect( habAsync, &psl->rclBitBlt, &psl->aptlSides[s]))
      break;
}

/******************************************************************************/
/*                                                                            */
/* Create the Size, Save and Buff bitmaps.				      */
/*                                                                            */
/******************************************************************************/
BOOL PrepareBitmap(VOID)
{
  hbmBitmapSize    = GpiCreateBitmap( hpsBitmapSize
				    , pbmp2BitmapFile
				    , 0L
				    , NULL
				    , NULL);
  if( !hbmBitmapSize)
    return( FALSE);
  GpiSetBitmap( hpsBitmapSize, hbmBitmapSize);


  bmp2BitmapSave    = bmp2BitmapFile;
  ADJUSTED_PBMP(pbmp2BitmapSave)->cx = LOUSHORT( sizlMaxClient.cx);
  ADJUSTED_PBMP(pbmp2BitmapSave)->cy = LOUSHORT( sizlMaxClient.cy);
  hbmBitmapSave     = GpiCreateBitmap( hpsBitmapSave
				    , pbmp2BitmapSave
				    , 0L
				    , NULL
				    , NULL);
  if( !hbmBitmapSave)
    return( FALSE);
  GpiSetBitmap( hpsBitmapSave, hbmBitmapSave);


  hbmBitmapBuff     = GpiCreateBitmap( hpsBitmapBuff
				    , pbmp2BitmapSave
				    , 0L
				    , NULL
				    , NULL);
  if( !hbmBitmapBuff)
    return( FALSE);
  GpiSetBitmap( hpsBitmapBuff, hbmBitmapBuff);

  return( TRUE);
}

/******************************************************************************/
/*									      */
/* Get the bitmap from disk.						      */
/* Note that there are 2 formats for bitmap files, one of which is archaic.   */
/* Both formats are supported here.  All new bitmaps should follow the format */
/* in BITMAPFILEHEADER. 						      */
/*									      */
/******************************************************************************/
BOOL ReadBitmap( hfile)
HFILE hfile;
{
    ULONG cScans;
    ULONG  cbRead;	 /* Number of bytes read by DosRead.		      */
    BOOL fRet = FALSE;	 /* Function return code.			      */
    FILESTATUS fsts;
    PBITMAPFILEHEADER2 pbfh2;

    /**************************************************************************/
    /* Find out how big the file is so we can read the whole thing in.	      */
    /**************************************************************************/

    if( DosQueryFileInfo( hfile, 1, &fsts, sizeof(FILESTATUS)))
	goto ReadBitmap_close_file;

    if( DosAllocMem( &pbfh2, fsts.cbFile, PAG_READ | PAG_WRITE | PAG_COMMIT))
	goto ReadBitmap_close_file;

    /**************************************************************************/
    /* Read the bits in from the file.					      */
    /**************************************************************************/

    if( DosRead( hfile, (PVOID)pbfh2, fsts.cbFile, &cbRead))
	goto ReadBitmap_free_bits;

    /**************************************************************************/
    /* Tell GPI to put the bits into the thread's PS. The function returns    */
    /* the number of scan lines of the bitmap that were copied.  We want      */
    /* all of them at once.						      */
    /**************************************************************************/

    ADJUSTED_PBMP(pbmp2BitmapFile)->cbFix = pbfh2->bmp2.cbFix;
    /* check to see if BMP file was an old structure or a new structure */
    if (pbfh2->bmp2.cbFix  == sizeof(BITMAPINFOHEADER)) {
        PBMP1(pbmp2BitmapFile)->cx        = PBFH1(pbfh2)->bmp.cx;
        PBMP1(pbmp2BitmapFile)->cy        = PBFH1(pbfh2)->bmp.cy;
        PBMP1(pbmp2BitmapFile)->cPlanes   = PBFH1(pbfh2)->bmp.cPlanes;
        PBMP1(pbmp2BitmapFile)->cBitCount = PBFH1(pbfh2)->bmp.cBitCount;
        }
    else {
        pbmp2BitmapFile->cx	= pbfh2->bmp2.cx;
        pbmp2BitmapFile->cy	= pbfh2->bmp2.cy;
        pbmp2BitmapFile->cPlanes   = pbfh2->bmp2.cPlanes;
        pbmp2BitmapFile->cBitCount = pbfh2->bmp2.cBitCount;
        }
    hbmBitmapFile = GpiCreateBitmap( hpsBitmapFile
			       , pbmp2BitmapFile
			       , 0L
			       , NULL
			       , NULL);
    if( !hbmBitmapFile)
        goto ReadBitmap_free_bits;
    if (GpiSetBitmap( hpsBitmapFile, hbmBitmapFile) == HBM_ERROR) {
        goto ReadBitmap_free_bits;
        }

    /* check to see if BMP file was an old structure or a new structure */
    if (pbfh2->bmp2.cbFix == sizeof(BITMAPINFOHEADER)) {
        cScans = GpiSetBitmapBits( hpsBitmapFile
        			 , 0L
        			 , (LONG) PBFH1(pbfh2)->bmp.cy
        			 , ((PBYTE)(pbfh2)) + pbfh2->offBits
        			 , (PBITMAPINFO2)&(PBFH1(pbfh2)->bmp));
        if (cScans != (LONG)PBFH1(pbfh2)->bmp.cy)  /* original number
                                                       of scans ? */
            goto ReadBitmap_free_bits;
        }
    else {
        cScans = GpiSetBitmapBits( hpsBitmapFile
        			 , 0L
        			 , (LONG) pbfh2->bmp2.cy
        			 , ((PBYTE)(pbfh2)) + pbfh2->offBits
        			 , (PBITMAPINFO2)&(pbfh2->bmp2));
        if (cScans != (LONG)pbfh2->bmp2.cy)  /* original number of scans ? */
            goto ReadBitmap_free_bits;
        }

    fRet = TRUE;


    /**************************************************************************/
    /* Close the file, free the buffer space and leave.  This is a	      */
    /* common exit point from the function.  Since the same cleanup	      */
    /* operations need to be performed for such a large number of	      */
    /* possible error conditions, this is concise way to do the right	      */
    /* thing.								      */
    /**************************************************************************/

ReadBitmap_free_bits:
    DosFreeMem( pbfh2);

ReadBitmap_close_file:
    DosClose( hfile);
    return fRet;
}
