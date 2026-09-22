/*
    FILE1.C -- Dialog Directory Listbox and Open File functions
   Created by Microsoft, IBM  Corporation 1990
*/

#define  INCL_DOS
#include "tool.h"
#include <string.h>

/****************************************************************************\
* This function builds a directory list in a list box.
\****************************************************************************/

int FAR PASCAL DlgDirList(hwnd, lpszPath, idDirList, idFileList,
                          idStaticPath, attr)
HWND hwnd;
PSZ  lpszPath;
int idDirList;
int idFileList;
int idStaticPath;
USHORT attr;
    {
    CHAR szPath[MAX_FNAME_LEN];
    CHAR chTmp;
    PSZ  lpszFile, lpszNull;
    HPOINTER hPointer;

    /* ensure path is legal and expand to full search path */
    if (!DlgParseFile(lpszPath, (PSZ)szPath, FALSE, TRUE))
        return FALSE;

    /* set pointer to hours glass */
    hPointer = WinQueryPointer(HWND_DESKTOP); 
    WinSetPointer(HWND_DESKTOP, WinQuerySysPointer(HWND_DESKTOP, SPTR_WAIT, FALSE));

    /* set current drive */
    DosSelectDisk(*szPath - 'A' + 1);

    /* temporarily put zero after directory spec and set current directory */
    lpszFile = FileInPath((PSZ)szPath);
    lpszNull = lpszFile;
    if (lpszNull > (PSZ)szPath + 3)
        lpszNull--;
    chTmp = *lpszNull;
    *lpszNull = '\0';
    DosChDir((PSZ)szPath);
    if (idStaticPath)
        WinSetWindowText(WinWindowFromID(hwnd, idStaticPath),
                         DlgFitPathToBox(hwnd, idStaticPath, (PSZ)szPath));
    *lpszNull = chTmp;

    /* fill list box with file names */
    if (idDirList && idFileList)
        {
        /* fill them up with new entries */
        DlgFillListBoxes(hwnd, idDirList, idFileList, attr, lpszFile);
        }

    /* reset pointer to previous figure */
    WinSetPointer(HWND_DESKTOP, hPointer);

    return TRUE;
    }




/****************************************************************************\
* This function gets the file name selected by the user.
\****************************************************************************/

int FAR PASCAL DlgDirSelect(hwnd, lpszIn, idListBox)
HWND hwnd;
PSZ  lpszIn;
int idListBox;
    {
    CHAR sz[MAX_FNAME_LEN];
    int item;
    PSZ  lpsz, lpszFile;
    BOOL fDir;

    /* get currently selected list entry */
    item = (int) WinSendDlgItemMsg(hwnd, idListBox, LM_QUERYSELECTION, 0L, 0L);
    if (item == LIT_NONE)
        return FALSE;
    WinSendDlgItemMsg(hwnd, idListBox, LM_QUERYITEMTEXT,
                      MPFROM2SHORT(item, MAX_FNAME_LEN), (MPARAM)(PSZ)sz);
    lpszFile = sz;

    /* extract name */
    if (fDir = (*sz == '['))
        {
        lpszFile = NextChar(lpszFile);
        if (*lpszFile == '-')
            {
            /* drive selection */
            lpszFile = NextChar(lpszFile);
            *(lpszFile+1) = ':';
            *(lpszFile+2) = '\0';
            }
        else
            {
            /* directory selection */
            lpsz = lpszFile;
            while (*lpsz != ']')
                lpsz = NextChar(lpsz);
            *lpsz = '\\';
            }
        }

    strcpy(lpszIn, lpszFile);
    return (fDir);
    }




/****************************************************************************\
* This function tries to open pdlf=>szFileName.  If the file does not
* exist, the function asks to create it.
*
*  Returns:
*    TDF_NOOPEN  - Illegal Filename or user didn't want to create
*    TDF_OLDOPEN - Existing file
*    TDF_NEWOPEN - File was created
\****************************************************************************/

