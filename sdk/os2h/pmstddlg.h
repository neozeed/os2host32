/**********************************************************************\
*								       *
* Module Name: PMSTDDLG.H					       *
*								       *
* OS/2 Presentation Manager Standard Dialog Declarations	       *
*								       *
* Copyright (c) 1987  Microsoft Corporation			       *
* Copyright (c) 1987  IBM Corporation				       *
*								       *
* ==================================================================== *
* The following symbols are used in this file for conditional	       *
* sections:							       *
*								       *
* INCL_WINSTDDLGS		 - include all dialogs/controls	       *
* INCL_WINSTDFILE		 - standard file dialog		       *
* INCL_WINSTDFONT		 - standard font dialog		       *
* INCL_WINSTDSPIN		 - spin button control class	       *
* INCL_WINSTDDRAG		 - standard drag dll		       *
*								       *
\**********************************************************************/

#if !(defined(INCL_32) || defined(INCL_16))
#ifdef M_I386
    #define INCL_32
#else /* not M_I386 */
    #define INCL_16
#endif /* M_I386 */
#endif /* INCL_32 || INCL_16 */

#ifdef INCL_WINSTDDLGS /* enable everything */
    #define INCL_WINSTDFILE
    #define INCL_WINSTDFONT
    #define INCL_WINSTDSPIN
    #define INCL_WINSTDDRAG
#endif /* INCL_WINSTDDLGS */


#ifdef INCL_WINSTDFILE
/**********************************************************************/
/*								      */
/*		       F I L E	  D I A L O G			      */
/*								      */
/**********************************************************************/

/*--------------------------------------------------------------------*/
/* File Dialog Selector Constants:				      */
/*    Prime variable FILEDLG.usDialogType with desired dialog type.   */
/*--------------------------------------------------------------------*/
#define	 OPEN_DIALOG	0	 /* Select OPEN	  dialog.	      */
#define	 SAVEAS_DIALOG	1	 /* Select SaveAs dialog.	      */

/*--------------------------------------------------------------------*/
/* File Dialog Invocation Flag Definitions.			      */
/*--------------------------------------------------------------------*/
#define	 FDS_CENTER	   0x00000001L /* Center within owner dialog. */
#define	 FDS_CUSTOM	   0x00000002L /* Use custom user template    */
#define	 FDS_HELPBUTTON	   0x00000008L /* Display Help button	      */

/*--------------------------------------------------------------------*/
/* Error Return Codes from dialog (self defining)		      */
/*--------------------------------------------------------------------*/
#define	 FDS_SUCCESSFUL				   0
#define	 FDS_ERR_DEALLOCATE_MEMORY		   1
#define	 FDS_ERR_FILTER_TRUNC			   2
#define	 FDS_ERR_INVALID_DIALOG			   3
#define	 FDS_ERR_INVALID_DRIVE			   4
#define	 FDS_ERR_INVALID_FILTER			   5
#define	 FDS_ERR_INVALID_PATHFILE		   6
#define	 FDS_ERR_OUT_OF_MEMORY			   7
#define	 FDS_ERR_PATH_TOO_LONG			   8
#define	 FDS_ERR_TOO_MANY_FILE_TYPES		   9
#define	 FDS_ERR_INVALID_VERSION		   10
#define	 FDS_ERR_INVALID_CUSTOM_HANDLE		   11
#define	 FDS_ERR_DIALOG_LOAD_ERROR		   12
#define	 FDS_ERR_DRIVE_ERROR			   13

/*--------------------------------------------------------------------*/
/* File Dialog Messages.					      */
/*--------------------------------------------------------------------*/
#define	FDM_FILTER	 (WM_USER+40)  /* mp1 = PSZ pszFileName	      */
				     /* mp2 = PSZ EA .type value      */
				     /* mr  = TRUE -> keep file.      */
#define	FDM_VALIDATE	 (WM_USER+41)  /* mp1 = PSZ pszPathName	      */
				     /* mp2 = Application return value*/
				     /* mr  = TRUE -> Exit dialog     */

/*--------------------------------------------------------------------*/
/* Define the window word offset for the "lUser" data value.	      */
/*--------------------------------------------------------------------*/
#define	 WWO_APP	    QWL_USER

/*--------------------------------------------------------------------*/
/* Define the type that is a pointer to an array of pointers.	      */
/*     Hence: pointer to an array of Z string pointers.		      */
/*--------------------------------------------------------------------*/
typedef	 PSZ	  (FAR * PAPSZ)[1];

