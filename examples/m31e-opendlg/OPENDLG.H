/*
    OPENDLG.H -- File Dialog include file
    Created by Microsoft Corporation, 1989
*/

#define IDS_MERGE1      0      /* merge string (%%) */
#define IDS_IFN         1      /* %% is not a valid filename. */
#define IDS_FNF         2      /* %% not found - Create new file? */
#define IDS_REF         3      /* Replace existing %%? */
#define IDS_SCC         4      /* %% has changed. Save current changes? */
#define IDS_EOF         5      /* Error opening %% */
#define IDS_ECF         6      /* Error creating %% */

/* Dialog box IDs Open/Save IDs */
#define IDD_FILEOPEN    -1
#define IDD_FILESAVE    -2

#define ATTRDIRLIST     0x4010 /* include directories and drives in listbox */

#define MAX_FNAME_LEN     CCHMAXPATH
#define CBEXTMAX          6
#define CBROOTNAMEMAX    (MAX_FNAME_LEN - CBEXTMAX - 1)
#define MAXMESSAGELENGTH 128 /* maximum cb     of a msg     (instruction
                                or warning). */

/* Action value for DlgFile, the values may be ORed, except DLG_OPENDLG */
/* and DLG_SAVEDLG. */
#define DLG_OPENDLG 0x00        /* Use the Open dialog box. */
#define DLG_SAVEDLG 0x01        /* Use the Save (As) dialog box */
#define DLG_NOOPEN  0x02        /* Don't Open the file we selected */
#define DLG_HELP    0x04        /* The dialog box supports Help */

/* Return values from DlgFile : */
#define TDF_ERRMEM   0
#define TDF_INVALID  1
#define TDF_NOOPEN   2
#define TDF_NEWOPEN  3
#define TDF_OLDOPEN  4
#define TDF_NOSAVE   5
#define TDF_NEWSAVE  6
#define TDF_OLDSAVE  7

/* Flags for OpenFile */
#define OF_REOPEN 0x8000
#define OF_EXIST  0x4000
#define OF_PROMPT 0x2000
#define OF_CREATE 0x1000
#define OF_CANCEL 0x0800
#define OF_VERIFY 0x0400
#define OF_DELETE 0x0200
#define OF_PARSE  0x0100

#define OF_READ      0x0080
#define OF_WRITE     0x0040
#define OF_READWRITE 0x0020


#ifndef RC_INVOKED

/* Useful abbreviations */
typedef char *PSTR;
typedef SHANDLE FAR *PHANDLE;

/* Data structure used to pass information to DlgFile(). */
typedef struct _DLF {   /* dlf */
        USHORT   rgbAction;             /* action usType:  eg. DLG_OPEN   */
        USHORT   rgbFlags;              /* open file flAttributes         */
        PHANDLE  phFile;                /* file handle                    */
        PSZ      pszExt;                /* default file extension "\\.ext"*/
        PSZ      pszAppName;            /* application name               */
        PSZ      pszTitle;              /* panel title or NULL            */
        PSZ      pszInstructions;       /* panel instructions or NULL     */
        CHAR szFileName[MAX_FNAME_LEN]; /* relative file name             */
        CHAR szOpenFile[MAX_FNAME_LEN]; /* full path name of current file */
	CHAR szLastWild[MAX_FNAME_LEN]; /* last relative wild card name   */
	CHAR szLastFile[MAX_FNAME_LEN]; /* last relative file name	  */
} DLF;
typedef DLF *NPDLF;
typedef DLF FAR *PDLF;

/*  Exported functions */
VOID	EXPENTRY AddExt(PSZ, PSZ);
PSZ	EXPENTRY FileInPath(PSZ);
ULONG	EXPENTRY GetTextExtent (HPS, PCH, int);
BOOL	EXPENTRY MergeStrings(PSZ, PSZ, PSZ);
BOOL	EXPENTRY OpenFile(PSZ, PHANDLE, PSZ, USHORT);
void	EXPENTRY SetupDLF( PDLF, int, PHFILE, PSZ, PSZ, PSZ, PSZ);
int	EXPENTRY DlgFile(HWND, PDLF);
int	EXPENTRY AlertBox(HWND, int, PSZ, PSZ, USHORT, USHORT);

#endif
