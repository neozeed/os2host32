/*
    FILE.C -- Open/Save As Dialog Box library routines
   Created by Microsoft, IBM  Corporation 1990
*/
#define  INCL_DOS
#define  INCL_DOSQUEUES
#include "tool.h"
#include <string.h>


/****************************************************************************\
* This function is the Open dialog box window procedure.  It handles input,
* allows the user to change directories, checks for legal filenames, opens
* specified files, appends default extensions and returns the file's name.
*
* The return values are
*   TDF_INVALID - Library error (internal error),
*   TDF_NOOPEN  - User hits cancel
*   TDF_NEWOPEN - Created new file (file left open)
*   TDF_OLDOPEN - Opened existing file (file left open)
\****************************************************************************/

MRESULT EXPENTRY DlgOpenWndProc(hwnd, msg, mp1, mp2)
HWND hwnd;
USHORT msg;
MPARAM mp1;
MPARAM mp2;
    {
    PDLF pdlf;
    PSZ  lpchFile;
    CHAR sz[MAX_FNAME_LEN];

    switch (msg)
        {
    case WM_INITDLG:
        /* initialize dialog box */
        DlgInitOpen(hwnd, LONGFROMMP(mp2));

        /* fill static field with path name and fill list box with
           filenames that lMatch spec */
        if (!DlgDirList(hwnd, ((PDLF)(mp2))->pszExt+1, ID_DIRLIST,
                        ID_FILELIST, ID_PATH, ((PDLF)(mp2))->rgbFlags))
            /* NOTE: shouldn't we post a message if something screws up? */
            WinDismissDlg(hwnd, TDF_INVALID);
        break;

    case WM_COMMAND:
        pdlf = (PDLF) WinQueryWindowULong(hwnd, 0);
        switch (SHORT1FROMMP(mp1))
            {
        case MBID_OK:
            /* Open button pressed */
            /* get name from edit box */
            WinQueryWindowText(WinWindowFromID(hwnd, ID_EDIT),
                CBROOTNAMEMAX, (PSZ)pdlf->szFileName);
	    Upper((PSZ)pdlf->szFileName);
            if (strlen((PSZ)pdlf->szFileName))
                DlgOpenName(hwnd, pdlf);
            break;

        case MBID_CANCEL:
            /* Cancel button pressed, dismiss dialog box */
            WinDismissDlg(hwnd, TDF_NOOPEN);
	    break;

	case MBID_HELP:
	    /* Help button pressed */
	    WinMessageBox( HWND_DESKTOP
			 , hwnd
			 , pdlf->pszInstructions
			 , pdlf->pszTitle
			 , NULL
			 , MB_OK | MB_APPLMODAL );
	    break;
            }
        break;

    case WM_CONTROL:
        pdlf = (PDLF) WinQueryWindowULong(hwnd, 0);
        switch (SHORT1FROMMP(mp1))
            {
        case ID_DIRLIST:
            /* user clicked in directory list box */
            switch (SHORT2FROMMP(mp1))
                {
            case LN_SELECT:
                /* single click case */
                /* get current edit string */
                WinQueryWindowText(WinWindowFromID(hwnd, ID_EDIT),
                                   CBROOTNAMEMAX, (PSZ)sz);
                Upper((PSZ)sz);

                /* get selected string */
                if (DlgDirSelect(hwnd, (PSZ)pdlf->szFileName, ID_DIRLIST))
                    {
                    /* if edit field contains wild card, then append file
		       part to selected directory, otherwise append
		       last wildcard search spec */
                    lpchFile = FileInPath((PSZ)sz);
		    strcat( (PSZ)pdlf->szFileName
			   , DlgSearchSpec( lpchFile)
			     ? lpchFile
			     : (PSZ)pdlf->szLastWild );
                    /* set edit box to resulting name */
                    WinSetWindowText(WinWindowFromID(hwnd, ID_EDIT),
                                     (PSZ)pdlf->szFileName);
                    }
                break;

            case LN_ENTER:
		/* get text from edit box */
		WinQueryWindowText(WinWindowFromID(hwnd, ID_EDIT),
				   CBROOTNAMEMAX, (PSZ)pdlf->szFileName);
		Upper((PSZ)pdlf->szFileName);
		if( DlgSearchSpec( (PSZ)pdlf->szFileName))
		    {
		    DlgDirList( hwnd
			      , (PSZ)pdlf->szFileName
			      , ID_DIRLIST
			      , ID_FILELIST
			      , ID_PATH
			      , pdlf->rgbFlags );
		    strcpy( (PSZ)pdlf->szLastWild
			   , FileInPath( (PSZ)pdlf->szFileName));
		    WinSetWindowText( WinWindowFromID( hwnd, ID_EDIT)
				    , (PSZ)pdlf->szFileName );
		    }
                break;
                }
            break;

        case ID_FILELIST:
            /* user clicked in file list box */
            switch (SHORT2FROMMP(mp1))
                {
            case LN_SELECT:
                /* single click case */

		/* get current edit string */
		/* if it contains a wildcard, save it before obliteration */
                WinQueryWindowText(WinWindowFromID(hwnd, ID_EDIT),
                                   CBROOTNAMEMAX, (PSZ)sz);
		Upper((PSZ)sz);
		if( DlgSearchSpec( (PSZ)sz))
		    strcpy( (PSZ)pdlf->szLastWild, FileInPath( (PSZ)sz));

                /* get selected file name */
                DlgDirSelect(hwnd, (PSZ)pdlf->szFileName, ID_FILELIST);
                /* set edit box to resulting name */
                WinSetWindowText(WinWindowFromID(hwnd, ID_EDIT),
                                 (PSZ)pdlf->szFileName);
                break;

            case LN_ENTER:
                /* double click case, single click already processed */
                DlgOpenName(hwnd, pdlf);
                break;
                }
            break;
            }
        break;

    default:
        return (WinDefDlgProc(hwnd, msg, mp1, mp2));
        }

    return (MRFROMLONG(0L));  /* message processed */
    }




