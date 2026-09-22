#include "jigsaw.h"
#include "globals.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/******************************************************************************/
/*                                                                            */
/* Create a memory DC and an associated PS.				      */
/*                                                                            */
/******************************************************************************/
BOOL CreateBitmapHdcHps( phdc, phps)
PHDC phdc;
PHPS phps;
{
  SIZEL    sizl;
  HDC	   hdc;
  HPS	   hps;

  hdc = DevOpenDC( habMain, OD_MEMORY, "*", 3L, (PDEVOPENDATA)&dop, NULL);
  if( !hdc)
    return( FALSE);

  sizl.cx = sizl.cy = 1L;
  hps = GpiCreatePS( habMain
		   , hdc
		   , &sizl
		   , PU_PELS | GPIA_ASSOC | GPIT_MICRO );
  if( !hps)
    return( FALSE);

  *phdc = hdc;
  *phps = hps;
  return( TRUE);
}

/*****************************************************************************/
/*                                                                            */
/* Add (at head or tail) or delete a specified segment list member.	      */
/*                                                                            */
/******************************************************************************/
PSEGLIST SegListUpdate( usOperation, pslUpdate)
USHORT   usOperation;
PSEGLIST pslUpdate;
{
  PSEGLIST psl;

  switch( usOperation)
  {
    case ADD_HEAD_SEG:
      if( pslHead == NULL)
      {
        DosAllocMem( &pslHead, sizeof( SEGLIST), PAG_READ | PAG_WRITE | PAG_COMMIT);
	if( pslHead == NULL)
	  return( NULL);
	*pslHead = *pslUpdate;
	pslHead->pslPrev = NULL;
	pslHead->pslNext = NULL;
	pslTail = pslHead;
      } else
      {
        DosAllocMem( &psl, sizeof( SEGLIST), PAG_READ | PAG_WRITE | PAG_COMMIT);
	if( psl == NULL)
	  return( NULL);
	*psl = *pslUpdate;
	pslHead->pslPrev = psl;
	psl->pslNext = pslHead;
	psl->pslPrev = NULL;
	pslHead = psl;
      }
      return( pslHead);
      break;

    case ADD_TAIL_SEG:
      if( pslTail == NULL)
      {
        DosAllocMem( &pslHead, sizeof( SEGLIST), PAG_READ | PAG_WRITE |
                PAG_COMMIT);
	if( pslHead == NULL)
	  return( NULL);
	*pslHead = *pslUpdate;
	pslHead->pslPrev = NULL;
	pslHead->pslNext = NULL;
	pslTail = pslHead;
      } else
      {
        DosAllocMem( &psl, sizeof( SEGLIST), PAG_READ | PAG_WRITE |
                PAG_COMMIT);
	if( psl == NULL)
	  return( NULL);
	*psl = *pslUpdate;
	pslTail->pslNext = psl;
	psl->pslPrev = pslTail;
	psl->pslNext = NULL;
	pslTail = psl;
      }
      return( pslTail);
      break;

    case MAKE_TAIL_SEG:
      if( pslUpdate == pslTail)
	return( pslTail);
      if( pslUpdate == pslHead)
      {
	pslHead = pslHead->pslNext;
	pslHead->pslPrev = NULL;
      } else
      {
	pslUpdate->pslPrev->pslNext = pslUpdate->pslNext;
	pslUpdate->pslNext->pslPrev = pslUpdate->pslPrev;
      }
      pslTail->pslNext = pslUpdate;
      pslUpdate->pslPrev = pslTail;
      pslTail = pslUpdate;
      pslTail->pslNext = NULL;
      return( pslTail);
      break;

    case MAKE_HEAD_SEG:
      if( pslUpdate == pslHead)
	return( pslHead);
      if( pslUpdate == pslTail)
      {
	pslTail = pslTail->pslPrev;
	pslTail->pslNext = NULL;
      } else
      {
	pslUpdate->pslPrev->pslNext = pslUpdate->pslNext;
	pslUpdate->pslNext->pslPrev = pslUpdate->pslPrev;
      }
      pslHead->pslPrev = pslUpdate;
      pslUpdate->pslNext = pslHead;
      pslHead = pslUpdate;
      pslHead->pslPrev = NULL;
      return( pslHead);
      break;

    case DEL_SEG:
      for( psl = pslHead; psl != NULL; psl = psl->pslNext)
      {
	if( psl->lSegId == pslUpdate->lSegId)
	{
	  if( psl == pslHead)
	  {
	    pslHead = psl->pslNext;
	    if( pslHead == NULL)
	      pslTail = NULL;
	    else
	      pslHead->pslPrev = NULL;
	  }else if( psl == pslTail)
	  {
	    pslTail = psl->pslPrev;
	    pslTail->pslNext = NULL;
	  } else
	  {
	    (psl->pslPrev)->pslNext = psl->pslNext;
	    (psl->pslNext)->pslPrev = psl->pslPrev;
	  }
	  DosFreeMem( psl);
	  return( psl);
	  break;
	}
      }
      return( NULL);
      break;

    default:
      return( NULL);
  }
}

