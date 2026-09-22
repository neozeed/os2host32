/*
    biopaint.c	-   WM_PAINT processing and calendar conversion routines

    Created by Microsoft, IBM Corporation, 1990
*/
#define INCL_WIN
#define INCL_GPI
#include <os2.h>

#include "bio.h"
#include <math.h>
#include <stdio.h>

/* Read-only global variables */
extern double   Born;
extern long     Day, SelectDay;
extern BOOL     bBorn;
extern FONTMETRICS tmFontInfo;
extern SHORT	LinesPerPage;
extern RECTL    rclClient;
extern SHORT	cxDateField;

/* Read-only static variables */
static double   Cycle[] = { 23.0, 28.0, 33.0 };
static char     cDayOfWeek[] = "MTWTFSS";
extern LONG     Color[];



/*  APPPaint() - Parent window WM_PAINT processing routine.
*
*   Purpose:
*       Routine to graph biorhythm cycles and tabulate dates.
*
*   Arguments:
*       hWnd          - Handle of Window owning message
*       message       - Message itself
*       mp1           - Extra message-dependent info
*       mp2           - Extra message-dependent info
*
*   Globals (static):
*       Cycle[]       - Array holding period for phy/emot/int: 23,28,33
*       cDayOfWeek[]  - Array of chars holding first letter of days of week.
*       Color[]       - Set of colored pens used to identify cycles.
*
*   Globals (referenced):
*       Born          - Birthdate in julian days.  Read from WIN.INI.
*	SelectDay     - Current day being tracked, day is highlighted.
*			This is stored as the number of days from birthdate.
*			Initialized to present day in WM_CREATE processing.
*       Day           - Day number from date born which is top line being
*                       displayed.  Initially three days before SelectDay.
*       bBorn         - Boolean indicating whether valid birtdate entered or
*	rclClient     - Size of client area defined by WM_SIZE message
*       LinesPerPage  - Number of system font lines on client area, defined
*                       by WM_SIZE message handling
*       tmFontInfo    - Text Metric structure defined during WM_CREATE 
*
*   Description:
*       Tabulates dates and graphs cycles.  On color displays, weekends
*       are written in red.  The update rectangle is used to minimize
*       repaint time of affected client area.
*/
VOID APPPaint( hWnd )
HWND   hWnd;
{
    HPS         hPS;
    POINTL      ptl;
    SHORT	y, i;
    SHORT	start, last;
    char        szDay[16];
    SHORT	Amplitude, offset;
    SHORT	year, month;
    double      day;
    RECTL       rc, rcClip;
    SHORT	DayOfWeek;
    HRGN	hrgnClip;
    POINTL	ptlTextBox[5];

    hPS = WinBeginPaint( hWnd, NULL, &rcClip );

    /* Erase client area */
    WinQueryWindowRect( hWnd, &rc );
    WinFillRect( hPS, &rc, CLR_WHITE );

    /* Label parts of table and graph. */
    ptl.y = rclClient.yTop - tmFontInfo.lMaxBaselineExt + /* Top line */
            tmFontInfo.lMaxDescender;
    ptl.x = 0;
    GpiCharStringAt( hPS, &ptl, 7L, (PCH)"   DATE" );
    ptl.x = cxDateField + tmFontInfo.lAveCharWidth;
    GpiCharStringAt( hPS, &ptl, 3L, (PCH)"LOW" );
    GpiQueryTextBox( hPS, 4L, "HIGH", TXTBOX_COUNT, ptlTextBox );
    ptl.x = rclClient.xRight - ptlTextBox[TXTBOX_CONCAT].x - tmFontInfo.lAveCharWidth;
    GpiCharStringAt( hPS, &ptl, 4L, (PCH)"HIGH" );

    /* Underline labels from left to right across client area */
    ptl.y = rclClient.yTop - tmFontInfo.lMaxBaselineExt;
    ptl.x = 0;
    GpiMove( hPS, &ptl );
    ptl.x = rclClient.xRight;
    GpiLine( hPS, &ptl );

    /* Draw a vertical line separator between dates and cycles */
    ptl.y = rclClient.yTop;
    ptl.x = cxDateField;
    GpiMove( hPS, &ptl );
    ptl.y = rclClient.yBottom;
    GpiLine( hPS, &ptl );

    /* Draw a dotted vertical center line to reference cycles */
    GpiSetLineType( hPS, LINETYPE_DOT );
    ptl.x = (cxDateField + rclClient.xRight) / 2;
    GpiMove( hPS, &ptl );
    ptl.y = rclClient.yTop;
    GpiLine( hPS, &ptl );
    /* (Should not have to restore line type after EndPaint) */
    GpiSetLineType( hPS, LINETYPE_DEFAULT );

    /* Update only the range of lines which fall into update rectangle */
    start = (int)((rclClient.yTop - rcClip.yTop) / tmFontInfo.lMaxBaselineExt);
    if (start<1)
       start = 1;
    last = (int)((rclClient.yTop - rcClip.yBottom) / tmFontInfo.lMaxBaselineExt);
    if (last>(LinesPerPage-1))
       last = LinesPerPage-1;
    
    /* Set clip rectangle to completely draw entire rectangle representing
       each date affected.  Start drawing one day before and after
       (outside clip rectangle) so that cycle lines will connect correctly
       with unaffected lines. */
    rcClip.yTop = rclClient.yTop - start*tmFontInfo.lMaxBaselineExt;
    start--;
    last++;
    rcClip.yBottom = rclClient.yTop - last*tmFontInfo.lMaxBaselineExt + 1;
    hrgnClip = GpiCreateRegion( hPS, 1L, &rcClip );
    GpiSetClipRegion( hPS, hrgnClip, &hrgnClip );

    /* List days and date */
    for (y=start; y<=last; y++) {
        /* Get the calendar date from julian day */
        calendar( Born+Day+y-1, &year, &month, &day );
        /* Get offset into days of the week initials array */
        DayOfWeek = (int)((LONG)(Born+Day+y) % 7);
        /* Assemble each of the parts in a buffer */
	sprintf(szDay, " %02d-%02d-%02d",
                month, (int)day, year - (trunc4((double)year / 100)*100) );
        /* If color available, draw weekends in red */
        if (DayOfWeek > 4)
           GpiSetColor( hPS, CLR_RED );
        ptl.x = 0;
        ptl.y = rclClient.yTop - ((y+1)*tmFontInfo.lMaxBaselineExt -
		tmFontInfo.lMaxDescender);
	GpiCharStringAt( hPS, &ptl, 1L, (PCH)&cDayOfWeek[DayOfWeek] );
	GpiQueryWidthTable( hPS, (LONG)'W', 1L, &ptl.x );
	GpiCharStringAt( hPS, &ptl, 9L, (PCH)szDay );
        GpiSetColor( hPS, CLR_BLACK );
    }

    /* Amplitude of sin wave is half client area minus space for dates */
    Amplitude = (int)((rclClient.xRight - cxDateField - tmFontInfo.lAveCharWidth) >> 1);
    /* Move to right, make room for column of dates */
    offset = (int)(Amplitude + cxDateField + tmFontInfo.lAveCharWidth - (tmFontInfo.lAveCharWidth>>1));
    for (i=0; i<3 && bBorn; i++ ) {
        GpiSetColor( hPS, Color[i] );
        for (y=start; y<=last; y++) {
            ptl.x = (int)(sin( (y+Day-1)/Cycle[i]*2*3.14159 ) * Amplitude + offset);
            ptl.y = rclClient.yTop - (y*tmFontInfo.lMaxBaselineExt +
                        tmFontInfo.lMaxBaselineExt/2);
            if ((y+Day-1 > 0) && (y>start))
               GpiLine( hPS, &ptl );
            else
               GpiMove( hPS, &ptl );
        }
    }

    /* Draw highlight on selected day if visible. */
    if ((SelectDay >= Day) && (SelectDay - Day < LinesPerPage - 1)) {
        rc.xRight = rclClient.xRight;
        rc.xLeft = rclClient.xLeft;
        rc.yTop = rclClient.yTop - (int)(SelectDay - Day + 1) * tmFontInfo.lMaxBaselineExt;
        rc.yBottom = rc.yTop - tmFontInfo.lMaxBaselineExt + 1;
        WinInvertRect( hPS, &rc );
    }

    WinEndPaint( hPS );

    return;
}