/****************************************************************************\
* This function stores the pdlf in the dialog window structure, sets
* the title and instruction texts and limits the text size.
\****************************************************************************/

VOID PASCAL DlgInitOpen (hwnd, lpParm)
HWND hwnd;
ULONG lpParm;
    {
    PDLF pdlf;

    /* Set pdlf local to window */
    pdlf = (PDLF) lpParm;
    WinSetWindowULong(hwnd, 0, lpParm);

    /* set edit box text size limit */
    WinSendDlgItemMsg(hwnd, ID_EDIT, EM_SETTEXTLIMIT, (MPARAM)CBROOTNAMEMAX, 0L);

    /* set edit window to search spec */
    if (pdlf->pszExt != (PSZ)NULL)
        WinSetWindowText(WinWindowFromID(hwnd, ID_EDIT), pdlf->pszExt+1);

    /* set title window */
    if (pdlf->pszTitle != (PSZ)NULL)
	WinSetWindowText(WinWindowFromID(hwnd, FID_TITLEBAR), pdlf->pszTitle);
    }




/****************************************************************************\
* This function processes the currently selected name in the open dialog
* box.  If the name represents a directory, the current directory is
* changed to that one and the corresponding files are displayed.  If the
* name is a file, then it is opened.
\****************************************************************************/

VOID PASCAL DlgOpenName(hwnd, pdlf)
HWND hwnd;
PDLF pdlf;
    {
    PSZ  lpch;
    USHORT wVal;
    CHAR sz[MAX_FNAME_LEN];

    /* try using current name as a directory */
    strcpy((PSZ)sz, (PSZ)pdlf->szFileName);
    if (!DlgSearchSpec((PSZ)sz))
        DlgAddSearchExt(pdlf, (PSZ)sz);
    if (DlgDirList(hwnd, (PSZ)sz, ID_DIRLIST, ID_FILELIST, ID_PATH,
                   pdlf->rgbFlags))
        {
        /* name was a directory, extract and set file name */
        lpch = FileInPath((PSZ)sz);
        strcpy((PSZ)pdlf->szFileName, lpch);
        WinSetWindowText(WinWindowFromID(hwnd, ID_EDIT),
                         (PSZ)pdlf->szFileName);
        }
    else
        /* try to open name as a file */
        if ((wVal = DlgOpenFile(pdlf, hwnd)) != TDF_NOOPEN)
            WinDismissDlg(hwnd, wVal);
    }




/****************************************************************************\
* This function is the SaveAs dialog box window procedure.  It handles input,
* tests for legal filenames and uses message boxes to report any problems.
*
* Return values are:
*     TDF_INVALID - Library error (internal error),
*     TDF_NOOPEN  - User hits cancel
*   Specific for DLG_NOOPEN
*     TDF_NEWSAVE - user wants to save to a new file (file not created)
*     TDF_OLDSAVE - user wants to save over existing file (file not opened)
*   else
*     TDF_NEWSAVE - user wants to save to a new file (file left open)
*     TDF_OLDSAVE - user wants to save over existing file (file left open)
\****************************************************************************/

