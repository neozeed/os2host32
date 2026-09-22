/*  BioDlg() - Dialog Box routine.
*
*   Created by Microsoft, IBM Corporation, 1990
*
*   Purpose:
*       Allow setting of birthdate and viewing date for biorhythm display.
*
*   Arguments:
*       hDlg          - Handle of Dialog Box owning message
*       message       - Message itself
*       mp1           - Extra message-dependent info
*       mp2           - Extra message-dependent info
*
*   Globals (modified):
*       Born          - Birthdate in julian days.  Read from OS2.INI.
*       SelectDay     - Current day being tracked, day is highlighted.
*			This is stored as # days from birthdate.
*			This is initialized to the current date in WM_CREATE.
*       Day           - Day number from date born which is top line being
*                       displayed.  Initially three days before SelectDay.
*       bBorn         - Boolean indicating whether valid birtdate entered or
*                       defined in OS2.INI.  Nothing graphed until valid.
*
*   Globals (referenced):
*       hAB           - Handle to the Anchor Block
*       szAppName[]   - RC file program name (Biorhythm).
*
*   Description:
*       Biorythm cycles start on the date of birth and the state of
*       of these cycles may be viewed on the selected date.  A check
*       box is provided to update (record) the birthdate in the WIN.INI
*       file so that it will be automatically available in subsequent
*       sessions.
*
*   Limits:
*       Minor error checking is provided when OK is selected to make
*       sure that the dates specified fall in the 20th and 21st
*       centuries.  No error checking is attempted to verify correct
*       month or day of month entries. 
*
*/

#define INCL_WIN
#include <os2.h>

#include "bio.h"
#include <math.h>
#include <stdio.h>

/* Read-only global variables */
extern HAB      hAB;
extern char     szAppName[];

/* Global variables (modified) */
extern long     SelectDay, Day;
extern double   Born;
extern BOOL     bBorn;

MRESULT EXPENTRY BioDlg( hDlg, message, mp1, mp2 )
HWND    hDlg;
USHORT  message;
MPARAM  mp1;
MPARAM  mp2;
{
    switch( message ) {
        case WM_INITDLG:
	    InitBioDlg(hDlg);
	    break;

        case WM_COMMAND:
	    BioDlgCmd(hDlg, mp1);
	    break;

        default:
            return( WinDefDlgProc( hDlg, message, mp1, mp2 ) );

    }
    return 0L;
}


/*  About() - General purpose About dialog box.
*
*   Purpose:
*       Provide program propoganda.
*
*   Arguments:
*       hDlg          - Handle of Dialog Box owning message
*       message       - Message itself
*       mp1           - Extra message-dependent info
*       mp2           - Extra message-dependent info
*/

MRESULT EXPENTRY About( hWndDlg, message, mp1, mp2 )
HWND   hWndDlg;
USHORT message;
MPARAM  mp1;
MPARAM  mp2;
{
    switch( message )
    {
      case WM_COMMAND:
        switch( LOUSHORT( mp1 ) )
        {
          case DID_OK:
            WinDismissDlg( hWndDlg, TRUE );
            break;

          default:
            break;
        }
        break;

      default:
        return( WinDefDlgProc( hWndDlg, message, mp1, mp2 ) );
    }
    return( FALSE );
}
/*
*/

VOID InitBioDlg(HWND hDlg)
{
/*
     If valid OS2.INI info, fill in birthdate edit fields
*/
    USHORT	year, month;
    double      day;

    if (bBorn) {
      calendar( Born, (SHORT *)&year, (SHORT *)&month, &day );
      WinSetDlgItemShort( hDlg, ID_BDYEAR, year, FALSE );
      WinSetDlgItemShort( hDlg, ID_BDMONTH, month, FALSE );
      WinSetDlgItemShort( hDlg, ID_BDDAY, (int)day, FALSE );
    }
    /* Display current date or date highlighted */
    calendar( Born+SelectDay, (SHORT *)&year, (SHORT *)&month, &day );
    WinSetDlgItemShort( hDlg, ID_YEAR, year, FALSE );
    WinSetDlgItemShort( hDlg, ID_MONTH, month, FALSE );
    WinSetDlgItemShort( hDlg, ID_DAY, (int)day, FALSE );
}
/*
*/

VOID BioDlgCmd( HWND hDlg, MPARAM mp1 )
{
/*
    Bio Dialog Box routine WM_COMMAND processor
*/
    USHORT	year, month, iDay;
    double      day;
    char        szBuf[10];

    switch( LOUSHORT( mp1 ) ) {
	case DID_OK:
	    /* Get the birthday edit field values */
	    WinQueryDlgItemShort( hDlg, ID_BDYEAR, &year, FALSE );
	    WinQueryDlgItemShort( hDlg, ID_BDMONTH, &month, FALSE );
	    WinQueryDlgItemShort( hDlg, ID_BDDAY, &iDay, FALSE );
	    day = (double)iDay;
	    /* Check that date is within acceptable range */
	    if (year<1900 || year>2100) {
	       WinMessageBox( HWND_DESKTOP, hDlg,
			      "Dates valid from 1900-2100",
			      "Birthday!", NULL,
			      MB_OK | MB_ICONEXCLAMATION );
	       break;
	    }
	    /* Get julian date of birth date */
	    Born = julian( year, month, day );

	    /* Write birth date to OS2.INI if check box checked */
	    if (WinSendDlgItemMsg(hDlg, ID_OS2INI, BM_QUERYCHECK, 0L, 0L)) {
		   sprintf(szBuf, "%d", year);
	      PrfWriteProfileString( HINI_USERPROFILE, szAppName, "Year", szBuf );
		   sprintf(szBuf, "%d", month);
	      PrfWriteProfileString( HINI_USERPROFILE, szAppName, "Month", szBuf );
		   sprintf(szBuf, "%d", (int)day);
	      PrfWriteProfileString( HINI_USERPROFILE, szAppName, "Day", szBuf );
	    }

	    /* Get selected day of interest edit field values */
	    WinQueryDlgItemShort( hDlg, ID_YEAR, &year, FALSE );
	    WinQueryDlgItemShort( hDlg, ID_MONTH, &month, FALSE );
	    WinQueryDlgItemShort( hDlg, ID_DAY, &iDay, FALSE );
	    day = (double)iDay;
	    /* Check that date is within acceptable range */
	    if (year<1900 || year>2100) {
	       WinMessageBox( HWND_DESKTOP, hDlg,
			      "Dates valid from 1900-2100",
			      "Display Date!", NULL,
			      MB_OK | MB_ICONEXCLAMATION );
	       break;
	    }

	    /* Compute number of days since birth */
		 SelectDay  = (long)(julian( year, month, day ) - Born);
	    /* Top date of display is 3 days before selected day */
	    Day = SelectDay - 3;
	    /* Got a valid birthdate, enable all routines */
	    bBorn = TRUE;
	    WinDismissDlg( hDlg, TRUE );
	    break;

	case DID_CANCEL:
	    /* Exit and ignore entries */
	    WinDismissDlg( hDlg, FALSE );
	    break;

	default:
	    break;
    }
}
/*
*/
