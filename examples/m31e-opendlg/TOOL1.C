/*
    TOOL1.C -- More commonly used library routines
    Created by Microsoft and/or IBM Corporation, 1990
*/

#include "tool.h"

/***************************************************************************\
* Merges sz[idMes1] and szText1 and displays a message box using
* the wStyle. Use caption title provided by szText2.
* Returns the answer from the message box (MBID_OK, MBID_CANCEL).
\***************************************************************************/

int EXPENTRY AlertBox (hwnd, idMes, lpszText1, lpszText2, idHelp, wStyle)
HWND  hwnd;
int   idMes;
PSZ  lpszText1;
PSZ  lpszText2;
USHORT idHelp;
USHORT wStyle;
    {
    char szMessage [MAXMESSAGELENGTH];

    MergeStrings((PSZ)vrgsz[idMes], lpszText1, (PSZ)szMessage);
    if (idHelp != NULL)
        wStyle |= MB_HELP;
    return (WinMessageBox (HWND_DESKTOP, hwnd, (PSZ)szMessage, lpszText2,
                           idHelp, wStyle));
    }


/***************************************************************************\
* Scan szSrc for merge spec. If found, insert string szMerge at that point.
* Then append rest of szSrc NOTE! Merge spec guaranteed to be two chars.
* returns TRUE if it does a merge, false otherwise.
\***************************************************************************/

BOOL EXPENTRY MergeStrings(lpszSrc, lpszMerge, lpszDst)
PSZ   lpszSrc;
PSZ   lpszMerge;
PSZ   lpszDst;
    {
    /* Find merge spec if there is one. */
    while (*(unsigned *)lpszSrc != *(unsigned *)vrgsz[IDS_MERGE1])
        {
        *lpszDst++ = *lpszSrc;
        /* If we reach end of string before merge spec, just return. */
        if (!*lpszSrc++)
            return FALSE;
        }

    /* If merge spec found, insert sz2 there. (check for null merge string */
    if (lpszMerge)
        {
        while (*lpszMerge)
            *lpszDst++ = *lpszMerge++;
        }

    /* Jump over merge spec */
    lpszSrc++,lpszSrc++;

    /* Now append rest of Src String */
    while (*lpszDst++ = *lpszSrc++);
    return TRUE;
    }



/****************************************************************************\
* This function invokes either an Open or Save dialog box.
*     (lpdlf->rgbAction == DLG_OPENDLG)  -->  invoke Open dlgBox
*     (lpdlf->rgbAction == DLG_SAVEDLG)  -->  invoke Save dlbBox
*
* Unless DLG_NOSAVE is specified, the file is opened and left open.
*
* Return values are:
*     TDF_INVALID - Library error (internal error),
*     TDF_ERRMEM  - Out of memory error
*     TDF_NOOPEN  - User hits cancel
*   specific to DLG_OPEN:
*     TDF_NEWOPEN - Created new file
*     TDF_OLDOPEN - Opened existing file
*   specific to DLG_SAVE:
*     TDF_NEWSAVE - user wants to save to a new file
*     TDF_OLDSAVE - user wants to save over existing file
*   specific to DLG_NOSAVE:
*     TDF_NEWSAVE - user wants to save to a new file
*     TDF_OLDSAVE - user wants to save over existing file
\****************************************************************************/

int EXPENTRY DlgFile(hwnd, pdlf)
HWND  hwnd;
PDLF pdlf;
    {
    /* create dialog box */
    if (pdlf->rgbAction & DLG_SAVEDLG)
        return WinDlgBox(HWND_DESKTOP, hwnd, DlgSaveAsWndProc, vhModule,
                         IDD_SAVEAS, (PSZ)pdlf);
    else
        return WinDlgBox(HWND_DESKTOP, hwnd, DlgOpenWndProc, vhModule,
                         IDD_OPEN, (PSZ)pdlf);
    }

/**************************************************************************\
* This function initializes the DLF structure
\**************************************************************************/
void EXPENTRY SetupDLF(PDLF pdlf, int iAction, PHFILE phFile,
		       PSZ Extension, PSZ AppName, PSZ pszInTitle,
		       PSZ pszInInstructions ) {
    pdlf->pszExt = Extension;
    pdlf->rgbFlags = ATTRDIRLIST;
    pdlf->phFile = phFile;
    pdlf->pszAppName = AppName;
    pdlf->rgbAction = iAction;
    pdlf->pszTitle = pszInTitle;
    pdlf->pszInstructions = pszInInstructions;
}