/******************************************************************************/
/*                                                                            */
/* set the default viewing transform					      */
/*                                                                            */
/******************************************************************************/
VOID SetDVTransform( fx11, fx12, fx21, fx22, l31, l32, lType)
FIXED fx11;
FIXED fx12;
FIXED fx21;
FIXED fx22;
FIXED l31;
FIXED l32;
FIXED lType;
{
  MATRIXLF  matlf;

  matlf.fxM11 = fx11;
  matlf.fxM12 = fx12;
  matlf.lM13  = 0L;
  matlf.fxM21 = fx21;
  matlf.fxM22 = fx22;
  matlf.lM23  = 0L;
  matlf.lM31  = l31;
  matlf.lM32  = l32;
  matlf.lM33  = 1L;
  GpiSetDefaultViewMatrix( hpsClient, 9L, &matlf, lType);
}

/******************************************************************************/
/*                                                                            */
/* Determine the bounding rect of a segment.				      */
/*                                                                            */
/* Use special knowledge that the only non-identity part of the segment       */
/* transform is the translation part.					      */
/******************************************************************************/
VOID SetRect( psl)
PSEGLIST psl;
{
  psl->rclCurrent	  = psl->rclBitBlt;	 /* world space bounding rect */
  psl->rclCurrent.xLeft   += psl->ptlModelXlate.x;
  psl->rclCurrent.yBottom += psl->ptlModelXlate.y;
  psl->rclCurrent.xRight  += psl->ptlModelXlate.x;
  psl->rclCurrent.yTop	  += psl->ptlModelXlate.y;
}
 
/******************************************************************************/
/*                                                                            */
/* Return a pointer to a segment list member, based on segment id.	      */
/*                                                                            */
/******************************************************************************/
PSEGLIST SegListGet( lSeg)
LONG lSeg;
{
  PSEGLIST  psl;

  for( psl = pslHead; psl != NULL; psl = psl->pslNext)
    if( psl->lSegId == lSeg)
      return( psl);
  return( NULL);
}

/******************************************************************************/
/*                                                                            */
/* Draw one piece.							      */
/*                                                                            */
/******************************************************************************/
VOID DrawPiece( hps, psl, fFill)
HPS      hps;
PSEGLIST psl;
BOOL     fFill;
{
    POINTL  aptl[4];

    if( psl->fVisible)
    {
      aptl[2].x = aptl[2].y = 0L;
      aptl[3] = psl->aptlBitBlt[3];
      aptl[0].x = psl->rclBitBlt.xLeft + psl->ptlModelXlate.x;
      aptl[0].y = psl->rclBitBlt.yBottom + psl->ptlModelXlate.y;
      GpiConvert( hpsClient, CVTC_MODEL, CVTC_DEVICE, 1L, aptl);
      aptl[1].x = aptl[0].x + aptl[3].x;
      aptl[1].y = aptl[0].y + aptl[3].y;
      GpiBitBlt( hps			    /* punch a hole		      */
	       , psl->hpsHole
	       , 4L
	       , aptl
	       , ROP_SRCAND
	       , BBO_IGNORE );
      if( fFill)
	GpiBitBlt( hps			    /* fill the hole		      */
		 , psl->hpsFill
		 , 4L
		 , aptl
		 , ROP_SRCPAINT
		 , BBO_IGNORE );
    }
}