/*--------------------------------------------------------------------*/
/* File Dialog application data structure.			      */
/*--------------------------------------------------------------------*/
typedef struct _FILEDLG {  /* fild */
    ULONG    cbsize;		/* Size of FILEDLG structure.	      */
    ULONG    fl;		/* FDS_ flags. Alter behavior of dlg. */
    ULONG    lUser;		/* User defined field.		      */
    ULONG    lReturn;		/* Result code from dialog dismissal. */
    LONG     lSRC;		/* System return code.		      */
    PSZ	     pszTitle;		/* String to display in title bar.    */
    PSZ	     pszOKButton;	/* String to display in OK button.    */
    PFNWP    pfnDlgProc;	/* Entry point to custom dialog proc. */
    PSZ	     pszIType;		/* Pointer to string containing	      */
				/*   initial EA type filter. Type     */
				/*   does not have to exist in list.  */
    PAPSZ    ppszITypeList;	/* Pointer to table of pointers that  */
				/*    point to null terminated Type   */
				/*    strings. End of table is marked */
				/*    by a NULL pointer.	      */
    PSZ	     pszIDrive;		/* Pointer to string containing	      */
				/*   initial drive. Drive does not    */
				/*   have to exist in drive list.     */
    PAPSZ    ppszIDriveList;	/* Pointer to table of pointers that  */
				/*    point to null terminated Drive  */
				/*    strings. End of table is marked */
				/*    by a NULL pointer.	      */
    HMODULE  hmod;		/* Custom File Dialog template.	      */
    #ifdef INCL_16
      USHORT _align;
    #endif
    char  szFullFile [CCHMAXPATH];   /* Initial or selected fully     */
				     /* qualified path and file.      */
    USHORT   idDlg;		/* Custom dialog id.		      */
    USHORT   usDialogType;	/* Dialog selector. File or SaveAs.   */
    SHORT    x;			/* Dialog position coordinates.	      */
    SHORT    y;
    SHORT    sEAType;		/* Selected file's EA Type.	  */
    BOOL     bEnableFileLB;	/* Flag for saveas file listbox enable*/
} FILEDLG, FAR *PFILEDLG;

/*--------------------------------------------------------------------*/
/* File Dialog - Function Prototype				      */
/*--------------------------------------------------------------------*/
#ifdef INCL_16
#define	KitFileDlgProc		Kit16FileDlgProc
#define	KitFileDlg(hwnd,pfild)	Kit16FileDlg(hwnd,pfild,TRUE)
HWND	APIENTRY Kit16FileDlg (HWND hwnd, PFILEDLG pfild, BOOL fIs16);
#else
HWND	APIENTRY KitFileDlg (HWND hwnd, PFILEDLG pfild);
#endif /* INCL_16 */

MRESULT APIENTRY KitFileDlgProc( HWND hwnd, USHORT msg, MPARAM mp1,
				 MPARAM mp2);

/*--------------------------------------------------------------------*/
/* File Dialog - dialog and control ids				      */
/*--------------------------------------------------------------------*/
#define	  DID_FILE_DIALOG	      256
#define	  DID_FILENAME_TXT	      257
#define	  DID_FILENAME_ED	      258
#define	  DID_DRIVE_TXT		      259
#define	  DID_DRIVE_CB		      260
#define	  DID_FILTER_TXT	      261
#define	  DID_FILTER_CB		      262
#define	  DID_DIRECTORY_TXT	      263
#define	  DID_DIRECTORY_LB	      264
#define	  DID_DIRECTORY2_LB	      265
#define	  DID_FILES_TXT		      266
#define	  DID_FILES_LB		      267
#define	  DID_OK_PB		      DID_OK
#define	  DID_CANCEL_PB		      DID_CANCEL
#define	  DID_HELP_PB		      270

#define	  IDS_FILE_ALL_FILES_SELECTOR	   1000
#define	  IDS_FILE_BACK_CUR_PATH	   1001
#define	  IDS_FILE_BACK_PREV_PATH	   1002
#define	  IDS_FILE_BACK_SLASH		   1003
#define	  IDS_FILE_BASE_FILTER		   1004
#define	  IDS_FILE_BLANK		   1005
#define	  IDS_FILE_COLON		   1006
#define	  IDS_FILE_DOT			   1007
#define	  IDS_FILE_DRIVE_LETTERS	   1008
#define	  IDS_FILE_FWD_CUR_PATH		   1009
#define	  IDS_FILE_FWD_PREV_PATH	   1010
#define	  IDS_FILE_FORWARD_SLASH	   1011
#define	  IDS_FILE_PARENT_DIR		   1012
#define	  IDS_FILE_Q_MARK		   1013
#define	  IDS_FILE_SPLAT		   1014
#define	  IDS_FILE_SPLAT_DOT		   1015
#define	  IDS_FILE_SAVEAS_TITLE		   1016
#define	  IDS_FILE_SAVEAS_FILTER_TXT	   1017
#define	  IDS_FILE_SAVEAS_FILENM_TXT	   1018
#define	  IDS_FILE_DUMMY_FILE_NAME	   1019
#define	  IDS_FILE_DUMMY_FILE_EXT	   1020
#define	  IDS_FILE_DUMMY_DRIVE		   1021
#define	  IDS_FILE_DUMMY_ROOT_DIR	   1022
#define	  IDS_FILE_PATH_PTR		   1023
#define	  IDS_FILE_VOLUME_PREFIX	   1024
#define	  IDS_FILE_VOLUME_SUFFIX	   1025