USHORT PASCAL DlgOpenFile(pdlf, hwnd)
PDLF pdlf;
HWND hwnd;
    {
    /* check for legal dos name */
    if (!DlgParseFile((PSZ)pdlf->szFileName, (PSZ)pdlf->szOpenFile,
                      FALSE, FALSE))
        {
        DlgAlertBox(hwnd, IDS_IFN, pdlf, MB_OK | MB_ICONEXCLAMATION);
        return (TDF_NOOPEN);
        }
    /* see if file already exists */
    if (DlgParseFile((PSZ)pdlf->szFileName, (PSZ)pdlf->szOpenFile,
                      TRUE, FALSE))
        {
        if (OpenFile((PSZ)pdlf->szFileName, pdlf->phFile,
                        (PSZ)pdlf->szOpenFile, OF_READ))
            return (TDF_OLDOPEN);
        else
            {
            DlgAlertBox(hwnd, IDS_EOF, pdlf, MB_OK | MB_ICONEXCLAMATION);
            return (TDF_NOOPEN);
            }
        }
    /* file doesn't exist: create new one? */
    else if (DlgAlertBox(hwnd, IDS_FNF, pdlf, MB_YESNO | MB_ICONQUESTION) == MBID_YES)
        {
        if (OpenFile((PSZ)pdlf->szFileName, pdlf->phFile,
                        (PSZ)pdlf->szOpenFile, OF_CREATE))
            return (TDF_NEWOPEN);
        else
            DlgAlertBox(hwnd, IDS_ECF, pdlf, MB_OK | MB_ICONEXCLAMATION);
        }
    return(TDF_NOOPEN);
    }




/****************************************************************************\
* This function returns the OS/2 file handle if operation is successful,
* 0 otherwise.
*
* Effects:  Depend on wMode:
*     OF_READ:      open file for reading only
*     OF_WRITE:     open file for writing only
*     OF_READWRITE: open file for reading and writing
*     OF_CREATE:    create the file if it does not exist
*     OF_REOPEN:    open file using info in the reopen buffer
*     OF_EXIST:     test file existence
*     OF_PARSE:     fill reopen buffer, with no other action
\****************************************************************************/


BOOL EXPENTRY OpenFile(lpszFile, lpHandle, lpszOpenFile, wMode)
PSZ  lpszFile;
PHFILE lpHandle;
PSZ  lpszOpenFile;
USHORT wMode;
    {
    HFILE hFile = NULL;
    ULONG wAction = NULL;
    ULONG wAttrs = NULL;
    ULONG wOpenFlag, wOpenMode;
    CHAR sz[MAX_FNAME_LEN];

    /* if reopen specified, use earlier pathname */
    if (wMode & OF_REOPEN)
        {
        strcpy((PSZ)sz, lpszOpenFile);
        lpszFile = (PSZ)sz;
        }

    /* parse file */
    if (!DlgParseFile(lpszFile, lpszOpenFile, wMode&OF_EXIST, FALSE))
        {
        *lpszOpenFile = '\0';
        return FALSE;
        }
    /* return if implementing boolean test OF_PARSE or OF_EXIST */
    if (wMode & (OF_PARSE | OF_EXIST))
        {
        return TRUE;
        }

    if (wMode & OF_READ)
        {
        wOpenFlag = OPEN_ACTION_OPEN_IF_EXISTS;
        wOpenMode = OPEN_SHARE_DENYWRITE;
        }
    else if (wMode & OF_WRITE)
        {
        wOpenFlag = OPEN_ACTION_OPEN_IF_EXISTS|OPEN_ACTION_CREATE_IF_NEW;
        wOpenMode = OPEN_ACCESS_WRITEONLY|OPEN_SHARE_DENYREADWRITE;
        }
    else if (wMode & OF_READWRITE)
        {
        wOpenFlag = OPEN_ACTION_OPEN_IF_EXISTS|OPEN_ACTION_CREATE_IF_NEW;
        wOpenMode = OPEN_ACCESS_READWRITE|OPEN_SHARE_DENYREADWRITE;
        }
    else if (wMode & OF_CREATE)
        {
        wOpenFlag = OPEN_ACTION_CREATE_IF_NEW;
        wOpenMode = OPEN_SHARE_DENYREADWRITE;
        }
    else
        {
        return FALSE;
        }

    if (DosOpen(lpszFile, (PHFILE)&hFile, &wAction,
                (ULONG) 0, 0, wOpenFlag, wOpenMode, (ULONG) 0))
        return FALSE;

    if (wMode & OF_CREATE)
        {
        if (DosClose(hFile))
            return FALSE;
        }
    else
        *lpHandle = hFile;

    return TRUE;
    }




/****************************************************************************\
* This function puts a path string into a static box.
\****************************************************************************/