/******************************************************************************/
/*                                                                            */
/* Draw the picture, using the passed region for clipping.		      */
/* Intersect the bounding box used for the clip region with the client rect.  */
/* Test each segment to see if its bounding box intersects the bounding box   */
/* of the clipping region.  Draw only if there is an intersection.	      */
/*                                                                            */
/******************************************************************************/
BOOL DoDraw( hps, hrgn, fPaint)
HPS  hps;
HRGN hrgn;
BOOL fPaint;
{
  HRGN	    hrgnOld;
  RECTL     rcl, rclRegion, rclDst, rclClient;
  PSEGLIST  psl;
  ULONG     ulPostCt;

  if( fPaint)
  {
    GpiSetColor( hps, CLR_BACKGROUND);
    GpiPaintRegion( hps, hrgn); 		/* erase region 	      */
  }
  GpiQueryRegionBox( hps, hrgn, &rclRegion);
  WinQueryWindowRect( hwndClient, &rclClient);
  if( !WinIntersectRect( habAsync, &rclRegion, &rclRegion, &rclClient))
    return( FALSE);				/* not in client window       */
  GpiSetClipRegion( hps, hrgn, &hrgnOld);	/* make the clip region       */
  for( psl = pslHead; psl != NULL; psl = psl->pslNext) /* scan all pieces     */
  {
    /**************************************************************************/
    /* get the piece bounding box in device coordinates 		      */
    /**************************************************************************/
    rcl = psl->rclCurrent;
    GpiConvert( hpsClient, CVTC_MODEL, CVTC_DEVICE, 2L, (PPOINTL)&rcl);
    rcl.xRight++;
    rcl.yTop++;
    /**************************************************************************/
    /* if the piece might be visible, and drawing allowed, draw the piece     */
    /**************************************************************************/
    if( WinIntersectRect( habAsync, &rclDst, &rcl, &rclRegion))
        DosQueryEventSem( hevDrawOn, &ulPostCt);
	if( ulPostCt)
	  DrawPiece( hps, psl, TRUE);
	else
	  break;
  }
  GpiSetClipRegion( hps, NULL, &hrgnOld);
  DosQueryEventSem( hevDrawOn, &ulPostCt);
  if( ulPostCt)
    GpiSetRegion( hpsClient, hrgnInvalid, 0L, NULL);

  return( TRUE);
}
 
/******************************************************************************/
/*                                                                            */
/* get bounding rect of whole picture in model coordinates		      */
/*                                                                            */
/******************************************************************************/
VOID CalcBounds(VOID)
{
  PSEGLIST  psl;
  RECTL     rclOld;
  ULONG     ulPostKillDraw, ulPostCt;

  if( !pslHead)
    return;
  rclBounds = rclOld = pslHead->rclCurrent;

  /*
   * get start killdraw post count
   */
  DosQueryEventSem(hevKillDraw, &ulPostKillDraw);

  for( psl = pslHead->pslNext; psl != NULL; psl = psl->pslNext) {
    DosQueryEventSem(hevKillDraw, &ulPostCt);
    if (ulPostKillDraw != ulPostCt) {
        rclBounds = rclOld;
        return;
        }
    WinUnionRect(habAsync, &rclBounds, &rclBounds, &(psl->rclCurrent));
    }
}