#define	  IDS_FILE_BAD_DRIVE_NAME	   1100
#define	  IDS_FILE_BAD_DRIVE_OR_PATH_NAME  1101
#define	  IDS_FILE_BAD_FILE_NAME	   1102
#define	  IDS_FILE_BAD_FQF		   1103
#define	  IDS_FILE_BAD_NETWORK_NAME	   1104
#define	  IDS_FILE_BAD_SUB_DIR_NAME	   1105
#define	  IDS_FILE_DRIVE_NOT_AVAILABLE	   1106
#define	  IDS_FILE_FQFNAME_TOO_LONG	   1107
#define	  IDS_FILE_OPEN_DIALOG_NOTE	   1108
#define	  IDS_FILE_PATH_TOO_LONG	   1109
#define	  IDS_FILE_SAVEAS_DIALOG_NOTE	   1110

#define	  IDS_FILE_DRIVE_DISK_CHANGE	   1120
#define	  IDS_FILE_DRIVE_NOT_READY1	   1121
#define	  IDS_FILE_DRIVE_NOT_READY2	   1122
#define	  IDS_FILE_DRIVE_LOCKED		   1123
#define	  IDS_FILE_DRIVE_NO_SECTOR	   1124
#define	  IDS_FILE_DRIVE_SOME_ERROR	   1125
#define	  IDS_FILE_DRIVE_INVALID	   1126
#define	  IDS_FILE_INSERT_DISK_NOTE	   1127
#define	  IDS_FILE_OK_WHEN_READY	   1128

#endif /* INCL_WINSTDFILE */


#ifdef INCL_WINSTDFONT
/**********************************************************************/
/*								      */
/*		       F O N T	  D I A L O G			      */
/*								      */
/**********************************************************************/

/**********************************************************************/
/* Font Dialog Creation Structure				      */
/**********************************************************************/
typedef struct _FONTDLG {  /* fntd */
    ULONG   cbSize;		/* sizeof(FONTDLG)		   */
    HPS	    hpsScreen;		/* Screen presentation space	   */
    HPS	    hpsPrinter;		/* Printer presentation space	   */
    PSZ	    pszTitle;		/* Application supplied title	   */
    PSZ	    pszPreview;		/* String to print in preview wndw */
    PSZ	    pszPtSizeList;	/* Application provided size list  */
    PFNWP   pfnDlgProc;		/* Dialog subclass procedure	   */
    CHAR    szFamilyname[FACESIZE];	/* Familyname of font	   */
    FIXED   fxPointSize;	/* Point size the user selected	   */
    ULONG   fl;			/* FNTS_* flags			   */
    ULONG   flFlags;		/* FNTF_* state flags		   */
    ULONG   flType;		/* Font type option bits	   */
    ULONG   flTypeMask;		/* Mask of which font types to use */
    ULONG   flStyle;		/* The selected style bits	   */
    ULONG   flStyleMask;	/* Mask of which style bits to use */
    ULONG   flCHSOptions;	/* CHS_* options the user selected */
    ULONG   flCHSMask;		/* Mask of CHS_* options to use	   */
    LONG    clrFore;		/* Selected foreground color	   */
    LONG    clrBack;		/* Selected background color	   */
    ULONG   lUser;		/* Blank field for application	   */
    LONG    lReturn;		/* Return Value of the Dialog	   */
    LONG    lEmHeight;		/* Em height of the current font   */
    LONG    lXHeight;		/* X height of the current font	   */
    LONG    lExternalLeading;	/* External Leading of font	   */
    HMODULE hMod;		/* Module to load custom template  */
    #ifdef INCL_16
      USHORT _align;
    #endif
    SHORT   sNominalPointSize;	/* Nominal Point Size of font	   */
    USHORT  usWeight;		/* The boldness of the font	   */
    USHORT  usWidth;		/* The width of the font	   */
    SHORT   x;			/* X coordinate of the dialog	   */
    SHORT   y;			/* Y coordinate of the dialog	   */
    USHORT  idDlg;		/* ID of a custom dialog template  */
    FATTRS  fAttrs;		/* Font attribute structure	   */
} FONTDLG, FAR *PFONTDLG;