PSZ  PASCAL DlgFitPathToBox(hwnd, idStatic, lpch)
HWND hwnd;
int idStatic;
PSZ  lpch;
    {
    WRECT rc;
    int cxField;
    char chDrive;
    HPS hps;

    /* get length of static field */
    WinQueryWindowRect(WinWindowFromID(hwnd, idStatic), (PRECTL)&rc);
    cxField = rc.xRight - rc.xLeft;

    hps = WinGetPS(hwnd);
    if (cxField < LOUSHORT(GetTextExtent(hps, lpch,
                                           strlen(lpch))))
        {
        chDrive = *lpch;
        /* chop characters off front of string until text is short enough */
        do
            do
                lpch = NextChar(lpch);
            while (*(lpch+6) != '\\' &&
                   *(lpch+6) != '\0');
        while (cxField < LOUSHORT(GetTextExtent(hps, lpch,
                                                 strlen(lpch))));
        /* insert header */
        *lpch++ = chDrive;
        *lpch++ = ':';
        *lpch++ = '\\';
        *lpch++ = '.';
        *lpch++ = '.';
        *lpch++ = '.';
        lpch -= 6;
        }
    WinReleasePS(hps);
    return (lpch);
    }




/****************************************************************************\
* This function fills a list box with appropriate file names from the
* current directory.
\****************************************************************************/

int PASCAL DlgFillListBoxes(hwnd, idDirList, idFileList, attr, lpszFileSpec)
HWND   hwnd;
int    idDirList;
int    idFileList;
USHORT attr;
PSZ    lpszFileSpec;
    {
    ULONG ulFiles, ulDrive;
    int i;
    PSZ  lpsz;
    HDIR hDir;
    ULONG lrgfDrives;
    FILEFINDBUF ffb;
    CHAR sz[20];
    int cDrives;
    int result = -1;
    HWND hwndFiles, hwndDirs;

    /* get listbox window handles */
    hwndFiles = WinWindowFromID(hwnd, idFileList);
    hwndDirs = WinWindowFromID(hwnd, idDirList);

    /* disable updates to listboxes */
    WinEnableWindowUpdate(hwndFiles, FALSE);
    WinEnableWindowUpdate(hwndDirs, FALSE);

    /* empty list boxes of any old entries */
    WinSendMsg(hwndFiles, LM_DELETEALL, 0L, 0L);
    WinSendMsg(hwndDirs, LM_DELETEALL, 0L, 0L);

    /* put files that lMatch search spec in file listbox */
    ulFiles = 1;
    hDir = 0xFFFF;
    if (!DosFindFirst(lpszFileSpec, (PHDIR)&hDir,
        attr&~(FILE_DIRECTORY|0x4000), (PFILEFINDBUF)&ffb, sizeof(FILEFINDBUF),
        &ulFiles, FIL_STANDARD))
        {
        do
            {
            /* add string to list box */
            result = (int) WinSendMsg(hwndFiles, LM_INSERTITEM,
                                      MPFROM2SHORT(LIT_SORTASCENDING, 0),
                                      (MPARAM)(PSZ)&(ffb.achName[0]));
            ulFiles = 1;
            }
        while (result >= 0 && !DosFindNext(hDir,
                                           (PFILEFINDBUF)&ffb,
                                           sizeof(FILEFINDBUF),
                                           &ulFiles));
        DosFindClose(hDir);
        }

    if (result != LIT_MEMERROR && (attr & FILE_DIRECTORY))
        {
        /* get directories */
        ulFiles = 1;
        hDir = 0xFFFF;
	if (!DosFindFirst((PSZ)szStar, (PHDIR)&hDir, FILE_DIRECTORY,
            (PFILEFINDBUF)&ffb, sizeof(FILEFINDBUF), &ulFiles, FIL_STANDARD))
            {
            do
                {
                /* extract file name */
                if (ffb.attrFile & FILE_DIRECTORY)
                    {
                    /* put brackets around directory */
		    lpsz = (PSZ)&(ffb.achName[0]);
                    if (*lpsz == '.' && *(lpsz+1) == '\0')
                        /* forget about current directory name '.' */
			continue;
                    sz[0] = '[';
                    strcpy((PSZ)&sz[1], lpsz);
                    sz[ffb.cchName    + 1] = ']';
                    sz[ffb.cchName    + 2] = '\0';
                    lpsz = (PSZ)sz;
                    /* add string to list box */
                    result = (int) WinSendMsg(hwndDirs, LM_INSERTITEM,
                                              MPFROM2SHORT(LIT_SORTASCENDING, 0),
                                              (MPARAM)lpsz);
                    }
                ulFiles = 1;
                }
            while (result >= -1 && !DosFindNext(hDir,
                                               (PFILEFINDBUF)&ffb,
                                               sizeof(FILEFINDBUF),
                                               &ulFiles));
            DosFindClose(hDir);
            }
        }

    if (result != LIT_MEMERROR && (attr & 0x4000))
        /* get drives */
        {
        sz[0] = '[';
        sz[1] = sz[3] = '-';
        sz[4] = ']';
        sz[5] = '\0';

        DosQCurDisk(&ulDrive, (PULONG)&lrgfDrives);
        cDrives = 0;
        for (i=0; 'A' + i <= 'Z'; i++)
            {
            if (lrgfDrives & 1L)
                {
                sz[2] = (char)('A' + i);
                /* add drive to list */
                result = (int) WinSendMsg(hwndDirs, LM_INSERTITEM,
					  MPFROM2SHORT(LIT_END, 0),
                                          (MPARAM)(PSZ)sz);
                cDrives++;
                }
            lrgfDrives >>= 1;
            }
        }

    /* enable and show updated listboxes */
    WinShowWindow(hwndFiles, TRUE);
    WinShowWindow(hwndDirs, TRUE);

    return result;
    }