MRESULT EXPENTRY DlgSaveAsWndProc(hwnd, msg, mp1, mp2)
HWND   hwnd;
USHORT msg;
MPARAM mp1;
MPARAM mp2;
    {
    PDLF    pdlf;
    PSZ     lpchFile;
    CHAR    sz[MAX_FNAME_LEN];

    switch (msg)
        {
    case WM_INITDLG:
        /* Store pdlf, set instructions, limit text size */
        DlgInitSaveAs(hwnd, LONGFROMMP(mp2));

        /* fill static field with path name and fill list box with
           filenames that lMatch spec */
        if (!DlgDirList(hwnd, ((PDLF)(mp2))->pszExt+1, ID_DIRLIST,
                        ID_FILELIST, ID_PATH, ((PDLF)(mp2))->rgbFlags))
            /* NOTE: shouldn't we post a message if something screws up? */
            WinDismissDlg(hwnd, TDF_INVALID);
        break;

    case WM_COMMAND:
        pdlf = (PDLF) WinQueryWindowULong(hwnd, 0);
        switch (SHORT1FROMMP(mp1))
           {
        case MBID_OK:
            /* get text from edit box */
            WinQueryWindowText(WinWindowFromID(hwnd, ID_EDIT),
                               CBROOTNAMEMAX, (PSZ)pdlf->szFileName);
	    Upper((PSZ)pdlf->szFileName);
	    if( DlgSearchSpec( (PSZ)pdlf->szFileName))
		{
		DlgDirList( hwnd
			  , (PSZ)pdlf->szFileName
			  , ID_DIRLIST
			  , ID_FILELIST
			  , ID_PATH
			  , pdlf->rgbFlags );
		strcpy( (PSZ)pdlf->szLastWild
		       , FileInPath( (PSZ)pdlf->szFileName));
		strcpy( (PSZ)pdlf->szFileName, (PSZ)pdlf->szLastFile);
		WinSetWindowText( WinWindowFromID( hwnd, ID_EDIT)
				, (PSZ)pdlf->szFileName );
		}
	    else if( strlen( (PSZ)pdlf->szFileName))
		DlgSaveAsName( hwnd, pdlf);
            break;

        case MBID_CANCEL:
            WinDismissDlg(hwnd, TDF_NOSAVE);
            break;

	case MBID_HELP:
	    /* Help button pressed */
	    WinMessageBox( HWND_DESKTOP
			 , hwnd
			 , pdlf->pszInstructions
			 , pdlf->pszTitle
			 , NULL
			 , MB_OK | MB_APPLMODAL );
	    break;
            }
        break;

    case WM_CONTROL:
        pdlf = (PDLF) WinQueryWindowULong(hwnd, 0);
        switch (SHORT1FROMMP(mp1))
            {
        case ID_DIRLIST:
            /* user clicked in directory list box */
            switch (SHORT2FROMMP(mp1))
                {
            case LN_SELECT:
                /* single click case */
                /* get current edit string */
                WinQueryWindowText(WinWindowFromID(hwnd, ID_EDIT),
                                   CBROOTNAMEMAX, (PSZ)sz);
                Upper((PSZ)sz);

                /* get selected string */
                if (DlgDirSelect(hwnd, (PSZ)pdlf->szFileName, ID_DIRLIST))
                    {
                    /* if edit field contains wild card, then append file
		       part to selected directory, otherwise append
		       last wildcard search spec */
                    lpchFile = FileInPath((PSZ)sz);
		    if( DlgSearchSpec( lpchFile))
			{
			strcat( (PSZ)pdlf->szFileName, lpchFile );
			strcpy( (PSZ)pdlf->szLastWild, lpchFile);
			}
		    else
			{
			strcat( (PSZ)pdlf->szFileName, (PSZ)pdlf->szLastWild );
			strcpy( (PSZ)pdlf->szLastFile, lpchFile);
			}
                    /* set edit box to resulting name */
                    WinSetWindowText(WinWindowFromID(hwnd, ID_EDIT),
                                     (PSZ)pdlf->szFileName);
                    }
                break;

            case LN_ENTER:
		/* get text from edit box */
		WinQueryWindowText(WinWindowFromID(hwnd, ID_EDIT),
				   CBROOTNAMEMAX, (PSZ)pdlf->szFileName);
		Upper((PSZ)pdlf->szFileName);
		if( DlgSearchSpec( (PSZ)pdlf->szFileName))
		    {
		    DlgDirList( hwnd
			      , (PSZ)pdlf->szFileName
			      , ID_DIRLIST
			      , ID_FILELIST
			      , ID_PATH
			      , pdlf->rgbFlags );
		    strcpy( (PSZ)pdlf->szLastWild
			   , FileInPath( (PSZ)pdlf->szFileName));
		    strcpy( (PSZ)pdlf->szFileName, (PSZ)pdlf->szLastFile);
		    WinSetWindowText( WinWindowFromID( hwnd, ID_EDIT)
				    , (PSZ)pdlf->szFileName );
		    }
		break;
		}
            break;

        case ID_FILELIST:
            /* user clicked in file list box */
            switch (SHORT2FROMMP(mp1))
                {
            case LN_SELECT:
                /* single click case */

		/* get current edit string */
		/* if it contains a wildcard, save it before obliteration */
                WinQueryWindowText(WinWindowFromID(hwnd, ID_EDIT),
                                   CBROOTNAMEMAX, (PSZ)sz);
		Upper((PSZ)sz);
		if( DlgSearchSpec( (PSZ)sz))
		    strcpy( (PSZ)pdlf->szLastWild, FileInPath( (PSZ)sz));

                /* get selected file name */
                DlgDirSelect(hwnd, (PSZ)pdlf->szFileName, ID_FILELIST);
                /* set edit box to resulting name */
                WinSetWindowText(WinWindowFromID(hwnd, ID_EDIT),
                                 (PSZ)pdlf->szFileName);
                break;

            case LN_ENTER:
                /* double click case, single click already processed */
		DlgSaveAsName(hwnd, pdlf);
                break;
                }
            break;
            }
        break;

    default:
        return (WinDefDlgProc(hwnd, msg, mp1, mp2));
        }

    return (MRFROMLONG(0L));    /* message processed */
    }