/**********************************************************************/
/* Font Dialog Style Flags					      */
/**********************************************************************/
#define	FNTS_CENTER		    1L
#define	FNTS_CUSTOM		    2L
#define	FNTS_MULTIFONTSELECTION	    4L
#define	FNTS_HELPBUTTON		    8L
#define	FNTS_APPLYBUTTON	   16L
#define	FNTS_RESETBUTTON	   32L
#define	FNTS_MODELESS		   64L

/**********************************************************************/
/* Font Dialog Flags						      */
/**********************************************************************/
#define	FNTF_VIEWPRINTERFONTS	    1L
#define	FNTF_PRINTERFONTSELECTED    2L

/**********************************************************************/
/* Color code definitions					      */
/**********************************************************************/
#define	CLRC_FOREGROUND		    1L
#define	CLRC_BACKGROUND		    2L

/**********************************************************************/
/* Font Dialog Messages						      */
/**********************************************************************/
#define	FNTM_FACENAMECHANGED   (WM_USER+50)  /* mp1 = PSZ pszFacename   */
#define	FNTM_POINTSIZECHANGED  (WM_USER+51)  /* mp1 = PSZ pszPointSize, */
					   /* mp2 = FIXED fxPointSize */
#define	FNTM_STYLECHANGED      (WM_USER+52)  /* mp1 = PSTYLECHANGE pstyc*/
#define	FNTM_COLORCHANGED      (WM_USER+53)  /* mp1 = LONG clr	      */
					   /* mp2 = USHORT codeClr    */
#define	FNTM_UPDATEPREVIEW     (WM_USER+54)  /* mp1 = HWND hWndPreview  */

/**********************************************************************/
/* Stylechange message parameter structure			      */
/**********************************************************************/
typedef struct _STYLECHANGE {	/* stylechg */
    USHORT	usWeight;
    USHORT	usWeightOld;
    USHORT	usWidth;
    USHORT	usWidthOld;
    ULONG	flType;
    ULONG	flTypeOld;
    ULONG	flTypeMask;
    ULONG	flTypeMaskOld;
    ULONG	flStyle;
    ULONG	flStyleOld;
    ULONG	flStyleMask;
    ULONG	flStyleMaskOld;
    ULONG	flCHSOptions;
    ULONG	flCHSOptionsOld;
    ULONG	flCHSMask;
    ULONG	flCHSMaskOld;
} STYLECHANGE, FAR *PSTYLECHANGE;

/**********************************************************************/
/* Font Dialog Shared Segment Names				      */
/**********************************************************************/
#define	FONTDLG_SHARED_SEGMENT1 "\\SHAREMEM\\EPKFDS01.SEG"
#define	FONTDLG_SHARED_SEGMENT2 "\\SHAREMEM\\EPKFDS02.SEG"

/**********************************************************************/
/* Font Dialog Semaphore Name					      */
/**********************************************************************/
#define	FONTDLG_SEMAPHORE	"\\SEM\\EPKFDS01.SEM"

/**********************************************************************/
/* Font Dialog Function Prototypes				      */
/**********************************************************************/
#ifdef INCL_16
#define	KitDefFontDlgProc	   Kit16DefFontDlgProc
#define	KitFontDialog(hwnd,pfntd)  Kit16FontDialog(hwnd,pfntd,TRUE)
HWND	APIENTRY Kit16FontDialog (HWND hwnd, PFONTDLG pfntd, BOOL fIs16);
#else
HWND	APIENTRY KitFontDialog (HWND hwnd, PFONTDLG pfntd);
#endif /* INCL_16 */

MRESULT APIENTRY KitDefFontDlgProc (HWND hwnd, USHORT msg, MPARAM mp1,
				    MPARAM mp2 );

/**********************************************************************/
/* font dialog and control id's				   */
/**********************************************************************/
#define	DID_FONT_DIALOG		 300
#define	DID_NAME_PREFIX		 301
#define	DID_NAME		 302
#define	DID_NAME2		 303
#define	DID_STYLE_PREFIX	 304
#define	DID_STYLE		 305
#define	DID_STYLE2		 306
#define	DID_DISPLAY_FILTER	 307
#define	DID_PRINTER_FILTER	 308
#define	DID_SIZE_PREFIX		 309
#define	DID_SIZE		 310
#define	DID_SAMPLE_GROUPBOX	 311
#define	DID_SAMPLE		 312
#define	DID_HELP_BUTTON		 313
#define	DID_EMPHASIS_GROUPBOX	 314
#define	DID_OUTLINE		 315
#define	DID_UNDERSCORE		 316
#define	DID_STRIKEOUT		 317
#define	DID_APPLY_BUTTON	 318
#define	DID_RESET_BUTTON	 319