/******************************************************************************/
/*                                                                            */
/* Calculate and set the default viewing transform based on zoom and scroll   */
/*                                                                            */
/******************************************************************************/
VOID CalcTransform( hwnd)
HWND hwnd;
{
  RECTL     rclClient;
  POINTL    ptlCenter, ptlTrans, ptlScale, aptl[4], ptlOrg, aptlSides[12];
  PSEGLIST  psl;
  LONG	    l;
  MATRIXLF  matlf;
  ULONG     ulPostCt, ulPostKillDraw;


  /*
   * get start killdraw post count
   */
  DosQueryEventSem(hevKillDraw, &ulPostKillDraw);


  /****************************************************************************/
  /* from bounding rect of picture get center of picture		      */
  /****************************************************************************/
  ptlCenter.x = (rclBounds.xLeft   + rclBounds.xRight) / 2;
  ptlCenter.y = (rclBounds.yBottom + rclBounds.yTop  ) / 2;
 
  /****************************************************************************/
  /* translate center of picture to origin				      */
  /****************************************************************************/
  SetDVTransform( (FIXED)UNITY
		, (FIXED)0
		, (FIXED)0
		, (FIXED)UNITY
		, -ptlCenter.x
		, -ptlCenter.y
		, TRANSFORM_REPLACE);
 
  /****************************************************************************/
  /* scale down to 1:1 of bitmap in file				      */
  /****************************************************************************/
  ptlScale.x = UNITY * ADJUSTED_PBMP(pbmp2BitmapFile)->cx
          / (ptlTopRight.x - ptlBotLeft.x);
  ptlScale.y = UNITY * ADJUSTED_PBMP(pbmp2BitmapFile)->cy
          / (ptlTopRight.y - ptlBotLeft.y);
 
  /****************************************************************************/
  /* add in zoom scale							      */
  /****************************************************************************/
  ptlScale.x += ptlScale.x * lScale / (ZOOM_MAX + 1);
  ptlScale.y += ptlScale.y * lScale / (ZOOM_MAX + 1);

  SetDVTransform( (FIXED)ptlScale.x
		, (FIXED)0
		, (FIXED)0
		, (FIXED)ptlScale.y
		, 0L
		, 0L
		, TRANSFORM_ADD);
 
  /****************************************************************************/
  /* translate center of picture to center of client window		      */
  /****************************************************************************/
  WinQueryWindowRect( hwnd, &rclClient);
  ptlTrans.x = (rclClient.xRight - rclClient.xLeft)   / 2;
  ptlTrans.y = (rclClient.yTop	 - rclClient.yBottom) / 2;
 
  /****************************************************************************/
  /* add in horizontal and vertical scrolling factors			      */
  /****************************************************************************/
  ptlTrans.x -= ptsScrollPos.x - ptsHalfScrollMax.x;
  ptlTrans.y += ptsScrollPos.y - ptsHalfScrollMax.y;
  SetDVTransform( (FIXED)UNITY
		, (FIXED)0
		, (FIXED)0
		, (FIXED)UNITY
		, ptlTrans.x
		, ptlTrans.y
		, TRANSFORM_ADD);

  GpiQueryDefaultViewMatrix( hpsClient, 9L, &matlf);
  GpiSetDefaultViewMatrix( hpsClient, 9L, &matlf, TRANSFORM_REPLACE);

  /****************************************************************************/
  /* create bitmaps for pieces						      */
  /****************************************************************************/

  ptlOffset = ptlBotLeft;		   /* BottomLeft corner in dev coord  */
  GpiConvert( hpsClient, CVTC_WORLD, CVTC_DEVICE, 1L, &ptlOffset);
  if( (ptlScale.x != ptlScaleRef.x) || (ptlScale.y != ptlScaleRef.y))
  {
    ptlScaleRef = ptlScale;

    /**************************************************************************/
    /* create a shadow bitmap of the original, sized to current output size   */
    /**************************************************************************/
    aptl[0] = ptlBotLeft;		   /* current output rect, dev coord  */
    aptl[1] = ptlTopRight;
    GpiConvert( hpsClient, CVTC_WORLD, CVTC_DEVICE, 2L, aptl);

    aptl[0].x -= ptlOffset.x;		   /* put bottom left at (0,0)	      */
    aptl[0].y -= ptlOffset.y;
    aptl[1].x -= ptlOffset.x;
    aptl[1].y -= ptlOffset.y;
#ifdef fred
    aptl[1].x -= ptlOffset.x - 1;	   /* correct for coordinate convert  */
    aptl[1].y -= ptlOffset.y - 1;
#endif
    aptl[2].x = 0L;
    aptl[2].y = 0L;
    aptl[3].x = ADJUSTED_PBMP(pbmp2BitmapFile)->cx;	   /* bitmap dimensions 	      */
    aptl[3].y = ADJUSTED_PBMP(pbmp2BitmapFile)->cy;
    GpiSetBitmap( hpsBitmapSize, hbmBitmapSize);
    GpiBitBlt( hpsBitmapSize		   /* copy the bitmap		      */
	     , hpsBitmapFile
	     , 4L
	     , aptl
	     , ROP_SRCCOPY
	     , BBO_IGNORE);

    for( psl = pslHead; psl != NULL; psl = psl->pslNext)
    {
      DosQueryEventSem( hevTerminate, &ulPostCt);
      if( ulPostCt) /* exit if quit   */
	break;

      DosQueryEventSem( hevKillDraw, &ulPostCt);
      if( ulPostCt != ulPostKillDraw) {
	break;
        }

      aptl[0].x = psl->rclBitBlt.xLeft;     /* bounding rect in world space   */
      aptl[0].y = psl->rclBitBlt.yBottom;
      aptl[1].x = psl->rclBitBlt.xRight;
      aptl[1].y = psl->rclBitBlt.yTop;
      aptl[2] = aptl[0];
      aptl[3] = aptl[1];
      GpiConvert( hpsClient, CVTC_WORLD, CVTC_DEVICE, 2L, &aptl[2]);
      ptlOrg = aptl[2];
      aptl[3].x -= ptlOrg.x - 1;	    /* bitmap rect of piece	      */
      aptl[3].y -= ptlOrg.y - 1;
      aptl[2].x = 0L;
      aptl[2].y = 0L;
      psl->aptlBitBlt[0] = aptl[0];
      psl->aptlBitBlt[1] = aptl[1];
      psl->aptlBitBlt[2] = aptl[2];
      psl->aptlBitBlt[3] = aptl[3];

      /************************************************************************/
      /* compute the piece control points				      */
      /************************************************************************/
      for ( l = 0; l < 12; l++)
	aptlSides[l] = psl->aptlSides[l];
      GpiConvert( hpsClient, CVTC_WORLD, CVTC_DEVICE, 12L, aptlSides);
      for ( l = 0; l < 12; l++)
      {
	aptlSides[l].x -= ptlOrg.x;
	aptlSides[l].y -= ptlOrg.y;
      }

      /************************************************************************/
      /* prepare the mask to punch a hole in the output bitmap		      */
      /************************************************************************/
      GpiSetClipPath( psl->hpsHole, 0L, SCP_RESET);  /* no clip path	      */
      GpiBitBlt( psl->hpsHole			     /* fill with 1's         */
	       , NULL
	       , 2L
	       , &psl->aptlBitBlt[2]
	       , ROP_ONE
	       , BBO_IGNORE);

      GpiBeginPath( psl->hpsHole, 1L);		     /* define a clip path    */
      GpiMove( psl->hpsHole, &aptlSides[11]);
      GpiPolySpline( psl->hpsHole, 12L, aptlSides);
      GpiEndPath( psl->hpsHole);
      GpiSetClipPath( psl->hpsHole, 1L, SCP_AND);
      GpiBitBlt( psl->hpsHole			     /* fill with 0's         */
	       , NULL
	       , 2L
	       , &psl->aptlBitBlt[2]
	       , ROP_ZERO
	       , BBO_IGNORE);
      GpiSetClipPath( psl->hpsHole, 0L, SCP_RESET);  /* clear the clip path   */

      /************************************************************************/
      /* prepare the mask to fill the hole in the output bitmap 	      */
      /************************************************************************/
      aptl[0] = psl->aptlBitBlt[2];
      aptl[1] = psl->aptlBitBlt[3];
      aptl[2] = aptl[0];
      GpiBitBlt( psl->hpsFill			     /* make inverse of hole  */
	       , psl->hpsHole
	       , 3L
	       , aptl
	       , ROP_NOTSRCCOPY
	       , BBO_IGNORE);

      aptl[0] = psl->aptlBitBlt[2];
      aptl[1] = psl->aptlBitBlt[3];
      aptl[2].x = ptlOrg.x - ptlOffset.x;	     /* pick the right part   */
      aptl[2].y = ptlOrg.y - ptlOffset.y;	     /* of the sized bitmap   */
      GpiBitBlt( psl->hpsFill			     /* fill with data	      */
	       , hpsBitmapSize
	       , 3L
	       , aptl
	       , ROP_SRCAND
	       , BBO_IGNORE);
      GpiSetClipPath( psl->hpsFill, 0L, SCP_RESET);  /* clear the clip path   */

      GpiSetColor( psl->hpsFill, CLR_RED);	     /* draw the outline      */
      GpiMove( psl->hpsFill, &aptlSides[11]);
      GpiPolySpline( psl->hpsFill, 12L, aptlSides);
      DrawPiece( hpsClient, psl, TRUE);
    }
  }
}
 