/******************************************************************************/
/*									      */
/* This function attempts to open a file for writing.  It queries if over-    */
/* write is OK if the file already exists.				      */
/*									      */
/******************************************************************************/
VOID PASCAL DlgSaveAsName( hwnd, pdlf)
HWND  hwnd;
PDLF  pdlf;
    {
    USHORT  usTdf;

    /* add extension if there is not one already */
    AddExt((PSZ)pdlf->szFileName, pdlf->pszExt);
    /* test file name legality */
    if (!DlgParseFile((PSZ)pdlf->szFileName,
		      (PSZ)pdlf->szOpenFile, FALSE, FALSE))
	{
	/* illegal filename */
	DlgAlertBox(hwnd, IDS_IFN, pdlf,
		    MB_OK | MB_ICONEXCLAMATION);
	return;
	}
    usTdf = TDF_NEWSAVE;
    /* test if file already exists */
    if (DlgParseFile((PSZ)pdlf->szFileName,
		     (PSZ)pdlf->szOpenFile, TRUE, FALSE))
	{
	/* overwrite? */
	if (DlgAlertBox(hwnd, IDS_REF, pdlf,
			MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION)
	    == MBID_NO)
	    return;
	usTdf = TDF_OLDSAVE;
	}
    if (!(pdlf->rgbAction & DLG_NOOPEN) &&
	!(OpenFile( (PSZ)pdlf->szFileName
		  , pdlf->phFile
		  , (PSZ)pdlf->szOpenFile
		  , OF_WRITE)))
	{
	DlgAlertBox(hwnd, IDS_EOF, pdlf,
		    MB_OK | MB_ICONEXCLAMATION);
	return;
	}
    WinDismissDlg(hwnd, usTdf);
    }



/****************************************************************************\
* This function initializes the SaveAs dialog box.  It puts the current
* directory string in the ID_PATH field and initializes the edit box
* with the proposed filename.  It is the simple relative file name if current
* dir == pdlf->szOpenFile.  Otherwise, it is the fully qualified name.
\****************************************************************************/