/**********************************************************************/
/* Stringtable id's					      */
/**********************************************************************/
#define	IDS_FONT_SAMPLE		   350
#define	IDS_FONT_KEY_0		   351
#define	IDS_FONT_KEY_9		   352
#define	IDS_FONT_KEY_SEP	   353
#define	IDS_FONT_DISP_ONLY	   354
#define	IDS_FONT_PRINTER_ONLY	   355
#define	IDS_FONT_COMBINED	   356
#define	IDS_FONT_WEIGHT1	   357
#define	IDS_FONT_WEIGHT2	   358
#define	IDS_FONT_WEIGHT3	   360
#define	IDS_FONT_WEIGHT4	   361
#define	IDS_FONT_WEIGHT5	   362
#define	IDS_FONT_WEIGHT6	   363
#define	IDS_FONT_WEIGHT7	   364
#define	IDS_FONT_WEIGHT8	   365
#define	IDS_FONT_WEIGHT9	   366
#define	IDS_FONT_WIDTH1		   367
#define	IDS_FONT_WIDTH2		   368
#define	IDS_FONT_WIDTH3		   369
#define	IDS_FONT_WIDTH4		   370
#define	IDS_FONT_WIDTH5		   371
#define	IDS_FONT_WIDTH6		   372
#define	IDS_FONT_WIDTH7		   373
#define	IDS_FONT_WIDTH8		   374
#define	IDS_FONT_WIDTH9		   375
#define	IDS_FONT_OPTION1	   376
#define	IDS_FONT_OPTION2	   377
#define	IDS_FONT_OPTION3	   378
#define	IDS_FONT_POINT_SIZE_LIST   379

#endif /* INCL_WINSTDFONT */

#ifdef INCL_WINSTDSPIN
/**********************************************************************/
/*								      */
/*			    S P I N    B U T T O N		      */
/*								      */
/**********************************************************************/

/**********************************************************************/
/* SPINBUTTON Creation Flags					      */
/**********************************************************************/

/**********************************************************************/
/* Character Acceptance						      */
/**********************************************************************/
#define	SPBS_ALLCHARACTERS 0x00000000L /* Default: All chars accepted */
#define	SPBS_NUMERICONLY   0x00000001L /* Only 0 - 9 accepted & VKeys */
#define	SPBS_READONLY	   0x00000002L /* No chars allowed in entryfld*/

/**********************************************************************/
/* Type of Component						      */
/**********************************************************************/
#define	SPBS_MASTER	   0x00000010L
#define	SPBS_SERVANT	   0x00000000L /* Default: Servant	      */

/**********************************************************************/
/* Type of Justification					      */
/**********************************************************************/
#define	SPBS_JUSTDEFAULT  0x00000000L /* Default: Same as Left	      */
#define	SPBS_JUSTLEFT	  0x00000008L
#define	SPBS_JUSTRIGHT	  0x00000004L
#define	SPBS_JUSTCENTER	  0x0000000CL

/**********************************************************************/
/* Border or not						      */
/**********************************************************************/
#define	SPBS_NOBORDER	  0x00000020L /* Borderless SpinField	      */
				      /* Default is to have a border. */

/**********************************************************************/
/* Fast spin or not						      */
/**********************************************************************/
#define	SPBS_FASTSPIN	  0x00000100L /* Allow fast spinning.  Fast   */
				      /* spinning is performed by     */
				      /* skipping over numbers	      */

/**********************************************************************/
/* Pad numbers on front with 0's				  */
/**********************************************************************/
#define	SPBS_PADWITHZEROS 0x00000080L /* Pad the number with zeroes   */

/**********************************************************************/
/* SPINBUTTON Messages						      */
/**********************************************************************/

/**********************************************************************/
/* Notification from Spinbutton to the application is sent in a	      */
/* WM_CONTROL message.						      */
/**********************************************************************/
#define	SPBN_UPARROW	   0x20A      /* up arrow button was pressed  */
#define	SPBN_DOWNARROW	   0x20B      /* down arrow button was pressed*/
#define	SPBN_ENDSPIN	   0x20C      /* mouse button was released    */
#define	SPBN_CHANGE	   0x20D      /* spinfield text has changed   */
#define	SPBN_SETFOCUS	   0x20E      /* spinfield received focus     */
#define	SPBN_KILLFOCUS	   0x20F      /* spinfield lost focus	      */

/**********************************************************************/
/* Messages from application to Spinbutton			      */
/**********************************************************************/
#define	SPBM_OVERRIDESETLIMITS 0x200  /* Set spinbutton limits without*/
				      /*  resetting the current value */
#define	SPBM_QUERYLIMITS       0x201  /* Query limits set by	      */
				      /*  SPBM_SETLIMITS	      */