/******************************************************************************/
/*                                                                            */
/* Redraw the entire client window.					      */
/*                                                                            */
/******************************************************************************/
VOID Redraw(VOID)
{
  RECTL   rclInvalid;
  HRGN	  hrgnUpdt;
  POINTL  aptlUpdtNew[3];
 
  WinQueryWindowRect( hwndClient, &rclInvalid);
  hrgnUpdt = GpiCreateRegion( hpsBitmapBuff, 1L, &rclInvalid);
  DoDraw( hpsBitmapBuff, hrgnUpdt, TRUE);
  GpiDestroyRegion( hpsBitmapBuff, hrgnUpdt);
  aptlUpdtNew[0].x = rclInvalid.xLeft;
  aptlUpdtNew[0].y = rclInvalid.yBottom;
  aptlUpdtNew[1].x = rclInvalid.xRight;
  aptlUpdtNew[1].y = rclInvalid.yTop;
  ROUND_DOWN_MOD( aptlUpdtNew[0].x, lByteAlignX);	  /* round down       */
  ROUND_DOWN_MOD( aptlUpdtNew[0].y, lByteAlignY);	  /* round down       */
  ROUND_UP_MOD(   aptlUpdtNew[1].x, lByteAlignX);	  /* round up	      */
  ROUND_UP_MOD(   aptlUpdtNew[1].y, lByteAlignY);	  /* round up	      */
  aptlUpdtNew[2] = aptlUpdtNew[0];
  GpiBitBlt( hpsClient
	   , hpsBitmapBuff
	   , 3L
	   , aptlUpdtNew
	   , ROP_SRCCOPY
	   , BBO_IGNORE );
}