VOID PASCAL DlgInitSaveAs (hwnd, lpParm)
HWND hwnd;
ULONG lpParm;
    {
    PDLF pdlf;
    CHAR sz[MAX_FNAME_LEN];
    PSZ  lpszFile, lpszFN, lpszCD;

    /* set pdlf local to window */
    pdlf = (PDLF) lpParm;
    WinSetWindowULong(hwnd, 0, lpParm);

    /* set edit box text size limit */
    WinSendDlgItemMsg(hwnd, ID_EDIT, EM_SETTEXTLIMIT, (MPARAM)CBROOTNAMEMAX, 0L);

    /* set title window */
    if (pdlf->pszTitle != (PSZ)NULL)
	WinSetWindowText(WinWindowFromID(hwnd, FID_TITLEBAR), pdlf->pszTitle);

    /* set szLastWild to search spec */
    if (pdlf->pszExt != (PSZ)NULL)
	strcpy( (PSZ)pdlf->szLastWild, (PSZ)pdlf->pszExt+1 );

    /* get current directory */
    DosSearchPath(0, (PSZ)szDot, (PSZ)szStar, (PSZ)sz, MAX_FNAME_LEN);
    lpszFile = FileInPath((PSZ)sz);
    if (lpszFile > (PSZ)sz + 3)
        lpszFile--;
    *lpszFile = '\0';

    /* compare path part name to previously opened file name and
       make the file name relative if they are the same */
    lpszFN = (PSZ)pdlf->szOpenFile;
    lpszCD = (PSZ)sz;
    lpszFile = FileInPath((PSZ)pdlf->szOpenFile);
    while (lpszFN < lpszFile && *lpszFN == *lpszCD)
        {
        lpszFN = NextChar(lpszFN);
        lpszCD = NextChar(lpszCD);
        }
    if (*lpszCD)
        lpszFN = (PSZ)pdlf->szOpenFile;
    else if (*lpszFN == '\\')
	lpszFN = NextChar(lpszFN);
    Upper( lpszFN);
    WinSetWindowText(WinWindowFromID(hwnd, ID_EDIT), lpszFN);
    strcpy( (PSZ)pdlf->szLastFile, lpszFN);

    /* set current path field */
    WinSetWindowText(WinWindowFromID(hwnd, ID_PATH),
                     DlgFitPathToBox(hwnd, ID_PATH, (PSZ)sz));
    }




/****************************************************************************\
* This function returns the filename part of the given path string.
\****************************************************************************/

PSZ  EXPENTRY FileInPath(lpsz)
PSZ  lpsz;
    {
    PSZ  lpch;

    /* strip path/drive specification from name if there is one */
    lpch = lpsz + strlen(lpsz);
    while (lpch > lpsz)
        {
        lpch = PrevChar(lpsz, lpch);
        if (*lpch == '\\' || *lpch == ':')
            {
            lpch = NextChar(lpch);
            break;
            }
        }
    return(lpch);
    }




/****************************************************************************\
* This function adds the extension to a file name if it is missing.
\****************************************************************************/

VOID EXPENTRY AddExt(lpsz, lpszExt)
PSZ  lpsz;
PSZ  lpszExt;
    {
    PSZ  lpch;

    lpch = lpsz + strlen(lpsz);
    while (*lpch != '.' && *lpch != '\\' && *lpch != ':' && lpch > lpsz)
        lpch = PrevChar(lpsz, lpch);

    if (*lpch != '.')
        strcat(lpsz, (PSZ)(lpszExt+2));
    }




/****************************************************************************\
* This function adds the "appropriate" extension to a filename, partial
* filename, search spec or partial search spec.
\****************************************************************************/

VOID PASCAL DlgAddSearchExt(pdlf, lpszEdit)
PDLF pdlf;
PSZ  lpszEdit;
    {
    PSZ  lpchLast;

    lpchLast = PrevChar(lpszEdit, lpszEdit + strlen(lpszEdit));
    if (*lpchLast == '\\' || *lpchLast == ':')
        strcat(lpszEdit, pdlf->pszExt + 1);
    else
        strcat(lpszEdit, pdlf->pszExt);
    }




/****************************************************************************\
* This function returns TRUE if lpsz contains a wildcard character '*' or '?';
\****************************************************************************/

BOOL PASCAL DlgSearchSpec(lpsz)
PSZ  lpsz;
   {
    for (; *lpsz; lpsz = NextChar(lpsz))
        if (*lpsz == '*' || *lpsz == '?')
            return TRUE;
    return FALSE;
   }




/****************************************************************************\
* This function displays an error or warning msg.
\****************************************************************************/

int PASCAL DlgAlertBox(hwnd, ids, pdlf, wStyle)
HWND hwnd;
int ids;
PDLF pdlf;
USHORT wStyle;
    {
    /* note:  5th param is idHelp */
    return (AlertBox(hwnd, ids, (PSZ)pdlf->szFileName, pdlf->pszAppName,
                         NULL, wStyle | MB_APPLMODAL));
    }