/****************************************************************************\
* This function parses a string into an fully expanded file name or search
* path.  If fExist is true then the file must exist or the search path must
* correspond to at least one existing file.  In all cases, the corresponding
* directory must exist.  The string must be a file name, ie. no wildcards,
* unless fWildOkay is true.  Returns TRUE if string is parsed correctly.
\****************************************************************************/

BOOL PASCAL DlgParseFile(lpszIn, lpszExp, fExist, fWildOkay)
PSZ  lpszIn, lpszExp;
BOOL fExist, fWildOkay;
    {
    CHAR szPath[MAX_FNAME_LEN];
    PSZ  lpszFile, lpsz;

    /* go past any path spec to first char in file name */
    lpszFile = FileInPath(lpszIn);

    /* check validity of file name */
    if (!fExist && !DlgValidName(lpszFile, fWildOkay))
        {
        *lpszExp = '\0';
        return FALSE;
        }

    /* copy path part to path string */
    lpsz = (PSZ)szPath;
    if (lpszIn == lpszFile)
        *lpsz++ = '.';
    else
        {
        while (lpszIn < lpszFile && lpsz < ((PSZ)szPath + MAX_FNAME_LEN - 1))
            *lpsz++ = *lpszIn++;
        }
    *lpsz = '\0';

    /* let DOS do the dirty work */
    if (fExist)
        {
        /* test existence of file while parsing path */
        if (DosSearchPath(0, (PSZ)szPath, lpszFile, lpszExp, MAX_FNAME_LEN))
            {
            *lpszExp = '\0';
            return FALSE;
            }
        }
    else
        {
        /* use dummy wild card while parsing path */
	if (DosSearchPath(0, (PSZ)szPath, (PSZ)szStar, lpszExp, MAX_FNAME_LEN))
            {
            *lpszExp = '\0';
            return FALSE;
            }
        /* replace wild card part with true file name converted to CAPS */
        lpsz = lpszExp;
        while (*lpsz != '?' && *lpsz != '*' && *lpsz != '\0')
            lpsz++;
        while (*lpszFile != '\0')
            {
            *lpsz++ = *lpszFile++;
            }
        *lpsz = '\0';
        Upper(lpszExp);
        }
    return TRUE;
    }




/****************************************************************************\
* This function determines if a string forms a legal file name or search
* path.  Wildcard characters are acceptable if fWildOkay is true.  Returns
* TRUE if string is legal.
\****************************************************************************/

BOOL PASCAL DlgValidName(lpszName, fWildOkay)
PSZ  lpszName;
BOOL fWildOkay;
    {
    int cLen;
    PSZ  lpsz;

    /* check file name */
    if (*lpszName == '\0')
        return FALSE;
    cLen = 0;
    lpsz = lpszName;
    while (*lpsz != '\0' && *lpsz != '.')
        {
        if (++cLen > 8 || *lpsz < 0x20)
            return FALSE;
        switch (*lpsz++)
            {
            case '*':
            case '?':
                if (fWildOkay)
                    break;
            case '\"':
            case '\\':
            case '/':
            case '[':
            case ']':
            case ':':
            case '|':
            case '<':
            case '>':
            case '+':
            case '=':
            case ';':
            case ',':
                return FALSE;
            }
        }

    /* check extension */
    if (*lpsz++ == '\0')
        return TRUE;
    cLen = 0;
    while (*lpsz != '\0')
        {
        if (++cLen > 3 || *lpsz < 0x20)
            return FALSE;
        switch (*lpsz++)
            {
            case '*':
            case '?':
                if (fWildOkay)
                    break;
            case '.':
            case '\"':
            case '\\':
            case '/':
            case '[':
            case ']':
            case ':':
            case '|':
            case '<':
            case '>':
            case '+':
            case '=':
            case ';':
            case ',':
                return FALSE;
            }
        }
    return TRUE;
    }