/******************************************************************************/
/* perform bitmap-based correlation					      */
/******************************************************************************/
PSEGLIST Correlate( pptl)
PPOINTL pptl;
{
  PSEGLIST  psl;
  POINTL    aptl[2];
  LONG	    lColor;
  RECTL     rcl;


  aptl[0] = aptl[1] = *pptl;
  aptl[1].x++;
  aptl[1].y++;
  GpiBitBlt( hpsBitmapSave, NULL, 2L, aptl, ROP_ONE, BBO_IGNORE);
  lColor = GpiQueryPel( hpsBitmapSave, pptl);
  for( psl = pslTail; psl != NULL; psl = psl->pslPrev)
  {
    rcl = psl->rclCurrent;
    GpiConvert( hpsClient, CVTC_MODEL, CVTC_DEVICE, 2L, (PPOINTL)&rcl);
    rcl.xRight++;
    rcl.yTop++;
    if( WinPtInRect( habAsync, &rcl, pptl))    /*  is point in bounding box?  */
    {
	DrawPiece( hpsBitmapSave, psl, FALSE);
	if( GpiQueryPel( hpsBitmapSave, pptl) != lColor)
	    break;			       /*  got a hit		      */
    }
  }
  return( psl);
}

/******************************************************************************/
/*									      */
/*  MyMessageBox							      */
/*									      */
/*  Displays a message box with the given string.  To simplify matters,       */
/*  the box will always have the same title ("Jigsaw"), will always	      */
/*  have a single button ("Ok"), will always have an exclamation point	      */
/*  icon, and will always be application modal. 			      */
/*									      */
/******************************************************************************/
VOID MyMessageBox( hWnd, psz)
HWND hWnd;
PSZ  psz;
{
    WinMessageBox( HWND_DESKTOP
		 , hWnd
		 , psz
		 , szTitle
		 , NULL
		 , MB_OK | MB_ICONEXCLAMATION | MB_APPLMODAL );
}

/******************************************************************************/
/*                                                                            */
/* mark a whole island							      */
/*                                                                            */
/******************************************************************************/
VOID MarkIsland( pslMark, fMark)
PSEGLIST pslMark;
BOOL     fMark;
{
  PSEGLIST  psl;
  BOOL	    fFirst;

  for( psl = pslMark, fFirst = TRUE
     ; (psl != pslMark) || fFirst
     ; psl = psl->pslNextIsland, fFirst = FALSE )
      psl->fIslandMark = fMark; 	      /* mark as island member	      */
}


/******************************************************************************/
/* Display Zoom factor							      */
/******************************************************************************/
VOID DisplayZoomFactor( lScaleFactor)
LONG lScaleFactor;
    {

    sprintf(szZoomFact, "1:%d", -lScaleFactor + 1);
    WinSetDlgItemText(hwndStatus, SID_ZOOMFACT, szZoomFact);
    WinSendMsg(hwndZoomScrollBar, SBM_SETPOS,
            MPFROMSHORT((USHORT) -lScaleFactor), 0L);

    }