#define	SPBM_SETTEXTLIMIT      0x202  /* Max entryfield characters    */
#define	SPBM_SPINUP	       0x203  /* Tell entry field to spin up  */
#define	SPBM_SPINDOWN	       0x204  /* Tell entry field to spin down*/
#define	SPBM_QUERYVALUE	       0x205  /* Tell entry field to send     */
				      /*  current value		      */

/**********************************************************************/
/* Query Flags							      */
/**********************************************************************/
#define	SPBQ_UPDATEIFVALID    0	      /* Default		      */
#define	SPBQ_ALWAYSUPDATE     1
#define	SPBQ_DONOTUPDATE      3

/**********************************************************************/
/* Return value for Empty Field.				      */
/*    If ptr too long, variable sent in query msg		      */
/**********************************************************************/
#define	SPBM_SETARRAY	       0x206  /* Change the data to spin      */
#define	SPBM_SETLIMITS	       0x207  /* Change the numeric Limits    */
#define	SPBM_SETCURRENTVALUE   0x208  /* Change the current value     */
#define	SPBM_SETMASTER	       0x209  /* Tell entryfield who master is*/

/**********************************************************************/
/* SPINBUTTON Window Class Definition				      */
/**********************************************************************/
#define	WC_SPINBUTTON	((PSZ)0xffff0020L)

/**********************************************************************/
/* REGISTERSPINBUTTON Function Prototype			      */
/**********************************************************************/
VOID	APIENTRY	RegisterSpinButton ( VOID );

#endif /* INCL_WINSTDSPIN */


#ifdef INCL_WINSTDDRAG
/**********************************************************************/
/*								      */
/*		  D I R E C T	M A N I P U L A T I O N		      */
/*								      */
/**********************************************************************/

#define	PMERR_NOT_DRAGGING 0x1f00	 /* move to pmerr.h	      */

#define	MSGF_DRAG	   0x0010	 /* message filter identifier */

#define	WM_DRAGFIRST	   0x0300
#define	WM_DRAGLAST	   (WM_DRAGFIRST + 0x000F)

#define	DM_DROP		   (WM_DRAGLAST - 0)
#define	DM_DRAGOVER	   (WM_DRAGLAST - 1)
#define	DM_DRAGLEAVE	   (WM_DRAGLAST - 2)
#define	DM_DROPHELP	   (WM_DRAGLAST - 3)
#define	DM_ENDCONVERSATION (WM_DRAGLAST - 4)
#define	DM_PRINT	   (WM_DRAGLAST - 5)
#define	DM_RENDER	   (WM_DRAGLAST - 6)
#define	DM_RENDERCOMPLETE  (WM_DRAGLAST - 7)
#define	DM_RENDERPREPARE   (WM_DRAGLAST - 8)

#define	DRT_ASM		   "Assembler Code"   /* drag type constants  */
#define	DRT_BASIC	   "BASIC Code"
#define	DRT_BINDATA	   "Binary Data"
#define	DRT_BITMAP	   "Bitmap"
#define	DRT_C		   "C Code"
#define	DRT_COBOL	   "COBOL Code"
#define	DRT_DLL		   "Dynamic Link Library"
#define	DRT_DOSCMD	   "DOS Command File"
#define	DRT_EXE		   "Executable"
#define	DRT_FORTRAN	   "FORTRAN Code"
#define	DRT_ICON	   "Icon"
#define	DRT_LIB		   "Library"
#define	DRT_METAFILE	   "Metafile"
#define	DRT_OS2CMD	   "OS/2 Command File"
#define	DRT_PASCAL	   "Pascal Code"
#define	DRT_RESOURCE	   "Resource File"
#define	DRT_TEXT	   "Plain Text"
#define	DRT_UNKNOWN	   "Unknown"

#define	DOR_NODROP	   0x0000	/* DM_DRAGOVER response codes */
#define	DOR_DROP	   0x0001
#define	DOR_NODROPOP	   0x0002
#define	DOR_NEVERDROP	   0x0003

#define	DO_COPYABLE	   0x0001	/* supported operation flags  */
#define	DO_MOVEABLE	   0x0002

#define	DC_OPEN		   0x0001	/* source control flags	      */
#define	DC_REF		   0x0002
#define	DC_GROUP	   0x0004
#define	DC_CONTAINER	   0x0008
#define	DC_PREPARE	   0x0010
#define	DC_REMOVEABLEMEDIA 0x0020

#define	DO_DEFAULT	   0xBFFE	/* Default operation	      */
#define	DO_UNKNOWN	   0xBFFF	/* Unknown operation	      */
#define	DO_COPY		   KC_CTRL
#define	DO_MOVE		   KC_ALT

#define	DMFL_TARGETSUCCESSFUL 0x0001	/* transfer reply flags	      */
#define	DMFL_NATIVERENDER     0x0002
#define	DMFL_RENDERRETRY      0x0004
#define	DMFL_RENDEROK         0x0008
#define	DMFL_RENDERFAIL       0x0010
#define	DMFL_TARGETFAIL       0x0020