/*  julian() - Compute julian date from Gregorian calendar date.
*
*   Purpose:
*       Provide a standard time base.
*
*   Arguments:
*       year          - Calendar year
*       month         - Calendar month
*       day           - Calendar day and fraction
*
*   Return Value:
*       double        - Julian date converted
*
*   Description:
*       Convert Gregorian dates to Julian Days.  Refer to Alamanac for
*       Computers (1978), p. B2, Naval Observatory Pub.
*
*   Limits:
*       Valid between ~1900 and 2099.
*
*/

double julian (year, month, day)
SHORT year, month;
double day;
{
  double dj;
  double fracDay, intDay;

  fracDay = modf(day, &intDay);
  dj = (long)367*year - 7*(year + (month+9) / 12) / 4 + 275*month / 9 +
       intDay + 1721013.5 + fracDay;
  return dj;
}


/*  calendar() - Compute Gregorian calendar date from julian date.
*
*   Purpose:
*       Provide a standard time base.
*
*   Arguments:
*       juldate       - Julian date to convert
*       year          - Calendar year result
*       month         - Calendar month result
*       day           - Calendar day and fraction result
*
*   Return Value:
*       void
*
*   Globals (modified):
*       none
*
*   Globals (referenced):
*       none
*
*   Description:
*       Convert Julian Days to Gregorian date.  Refer to Astronomical
*       Formulae for Calculators (1979), p. 23, by Jean Meeus.
*
*   Limits:
*       Valid for positive Julian Day values.
*
*/

void calendar (juldate, year, month, day)
double juldate;
SHORT *year;
SHORT *month;
double *day;
{
  long b, c, d, e, z, alf;

  juldate = juldate + 0.5;
  z = trunc4(juldate);
  alf = trunc4((z - 1867216.25)/36524.25);
  b = z + 1 + alf - alf / 4 + 1524;
  c = trunc4((b - 122.1)/365.25);
  d = 365*c + c / 4;
  e = trunc4((b - d)/30.6001);
  *day = b - d - trunc4(30.6001*e) + juldate - z;
  if (e > 13)
      *month = (int)e - 13;
  else
      *month = (int)e - 1;
  if (*month > 2)
      *year = (int)c - 4716;
  else
      *year = (int)c - 4715;
}

long trunc4( dflValue )
double dflValue;
{
   double intValue;
   modf(dflValue, &intValue);
   return (long)intValue;
}