#define	DRG_IMAGEICON	      0x00000001L  /* drag image manipulation */
#define	DRG_IMAGETRANSPARENT  0x00000002L  /*  flags		      */
#define	DRG_NOIMAGEAUGMENT    0x00000004L
#define	DRG_IMAGEBITMAP	      0x00000008L


typedef LHANDLE HSTR;  /* hstr */

typedef struct _DRAGITEM {  /* ditem */
  HWND	  hwndItem;		     /* conversation partner	      */
  ULONG	  ulItemID;		     /* identifies item being dragged */
  HSTR	  hstrType;		     /* type of item		      */
  HSTR	  hstrRendMechFmt;	     /* rendering mechanism and format*/
  HSTR	  hstrContainerName;	     /* name of source container      */
  HSTR	  hstrSourceName;	     /* name of item at source	      */
  HSTR	  hstrTargetName;	     /* suggested name of item at dest*/
  USHORT  fsControl;		     /* source item control flags     */
  USHORT  fsSupportedOps;	     /* ops supported by source	      */
} DRAGITEM;
typedef DRAGITEM FAR *PDRAGITEM;

typedef struct _DRAGINFO {  /* dinfo */
  ULONG	   cbDraginfo;		     /* Size of DRAGINFO and DRAGITEMs*/
  USHORT   cbDragitem;		     /* size of DRAGITEM	      */
  USHORT   usOperation;		     /* current drag operation	      */
  HWND	   hwndSource;		     /* window handle of source	      */
  SHORT	   xDrop;		     /* x coordinate of drop position */
  SHORT	   yDrop;		     /* y coordinate of drop position */
  SHORT	   cxHotspot;		     /* x offset of mouse hotspot from*/
				     /*	  origin of dragged image     */
  SHORT	   cyHotspot;		     /* y offset of mouse hotspot from*/
				     /*	  origin of dragged image     */
  LHANDLE  hImage;		     /* image handle passed to DrgDrag*/
  ULONG	   fl;			     /* flags passed to DrgDrag	      */
  USHORT   cditem;		     /* count of DRAGITEMs	      */
  USHORT   usReserved;		     /* reserved for future use	      */
} DRAGINFO;
typedef DRAGINFO FAR *PDRAGINFO;

typedef struct _DRAGTRANSFER {	/* dxfer */
  ULONG	     cb;		     /* size of control block	      */
  HWND	     hwndClient;	     /* handle of target	      */
  PDRAGITEM  pditem;		     /* DRAGITEM being transferred    */
  HSTR	     hstrSelectedRMF;	     /* rendering mech & fmt of choice*/
  HSTR	     hstrRenderToName;	     /* name source will use	      */
  ULONG	     ulTargetInfo;	     /* reserved for target's use     */
  USHORT     usOperation;	     /* operation being performed     */
  USHORT     fsReply;		     /* reply flags		      */
} DRAGTRANSFER;
typedef DRAGTRANSFER FAR *PDRAGTRANSFER;

#ifdef INCL_16
#define	 DrgAccessDraginfo		   Drg16AccessDraginfo
#define	 DrgAddStrHandle		   Drg16AddStrHandle
#define	 DrgAllocDraginfo		   Drg16AllocDraginfo
#define	 DrgAllocDragtransfer		   Drg16AllocDragtransfer
#define	 DrgDeleteDraginfoStrHandles	   Drg16DeleteDraginfoStrHandles
#define	 DrgDeleteStrHandle		   Drg16DeleteStrHandle
#define	 DrgDrag			   Drg16Drag
#define	 DrgFreeDraginfo		   Drg16FreeDraginfo
#define	 DrgFreeDragtransfer		   Drg16FreeDragtransfer
#define	 DrgGetPS			   Drg16GetPS
#define	 DrgPostTransferMsg		   Drg16PostTransferMsg
#define	 DrgPushDraginfo		   Drg16PushDraginfo
#define	 DrgQueryDragitem		   Drg16QueryDragitem
#define	 DrgQueryDragitemCount		   Drg16QueryDragitemCount
#define	 DrgQueryDragitemPtr		   Drg16QueryDragitemPtr
#define	 DrgQueryNativeRendMechFmt	   Drg16QueryNativeRendMechFmt
#define	 DrgQueryNativeRendMechFmtLen	   Drg16QueryNativeRendMechFmtLen
#define	 DrgQueryStrName		   Drg16QueryStrName
#define	 DrgQueryStrNameLen		   Drg16QueryStrNameLen
#define	 DrgQueryTrueType		   Drg16QueryTrueType
#define	 DrgQueryTrueTypeLen		   Drg16QueryTrueTypeLen
#define	 DrgReleasePS			   Drg16ReleasePS
#define	 DrgSendTransferMsg		   Drg16SendTransferMsg
#define	 DrgSetDragPointer		   Drg16SetDragPointer
#define	 DrgSetDragImage		   Drg16SetDragImage
#define	 DrgSetDragitem			   Drg16SetDragitem
#define	 DrgVerifyNativeRendMechFmt	   Drg16VerifyNativeRendMechFmt
#define	 DrgVerifyRendMechFmt		   Drg16VerifyRendMechFmt
#define	 DrgVerifyTrueType		   Drg16VerifyTrueType
#define	 DrgVerifyType			   Drg16VerifyType
#define	 DrgVerifyTypeSet		   Drg16VerifyTypeSet
#endif

BOOL EXPENTRY DrgAccessDraginfo (PDRAGINFO pdinfo);
HSTR EXPENTRY DrgAddStrHandle (PSZ psz);
PDRAGINFO EXPENTRY DrgAllocDraginfo (USHORT cditem);
PDRAGTRANSFER EXPENTRY DrgAllocDragtransfer (USHORT cdxfer);
BOOL EXPENTRY DrgDeleteDraginfoStrHandles (PDRAGINFO pdinfo);
BOOL EXPENTRY DrgDeleteStrHandle (HSTR hstr);
HWND EXPENTRY DrgDrag (HWND hwndSource, PDRAGINFO pdinfo, LHANDLE hImage,
		       SHORT cx, SHORT cy,
		       SHORT vkTerminate, ULONG fl, PVOID pRsvd);
BOOL EXPENTRY DrgFreeDraginfo (PDRAGINFO pdinfo);
BOOL EXPENTRY DrgFreeDragtransfer (PDRAGTRANSFER pdxfer);
HPS EXPENTRY DrgGetPS (HWND hwnd);
BOOL EXPENTRY DrgPostTransferMsg (HWND hwnd, USHORT msg, PDRAGTRANSFER pdxfer,
				  USHORT fs, USHORT usRsvd, BOOL fRetry);
BOOL EXPENTRY DrgPushDraginfo (PDRAGINFO pdinfo, HWND hwndDest);
BOOL EXPENTRY DrgQueryDragitem (PDRAGINFO pdinfo, USHORT cbBuffer,
				PDRAGITEM pditem, USHORT iItem);
USHORT EXPENTRY DrgQueryDragitemCount (PDRAGINFO pdinfo);
PDRAGITEM EXPENTRY DrgQueryDragitemPtr (PDRAGINFO pdinfo, USHORT i);
BOOL EXPENTRY DrgQueryNativeRendMechFmt (PDRAGITEM pditem,
		      USHORT cbBuffer, PCHAR pBuffer);
USHORT EXPENTRY DrgQueryNativeRendMechFmtLen (PDRAGITEM pditem);
USHORT EXPENTRY DrgQueryStrName (HSTR hstr, USHORT cbBuffer, PSZ pBuffer);
USHORT EXPENTRY DrgQueryStrNameLen (HSTR hstr);
BOOL EXPENTRY DrgQueryTrueType (PDRAGITEM pditem, USHORT cbBuffer, PSZ pBuffer);
USHORT EXPENTRY DrgQueryTrueTypeLen (PDRAGITEM pditem);
BOOL EXPENTRY DrgReleasePS (HPS hps);
MRESULT EXPENTRY DrgSendTransferMsg (HWND hwnd, USHORT msg,
				     MPARAM mp1, MPARAM mp2);
BOOL EXPENTRY DrgSetDragitem (PDRAGINFO pdinfo, PDRAGITEM pditem,
			      USHORT cbBuffer, USHORT iItem);
BOOL EXPENTRY DrgSetDragPointer (PDRAGINFO pdinfo, HPOINTER hptr);
BOOL EXPENTRY DrgSetDragImage (PDRAGINFO pdinfo, LHANDLE hCustom,
			       SHORT cx, SHORT cy, ULONG fl, PVOID pRsvd);
BOOL EXPENTRY DrgVerifyNativeRendMechFmt (PDRAGITEM pditem, PSZ pszRMF);
BOOL EXPENTRY DrgVerifyRendMechFmt (PDRAGITEM pditem, PSZ pszMech, PSZ pszFmt);
BOOL EXPENTRY DrgVerifyTrueType (PDRAGITEM pditem, PSZ pszType);
BOOL EXPENTRY DrgVerifyType (PDRAGITEM pditem, PSZ pszType);
BOOL EXPENTRY DrgVerifyTypeSet (PDRAGITEM pditem, PSZ pszType, USHORT cbMatch,
				PSZ pszMatch);

#endif /* INCL_WINSTDDRAG */

