/*static char *SCCSID = "@(#)bsedos16.h	1.29 90/04/20";*/
/***************************************************************************\
*
* Module Name: BSEDOS16.H
*
* OS/2 Base Include File for 16-bit API
*
* Copyright (c) International Business Machines Corporation 1987
* Copyright (c) Microsoft Corporation 1987
*
* =======================================================================
*
* Subcomponents marked with "+" are partially included by default
*
*   #define:		    To include:
*
* + INCL_DOSPROCESS	    Process and thread support
*   INCL_DOSINFOSEG	    InfoSeg support
* + INCL_DOSFILEMGR	    File Management
* + INCL_DOSMEMMGR	    Memory Management
* + INCL_DOSSEMAPHORES	    Semaphore support
* + INCL_DOSDATETIME	    Date/Time and Timer support
*   INCL_DOSMODULEMGR	    Module manager
* + INCL_DOSRESOURCES	    Resource support
*   INCL_DOSNLS		    National Language Support
*   INCL_DOSSIGNALS	    Signals
*   INCL_DOSMISC	    Miscellaneous
*   INCL_DOSMONITORS	    Monitors
*   INCL_DOSQUEUES	    Queues
*   INCL_DOSSESMGR	    Session Manager Support
*   INCL_DOSDEVICES	    Device specific, ring 2 support
*   INCL_DOSNMPIPES	    Named Pipes Support
*   INCL_DOSPROFILE	    DosProfile API
*   INCL_DOSMVDM	    MVDM support
*
*
\***************************************************************************/

#define INCL_DOSINCLUDED

#ifndef INCL_BASEINCLUDED
#if !(defined(INCL_32) || defined(INCL_16))
#ifdef M_I386
    #define INCL_32
#else /* not M_I386 */
    #define INCL_16
#endif /* M_I386 */
#endif /* INCL_32 || INCL_16 */

#if !defined(INCL_16)
#pragma message ("16-bit Base API included when using 32-bit compiler")
#endif /* INCL_32 */
#endif /* INCL_BASEINCLUDED */

#ifdef INCL_DOS

#define INCL_DOSPROCESS
#define INCL_DOSINFOSEG
#define INCL_DOSFILEMGR
#define INCL_DOSMEMMGR
#define INCL_DOSSEMAPHORES
#define INCL_DOSDATETIME
#define INCL_DOSMODULEMGR
#define INCL_DOSRESOURCES
#define INCL_DOSNLS
#define INCL_DOSSIGNALS
#define INCL_DOSMISC
#define INCL_DOSMONITORS
#define INCL_DOSQUEUES
#define INCL_DOSSESMGR
#define INCL_DOSDEVICES
#define INCL_DOSNMPIPES
#define INCL_DOSPROFILE
#define INCL_DOSMVDM

#endif /* INCL_DOS */

#ifdef INCL_ERRORS
#define INCL_DOSERRORS
#endif /* INCL_ERRORS */

#if (defined(INCL_DOSPROCESS) || !defined(INCL_NOCOMMON))

/* XLATOFF */
#ifndef INCL_NOXLATE_DOS16
#define	DosBeep		Dos16Beep
#define	DosExit		Dos16Exit
#endif /* INCL_NOXLATE_DOS16 */
/* XLATON */

/*** General services */

APIRET	APIENTRY DosBeep(USHORT usFrequency, USHORT usDuration);

/*** Process and Thread support */

VOID APIENTRY DosExit(USHORT fTerminate, USHORT usExitCode);

/* DosExit codes */
#define EXIT_THREAD		   0
#define EXIT_PROCESS		   1

#endif /* common INCL_DOSPROCESS definitions */

#ifdef INCL_DOSPROCESS

typedef struct _PIDINFO {	/* pidi */
    PID	    pid;
    TID	    tid;
    PID	    pidParent;
} PIDINFO;
typedef PIDINFO FAR *PPIDINFO;

#ifdef M_I386
typedef VOID (*PFNTHREAD)(VOID);	/* doesn't need to be PASCAL,	*/
					/* since it takes no parameters */
#else  /* not M_I386 */
typedef VOID (FAR *PFNTHREAD)(VOID);	/* doesn't need to be PASCAL,	*/
					/* since it takes no parameters */
#endif /* M_I386 */

/* XLATOFF */
#ifndef INCL_NOXLATE_DOS16
#define	DosCreateThread	Dos16CreateThread
#define	DosResumeThread	Dos16ResumeThread
#define	DosSuspendThread	Dos16SuspendThread
#define	DosCwait	Dos16Cwait
#define	DosSleep	Dos16Sleep
#define	DosExecPgm	Dos16ExecPgm
#define	DosEnterCritSec	Dos16EnterCritSec
#define	DosExitCritSec	Dos16ExitCritSec
#define	DosExitList	Dos16ExitList
#define	DosGetPrty	Dos16GetPrty
#define	DosSetPrty	Dos16SetPrty
#define	DosGetPID	Dos16GetPID
#define	DosGetPPID	Dos16GetPPID
#define	DosKillProcess	Dos16KillProcess
#endif /* INCL_NOXLATE_DOS16 */
/* XLATON */

APIRET	APIENTRY DosCreateThread(PFNTHREAD pfnFun, PTID pTid, PBYTE pbStack);
APIRET	APIENTRY DosResumeThread(TID tid);
APIRET	APIENTRY DosSuspendThread(TID tid);

/* Action code values */
#define DCWA_PROCESS		   0
#define DCWA_PROCESSTREE	   1

/* Wait option values */
#define DCWW_WAIT		   0
#define DCWW_NOWAIT		   1

typedef struct _RESULTCODES {	/* resc */
    USHORT  codeTerminate;
    USHORT  codeResult;
} RESULTCODES;
typedef RESULTCODES FAR *PRESULTCODES;

APIRET	APIENTRY DosCwait(USHORT fScope, USHORT fWait, PRESULTCODES prescResults,
			 PPID ppidProcess, PID pidWaitProcess);
APIRET	APIENTRY DosSleep(ULONG ulTime);

/* codeTerminate values (also passed to ExitList routines) */
#define TC_EXIT			   0
#define TC_HARDERROR		   1
#define TC_TRAP			   2
#define TC_KILLPROCESS		   3

typedef VOID (PASCAL FAR *PFNEXITLIST)(USHORT);

APIRET	APIENTRY DosEnterCritSec(VOID);
APIRET	APIENTRY DosExitCritSec(VOID);
APIRET	APIENTRY DosExitList(USHORT fFnCode, PFNEXITLIST pfnFunction);

/* DosExitList functions */
#define EXLST_ADD		   1
#define EXLST_REMOVE		   2
#define EXLST_EXIT		   3

APIRET	APIENTRY DosExecPgm(PCHAR pchFailName, SHORT cbFailName,
			   USHORT fExecFlags, PSZ pszArgs, PSZ pszEnv,
			   PRESULTCODES prescResults, PSZ pszPgmName);

/* DosExecPgm functions */
#define EXEC_SYNC		   0
#define EXEC_ASYNC		   1
#define EXEC_ASYNCRESULT	   2
#define EXEC_TRACE		   3
#define EXEC_BACKGROUND		   4
#define EXEC_LOAD		   5

APIRET	APIENTRY DosGetPID(PPIDINFO ppidInfo);
APIRET	APIENTRY DosGetPPID(USHORT pidChild, PUSHORT ppidParent);

APIRET	APIENTRY DosGetPrty(USHORT usScope, PUSHORT pusPriority, USHORT pid);
APIRET	APIENTRY DosSetPrty(USHORT usScope, USHORT fPrtyClass, SHORT sChange,
			   USHORT id);

/* Priority scopes */
#define PRTYS_PROCESS		   0
#define PRTYS_PROCESSTREE	   1
#define PRTYS_THREAD		   2

/* Priority classes */
#define PRTYC_NOCHANGE		   0
#define PRTYC_IDLETIME		   1
#define PRTYC_REGULAR		   2
#define PRTYC_TIMECRITICAL	   3
#define PRTYC_FOREGROUNDSERVER	   4

/* Priority deltas */
#define PRTYD_MINIMUM		  -31
#define PRTYD_MAXIMUM		   31

APIRET	APIENTRY DosKillProcess(USHORT usScope, PID pidProcess);

#define DKP_PROCESSTREE		   0
#define DKP_PROCESS		   1

#endif /* INCL_DOSPROCESS */


/*** InfoSeg support */

#ifdef INCL_DOSINFOSEG

/* Global Information Segment */

typedef struct _GINFOSEG {	/* gis */
    ULONG   time;
    ULONG   msecs;
    UCHAR   hour;
    UCHAR   minutes;
    UCHAR   seconds;
    UCHAR   hundredths;
    USHORT  timezone;
    USHORT  cusecTimerInterval;
    UCHAR   day;
    UCHAR   month;
    USHORT  year;
    UCHAR   weekday;
    UCHAR   uchMajorVersion;
    UCHAR   uchMinorVersion;
    UCHAR   chRevisionLetter;
    UCHAR   sgCurrent;
    UCHAR   sgMax;
    UCHAR   cHugeShift;
    UCHAR   fProtectModeOnly;
    USHORT  pidForeground;
    UCHAR   fDynamicSched;
    UCHAR   csecMaxWait;
    USHORT  cmsecMinSlice;
    USHORT  cmsecMaxSlice;
    USHORT  bootdrive;
    UCHAR   amecRAS[32];
    UCHAR   csgWindowableVioMax;
    UCHAR   csgPMMax;
} GINFOSEG;
typedef GINFOSEG FAR *PGINFOSEG;

/* Local Information Segment */

typedef struct _LINFOSEG {	/* lis */
    PID	    pidCurrent;
    PID	    pidParent;
    USHORT  prtyCurrent;
    TID	    tidCurrent;
    USHORT  sgCurrent;
    UCHAR   rfProcStatus;
    UCHAR   dummy1;
    BOOL    fForeground;
    UCHAR   typeProcess;
    UCHAR   dummy2;
    SEL	    selEnvironment;
    USHORT  offCmdLine;
    USHORT  cbDataSegment;
    USHORT  cbStack;
    USHORT  cbHeap;
    HMODULE hmod;
    SEL	    selDS;
} LINFOSEG;
typedef LINFOSEG FAR *PLINFOSEG;

/* Process Type codes (local information segment typeProcess field)	      */

#define PT_FULLSCREEN		   0 /* Full screen application		      */
#define PT_REALMODE		   1 /* Real mode process		      */
#define PT_WINDOWABLEVIO	   2 /* VIO windowable application	      */
#define PT_PM			   3 /* Presentation Manager application      */
#define PT_DETACHED		   4 /* Detached application		      */

/* Process Status Flag definitions (local info seg rfProcStatus field)	      */

#define PS_EXITLIST		   1 /* Thread is in exitlist routine	      */


/* XLATOFF */
#ifndef INCL_NOXLATE_DOS16
#define	DosGetInfoSeg	Dos16GetInfoSeg
#endif /* INCL_NOXLATE_DOS16 */
/* XLATON */

APIRET	APIENTRY DosGetInfoSeg(PSEL pselGlobal, PSEL pselLocal);

/* Helper macros used to convert selector to PINFOSEG or LINFOSEG	      */

#define MAKEPGINFOSEG(sel)  ((PGINFOSEG)MAKEP(sel, 0))
#define MAKEPLINFOSEG(sel)  ((PLINFOSEG)MAKEP(sel, 0))

#endif /* INCL_DOSINFOSEG */

#ifndef INCL_SAADEFS

/*
 * CCHMAXPATH is the maximum fully qualified path name length including
 * the drive letter, colon, backslashes and terminating NULL.
 */
#define CCHMAXPATH		   260

/*
 * CCHMAXPATHCOMP is the maximum individual path component name length
 * including a terminating NULL.
 */
#define CCHMAXPATHCOMP		   256

#endif	/* !INCL_SAADEFS */

#if (defined(INCL_DOSFILEMGR) || !defined(INCL_NOCOMMON))

/*** File manager */

/* DosSetFilePtr() file position codes */

#define FILE_BEGIN		   0x0000 /* relative to beginning of file    */
#define FILE_CURRENT		   0x0001 /* relative to current fptr position*/
#define FILE_END		   0x0002 /* relative to end of file	      */

/* DosFindFirst/Next Directory handle types */

#define HDIR_SYSTEM		   1	/* Use system handle (1)	      */
#define HDIR_CREATE		   (-1)	/* Allocate a new, unused handle   */

/* DosCopy control bits - may be or'ed together */
#define DCPY_EXISTING		   0x00001 /* Copy even if target exists      */
#define DCPY_APPEND		   0x00002 /* Append to existing file, don't replace*/
#define DCPY_FAILEAS		   0x00004 /* Fail if EAs not supported on target*/

/* Dosopen/DosQFHandState/DosQueryFileInfo et al file attributes; also	      */
/* known as Dso File Mode bits...					      */
#define FILE_NORMAL		   0x0000
#define FILE_READONLY		   0x0001
#define FILE_HIDDEN		   0x0002
#define FILE_SYSTEM		   0x0004
#define FILE_DIRECTORY		   0x0010
#define FILE_ARCHIVED		   0x0020

/* DosOpen() actions */
#define FILE_EXISTED		   0x0001
#define FILE_CREATED		   0x0002
#define FILE_TRUNCATED		   0x0003

/* DosOpen() open flags */
#define FILE_OPEN		   0x0001
#define FILE_TRUNCATE		   0x0002
#define FILE_CREATE		   0x0010

/* applies if file already exists */

#define OPEN_ACTION_FAIL_IF_EXISTS     0x0000  /* ---- ---- ---- 0000	      */
#define OPEN_ACTION_OPEN_IF_EXISTS     0x0001  /* ---- ---- ---- 0001	      */
#define OPEN_ACTION_REPLACE_IF_EXISTS  0x0002  /* ---- ---- ---- 0010	      */

/* applies if file does not exist */

#define OPEN_ACTION_FAIL_IF_NEW	       0x0000  /* ---- ---- 0000 ----	      */
#define OPEN_ACTION_CREATE_IF_NEW      0x0010  /* ---- ---- 0001 ----	      */

/* DosOpen/DosSetFHandState flags */

#define OPEN_ACCESS_READONLY	       0x0000  /* ---- ---- ---- -000	      */
#define OPEN_ACCESS_WRITEONLY	       0x0001  /* ---- ---- ---- -001	      */
#define OPEN_ACCESS_READWRITE	       0x0002  /* ---- ---- ---- -010	      */
#define OPEN_SHARE_DENYREADWRITE       0x0010  /* ---- ---- -001 ----	      */
#define OPEN_SHARE_DENYWRITE	       0x0020  /* ---- ---- -010 ----	      */
#define OPEN_SHARE_DENYREAD	       0x0030  /* ---- ---- -011 ----	      */
#define OPEN_SHARE_DENYNONE	       0x0040  /* ---- ---- -100 ----	      */
#define OPEN_FLAGS_NOINHERIT	       0x0080  /* ---- ---- 1--- ----	      */
#define OPEN_FLAGS_NO_LOCALITY	       0x0000  /* ---- -000 ---- ----	      */
#define OPEN_FLAGS_SEQUENTIAL	       0x0100  /* ---- -001 ---- ----	      */
#define OPEN_FLAGS_RANDOM	       0x0200  /* ---- -010 ---- ----	      */
#define OPEN_FLAGS_RANDOMSEQUENTIAL    0x0300  /* ---- -011 ---- ----	      */
#define OPEN_FLAGS_NO_CACHE	       0x1000  /* ---1 ---- ---- ----	      */
#define OPEN_FLAGS_FAIL_ON_ERROR       0x2000  /* --1- ---- ---- ----	      */
#define OPEN_FLAGS_WRITE_THROUGH       0x4000  /* -1-- ---- ---- ----	      */
#define OPEN_FLAGS_DASD		       0x8000  /* 1--- ---- ---- ----	      */


/* DosSearchPath() constants */

#define SEARCH_PATH		   0x0000
#define SEARCH_CUR_DIRECTORY	   0x0001
#define SEARCH_ENVIRONMENT	   0x0002
#define SEARCH_IGNORENETERRS	   0x0004

/*
 * DosFileIO
 */
/* File IO command words */
#define FIO_LOCK		   0	   /* Lock Files		      */
#define FIO_UNLOCK		   1	   /* Unlock Files		      */
#define FIO_SEEK		   2	   /* Seek (set file ptr)	      */
#define FIO_READ		   3	   /* File Read			      */
#define FIO_WRITE		   4	   /* File Write		      */

/* Lock Sharing Modes */
#define FIO_NOSHARE		   0	   /* None			      */
#define FIO_SHAREREAD		   1	   /* Read-Only			      */

typedef struct	_FIOLOCKCMD {	/* FLC	FileLockCmd prefix	     */
    USHORT  usCmd;		/* Cmd = FIO_LOCK		     */
    USHORT  cLockCnt;		/* Lock records that follow	     */
    ULONG   cTimeOut;		/* in Msec			     */
} FIOLOCKCMD;
typedef FIOLOCKCMD FAR *PFIOLOCKCMD;


typedef struct	_FIOLOCKREC {	/* FLR FileLockRecord		     */
     USHORT fShare;		/* FIO_NOSHARE or FIO_SHAREREAD	     */
     ULONG  cbStart;		/* Starting offset for lock region   */
     ULONG  cbLength;		/* Length of lock region	     */
} FIOLOCKREC;
typedef FIOLOCKREC FAR *PFIOLOCKREC;


typedef struct	_FIOUNLOCKCMD { /* FUC FileUnlockCmd prefix	     */
    USHORT  usCmd;		/* Cmd = FIO_UNLOCK		     */
    USHORT  cUnlockCnt;		/* Unlock records that follow	     */
} FIOUNLOCKCMD;
typedef FIOUNLOCKCMD FAR *PFIOUNLOCKCMD;


typedef struct	_FIOUNLOCKREC { /* FUR FileUnlockRecord		     */
    ULONG   cbStart;		/* Starting offset for unlock region */
    ULONG   cbLength;		/* Length of unlock region	     */
} FIOUNLOCKREC;
typedef FIOUNLOCKREC FAR *PFIOUNLOCKREC;


typedef struct	_FIOSEEKCMD {	/* FSC Seek command structure		  */
    USHORT  usCmd;		/* Cmd = FIO_SEEK			  */
    USHORT  fsMethod;		/* One of&gml FPM_BEGINNING, FPM_CURRENT, */
				/* or FPM_END				  */
    ULONG   cbDistance;		/* Byte offset for seek			  */
    ULONG   cbNewPosition;	/* Bytes from start of file after seek	  */
} FIOSEEKCMD;
typedef FIOSEEKCMD FAR *PFIOSEEKCMD;


typedef struct	_FIOREADWRITE { /* RWC Read&Write command structure	  */
    USHORT  usCmd;		/* Cmd = FIO_READ or FIO_WRITE		  */
    PVOID   pbBuffer;		/* Pointer to data buffer		  */
    ULONG   cbBufferLen;	/* Bytes in buffer or max size		  */
    ULONG   cbActualLen;	/* Bytes actually read/written		  */
} FIOREADWRITE;
typedef FIOREADWRITE FAR *PFIOREADWRITE;


/***
 * EA Info Levels & Find First/Next
 * API's: DosFindFirst, DosQueryFileInfo, DosQueryPathInfo, DosSetFileInfo,
 *	DosSetPathInfo
 */

/* File info levels : All listed API's */
#define FIL_STANDARD		   1   /* Info level 1, standard file info */
#define FIL_QUERYEASIZE		   2   /* Level 2, return Full EA size	   */
#define FIL_QUERYEASFROMLIST	   3   /* Level 3, return requested EA's   */
#define	FIL_QUERYALLEAS		   4   /* Level 4, return all EA's         */

/* File info levels: Dos...PathInfo only */
#define FIL_QUERYFULLNAME	   5   /* Level 5, return fully qualified  */
				       /*   name of file		   */
#define FIL_NAMEISVALID		   6   /* Level 6, check validity of	   */
				       /* file/path name for this FSD	   */

/* DosFindNotifyFirst() */
#define FNOTIL_STANDARD		   1   /* Find-Notify Info level 1&gml Return */
				       /* standard directory change info      */

/* DosFsAttach() */
/* Attact or detach */
#define FSATTACH		   0	   /* Attach file server	      */
#define FSDETACH		   1	   /* Detach file server	      */

/* DosFsCtl() */
/* Routing type */
#define FSCTL_HANDLE		   1	   /* File Handle directs req routing */
#define FSCTL_PATHNAME		   2	   /* Path Name directs req routing   */
#define FSCTL_FSDNAME		   3	   /* FSD Name directs req routing    */

/* DosQueryFSAttach() */
/* Information level types (defines method of query) */
#define FSAIL_QUERYNAME		   1	   /* Return data for a Drive or Device */
#define FSAIL_DEVNUMBER		   2	   /* Return data for Ordinal Device #	*/
#define FSAIL_DRVNUMBER		   3	   /* Return data for Ordinal Drive #	*/

/* Item types (from data structure item "iType") */
#define FSAT_CHARDEV		   1	   /* Resident character device	   */
#define FSAT_PSEUDODEV		   2	   /* Pusedu-character device	   */
#define FSAT_LOCALDRV		   3	   /* Local drive		   */
#define FSAT_REMOTEDRV		   4	   /* Remote drive attached to FSD */

typedef struct	_FSQBUFFER {	/* fsqbuf Data structure for QFSAttach*/
    USHORT  iType;		/* Item type			      */
    USHORT  cbName;		/* Length of item name, sans NULL     */
    UCHAR   szName[1];		/* ASCIIZ item name		      */
    USHORT  cbFSDName;		/* Length of FSD name, sans NULL      */
    UCHAR   szFSDName[1];	/* ASCIIZ FSD name		      */
    USHORT  cbFSAData;		/* Length of FSD Attach data returned */
    UCHAR   rgFSAData[1];	/* FSD Attach data from FSD	      */
} FSQBUFFER;
typedef FSQBUFFER FAR *PFSQBUFFER;

/*
 * File System Drive Information&gml DosQueryFSInfo DosSetFSInfo
 */

/* FS Drive Info Levels							   */
#define FSIL_ALLOC		   1 /* Drive allocation info (Query only) */
#define FSIL_VOLSER		   2 /* Drive Volume/Serial information	   */

/* DosQueryFHType() */
/* Handle classes (low 8 bits of Handle Type)				   */
#define FHT_DISKFILE		   0x0000  /* Disk file handle		   */
#define FHT_CHRDEV		   0x0001  /* Character device handle	   */
#define FHT_PIPE		   0x0002  /* Pipe handle		   */

/* Handle bits (high 8 bits of Handle Type)				   */
#define FHB_DSKREMOTE		   0x8000  /* Remote disk		   */
#define FHB_CHRDEVREMOTE	   0x8000  /* Remote character device	   */
#define FHB_PIPEREMOTE		   0x8000  /* Remote pipe		   */


typedef SHANDLE HFILE;		/* hf */
typedef HFILE FAR *PHFILE;

#ifndef INCL_SAADEFS

/* File time and date types */

typedef struct _FTIME {		/* ftime */
    USHORT twosecs : 5;
    USHORT minutes : 6;
    USHORT hours   : 5;
} FTIME;
typedef FTIME FAR *PFTIME;

typedef struct _FDATE {		/* fdate */
    USHORT day	   : 5;
    USHORT month   : 4;
    USHORT year	   : 7;
} FDATE;
typedef FDATE FAR *PFDATE;

typedef struct _FILEFINDBUF {	/* findbuf */
    FDATE   fdateCreation;
    FTIME   ftimeCreation;
    FDATE   fdateLastAccess;
    FTIME   ftimeLastAccess;
    FDATE   fdateLastWrite;
    FTIME   ftimeLastWrite;
    ULONG   cbFile;
    ULONG   cbFileAlloc;
    USHORT  attrFile;
    UCHAR   cchName;
    CHAR    achName[CCHMAXPATHCOMP];
} FILEFINDBUF;
typedef FILEFINDBUF FAR *PFILEFINDBUF;

typedef struct _FILEFINDBUF2 {	/* findbuf2 */
    FDATE   fdateCreation;
    FTIME   ftimeCreation;
    FDATE   fdateLastAccess;
    FTIME   ftimeLastAccess;
    FDATE   fdateLastWrite;
    FTIME   ftimeLastWrite;
    ULONG   cbFile;
    ULONG   cbFileAlloc;
    USHORT  attrFile;
    ULONG   cbList;
    UCHAR   cchName;
    CHAR    achName[CCHMAXPATHCOMP];
} FILEFINDBUF2;
typedef FILEFINDBUF2 FAR *PFILEFINDBUF2;

/* extended attribute structures */
typedef struct _GEA {	       /* gea */
    BYTE    cbName;	       /* name length not including NULL	  */
    CHAR    szName[1];	       /* attribute name			  */
} GEA;
typedef GEA FAR *PGEA;

typedef struct _GEALIST {	/* geal */
    ULONG  cbList;		/* total bytes of structure inc full list */
    GEA	   list[1];		/* variable length GEA structures	  */
} GEALIST;
typedef GEALIST FAR * PGEALIST;

typedef struct _FEA {		/* fea */
    BYTE    fEA;		/* flags				  */
    BYTE    cbName;		/* name length not including NULL	  */
    USHORT  cbValue;		/* value length				  */
} FEA;
typedef FEA FAR *PFEA;

/* flags for _FEA.fEA */

#define FEA_NEEDEA 0x80	    /* need EA bit */

typedef struct _FEALIST {	/* feal */
    ULONG  cbList;		/* total bytes of structure inc full list */
    FEA	   list[1];		/* variable length FEA structures	  */
} FEALIST;
typedef FEALIST FAR * PFEALIST;

typedef struct _EAOP {		/* eaop */
    PGEALIST fpGEAList;		/* general EA list */
    PFEALIST fpFEAList;		/* full EA list	   */
    ULONG    oError;
} EAOP;
typedef EAOP FAR * PEAOP;


/*
 * Equates for EA types
 *
 * Values 0xFFFE thru 0x8000 are reserved.
 * Values 0x0000 thru 0x7fff are user definable.
 * Value  0xFFFC is not used
 */

#define	    EAT_BINARY		   0xFFFE /* length preceeded binary	      */
#define	    EAT_ASCII		   0xFFFD /* length preceeded ASCII	      */
#define	    EAT_BITMAP		   0xFFFB /* length preceeded bitmap	      */
#define	    EAT_METAFILE	   0xFFFA /* length preceeded metafile	      */
#define	    EAT_ICON		   0xFFF9 /* length preceeded icon	      */
#define	    EAT_EA		   0xFFEE /* length preceeded ASCII extended attribute */
					  /* name of associated data (#include)*/
#define	    EAT_MVMT		   0xFFDF /* multi-valued, multi-typed field  */
#define	    EAT_MVST		   0xFFDE /* multi-valued, single-typed field */
#define	    EAT_ASN1		   0xFFDD /* ASN.1 field		      */


#endif	/* !INCL_SAADEFS */

/* XLATOFF */
#ifndef INCL_NOXLATE_DOS16
#define	DosBufReset	Dos16BufReset
#define	DosChDir	Dos16ChDir
#define	DosChgFilePtr	Dos16ChgFilePtr
#define	DosClose	Dos16Close
#define	DosCopy	Dos16Copy
#define	DosDelete	Dos16Delete
#define	DosDupHandle	Dos16DupHandle
#define	DosEditName	Dos16EditName
#define	DosEnumAttribute	Dos16EnumAttribute
#define	DosFileIO	Dos16FileIO
#define	DosFileLocks	Dos16FileLocks
#define	DosFindClose	Dos16FindClose
#define	DosFindFirst	Dos16FindFirst
#define	DosFindFirst2	Dos16FindFirst2
#define	DosFindNext	Dos16FindNext
#define	DosFindNotifyClose	Dos16FindNotifyClose
#define	DosFindNotifyFirst	Dos16FindNotifyFirst
#define	DosFindNotifyNext	Dos16FindNotifyNext
#define	DosFSAttach	Dos16FSAttach
#define	DosFSCtl	Dos16FSCtl
#define	DosMkDir	Dos16MkDir
#define	DosMkDir2	Dos16MkDir2
#define	DosMove		Dos16Move
#define	DosNewSize	Dos16NewSize
#define	DosOpen		Dos16Open
#define	DosOpen2	Dos16Open2
#define	DosQCurDir	Dos16QCurDir
#define	DosQCurDisk	Dos16QCurDisk
#define	DosQFHandState	Dos16QFHandState
#define	DosQFileInfo	Dos16QFileInfo
#define	DosQFileMode	Dos16QFileMode
#define	DosQFSAttach	Dos16QFSAttach
#define	DosQFSInfo	Dos16QFSInfo
#define	DosQHandType	Dos16QHandType
#define	DosQPathInfo	Dos16QPathInfo
#define	DosQVerify	Dos16QVerify
#define	DosRead		Dos16Read
#define	DosReadAsync	Dos16ReadAsync 
#define	DosRmDir	Dos16RmDir
#define	DosSelectDisk	Dos16SelectDisk
#define	DosSetFHandState	Dos16SetFHandState
#define	DosSetFileInfo	Dos16SetFileInfo
#define	DosSetFileMode	Dos16SetFileMode
#define	DosSetFilePtr	Dos16SetFilePtr
#define	DosSetFSInfo	Dos16SetFSInfo
#define	DosSetMaxFH	Dos16SetMaxFH
#define	DosSetPathInfo	Dos16SetPathInfo
#define	DosSetVerify	Dos16SetVerify
#define	DosShutdown	Dos16Shutdown
#define	DosWrite	Dos16Write
#define	DosWriteAsync	Dos16WriteAsync
#endif /* INCL_NOXLATE_DOS16 */
/* XLATON */

APIRET	APIENTRY DosOpen(PSZ pszFname, PHFILE phfOpen, PUSHORT pusAction,
			ULONG ulFSize, USHORT usAttr, USHORT fsOpenFlags,
			USHORT fsOpenMode, ULONG ulReserved);
APIRET	APIENTRY DosOpen2(PSZ pszFname, PHFILE phf, PUSHORT pusAction,
			 ULONG ulFSize, USHORT usAttr, USHORT usOpenFlags,
			 ULONG usOpenMode, PEAOP pvEABuf, ULONG ulReserved);
APIRET	APIENTRY DosClose(HFILE hf);
APIRET	APIENTRY DosRead(HFILE hf, PVOID pBuf, USHORT cbBuf,
			PUSHORT pcbBytesRead);
APIRET	APIENTRY DosWrite(HFILE hf, PVOID bBuf, USHORT cbBuf,
			 PUSHORT pcbBytesWritten);

/* File system shutdown */

APIRET	APIENTRY DosShutdown(ULONG ulReserved);

/* File time and date types */

typedef struct _FILESTATUS {	/* fsts */
    FDATE   fdateCreation;
    FTIME   ftimeCreation;
    FDATE   fdateLastAccess;
    FTIME   ftimeLastAccess;
    FDATE   fdateLastWrite;
    FTIME   ftimeLastWrite;
    ULONG   cbFile;
    ULONG   cbFileAlloc;
    USHORT  attrFile;
} FILESTATUS;
typedef FILESTATUS FAR *PFILESTATUS;

typedef struct _FILESTATUS2 {  /* fsts2 */
    FDATE   fdateCreation;
    FTIME   ftimeCreation;
    FDATE   fdateLastAccess;
    FTIME   ftimeLastAccess;
    FDATE   fdateLastWrite;
    FTIME   ftimeLastWrite;
    ULONG   cbFile;
    ULONG   cbFileAlloc;
    USHORT  attrFile;
    ULONG   cbList;
} FILESTATUS2;
typedef FILESTATUS2 FAR *PFILESTATUS2;

typedef struct _FSALLOCATE {	/* fsalloc */
    ULONG   idFileSystem;
    ULONG   cSectorUnit;
    ULONG   cUnit;
    ULONG   cUnitAvail;
    USHORT  cbSector;
} FSALLOCATE;
typedef FSALLOCATE FAR *PFSALLOCATE;

typedef struct _VOLUMELABEL {	/* vol */
    BYTE    cch;
    CHAR    szVolLabel[12];
} VOLUMELABEL;
typedef VOLUMELABEL FAR *PVOLUMELABEL;

typedef struct _FSINFO {	/* fsinf */
    FDATE   fdateCreation;
    FTIME   ftimeCreation;
    VOLUMELABEL vol;
} FSINFO;
typedef FSINFO FAR *PFSINFO;

/* HANDTYPE values */

#define HANDTYPE_FILE		   0x0000
#define HANDTYPE_DEVICE		   0x0001
#define HANDTYPE_PIPE		   0x0002
#define HANDTYPE_NETWORK	   0x8000

typedef struct _FILELOCK {	/* flock */
    LONG    lOffset;
    LONG    lRange;
} FILELOCK;
typedef FILELOCK FAR *PFILELOCK;

typedef SHANDLE HDIR;		/* hdir */
typedef HDIR FAR *PHDIR;

APIRET	APIENTRY DosDelete(PSZ pszFName, ULONG ulReserverd);
APIRET	APIENTRY DosDupHandle(HFILE hfOld, PHFILE phfNew);

APIRET	APIENTRY DosQFHandState(HFILE hf, PUSHORT pfsOpenMode);
APIRET	APIENTRY DosSetFHandState(HFILE hf, USHORT fsState);
APIRET	APIENTRY DosQHandType(HFILE hf, PUSHORT pfsType, PUSHORT pusDevAttr);

APIRET	APIENTRY DosReadAsync (HFILE hf, PULONG hsemRam, PUSHORT pusErrCode,
			      PVOID pBuf, USHORT cbBuf, PUSHORT pcbBytesRead);
APIRET	APIENTRY DosWriteAsync(HFILE hf, PULONG hsemRam, PUSHORT pusErrCode,
			      PVOID pBuf, USHORT cbBuf, PUSHORT pcbBytesWritten);

APIRET	APIENTRY DosFindFirst(PSZ pszFSpec, PHDIR phdir, USHORT usAttr,
			     PFILEFINDBUF pffb, USHORT cbBuf, PUSHORT pcSearch,
			     ULONG ulReserved);
APIRET	APIENTRY DosFindFirst2(PSZ pszFSpec, PHDIR phdir, USHORT usAttr,
			      PVOID pBuf, USHORT cbBuf, PUSHORT pcSearch,
			      USHORT usInfoLevel, ULONG ulReserved);
APIRET	APIENTRY DosFindNext(HDIR hdir, PFILEFINDBUF pffb, USHORT cbBuf,
			    PUSHORT pcSearch);
APIRET	APIENTRY DosFindClose(HDIR hdir);
APIRET	APIENTRY DosFindNotifyFirst(PSZ pszPath, PHDIR hdir, USHORT usAttr,
				   PVOID pBuf, USHORT cbBuf, PUSHORT pcChg,
				   USHORT usInfoLevel, ULONG ulTimeOut,
				   ULONG ulReserved);
APIRET	APIENTRY DosFindNotifyNext(HDIR hDir, PVOID pBuf, USHORT cbBuf,
				  PUSHORT pcChg, ULONG ulTimeOut);
APIRET	APIENTRY DosFindNotifyClose(HDIR hDir);

APIRET	APIENTRY DosFSAttach(PSZ pszDevName, PSZ pszFSD, PVOID pData,
			    USHORT cbData, USHORT fsOp, ULONG ulReserved);
APIRET	APIENTRY DosQFSAttach(PSZ pszDev, USHORT usOrdinal, USHORT usInfoLevel,
			     PVOID pFSAttBuf, PUSHORT cbBuf, ULONG ulReserved);
APIRET	APIENTRY DosFSCtl(PVOID pData, USHORT cbData, PUSHORT pcbData,
			 PVOID pParms, USHORT cbParms, PUSHORT pcbParms,
			 USHORT usFunCode, PSZ pszRoute, HFILE hf,
			 USHORT usRouteMethod, ULONG ulReserved);

APIRET	APIENTRY DosNewSize(HFILE hf, ULONG ulNewSize);
APIRET	APIENTRY DosBufReset(HFILE hf);

APIRET	APIENTRY DosChgFilePtr(HFILE hf, LONG lOffset, USHORT fsMethod,
			      PULONG pulNewOffset);

APIRET	APIENTRY DosSetFilePtr(HFILE hf, LONG lOffset, USHORT fsMethod,
			      PULONG pulNewOffset);

APIRET	APIENTRY DosFileLocks(HFILE hf, PLONG pUnlock, PLONG pLock);

APIRET	APIENTRY DosMove(PSZ pszOld, PSZ pszNew, ULONG ulReserved);
APIRET	APIENTRY DosCopy(PSZ pszSrc, PSZ pszDst, USHORT usOpt, ULONG ulReserved);
APIRET	APIENTRY DosEditName(USHORT usEditLevel, PSZ pszSrc, PSZ pszEdit,
			    PBYTE pszDst, USHORT cbDst);

APIRET	APIENTRY DosFileIO(HFILE hf, PVOID pbCmd, USHORT cbCmd, PUSHORT pusErr);
APIRET	APIENTRY DosMkDir(PSZ pszDirName, ULONG usReserved);
APIRET	APIENTRY DosMkDir2(PSZ pszDir, PEAOP pBuf, ULONG ulReserved);
APIRET	APIENTRY DosRmDir(PSZ pszDir, ULONG ulReserved);
APIRET	APIENTRY DosSelectDisk(USHORT usDrvNum);
APIRET	APIENTRY DosQCurDisk(PUSHORT pusDrvNum, PULONG pulLogDrvMap);

APIRET	APIENTRY DosChDir(PSZ pszDir, ULONG ulReserved);
APIRET	APIENTRY DosQCurDir(USHORT usDrvNum, PBYTE pszPathBuf,
			   PUSHORT pcbPathBuf);

APIRET	APIENTRY DosQFSInfo(USHORT usDrvNum, USHORT usInfoLevel, PVOID pbInfo,
			   USHORT cbInfo);
APIRET	APIENTRY DosSetFSInfo(USHORT usDrvNum, USHORT usInfoLevel, PVOID pBuf,
			     USHORT cbBuf);
APIRET	APIENTRY DosQVerify(PUSHORT pfVerifyOn);
APIRET	APIENTRY DosSetVerify(USHORT fVerify);
APIRET	APIENTRY DosSetMaxFH(USHORT usHandles);

APIRET	APIENTRY DosQFileInfo(HFILE hf, USHORT usInfoLevel, PVOID pInfoBuf,
			     USHORT cbInfoBuf);
APIRET	APIENTRY DosSetFileInfo(HFILE hf, USHORT usInfoLevel, PVOID pInfoBuf,
			       USHORT cbInfoBuf);
APIRET	APIENTRY DosQPathInfo(PSZ pszPath, USHORT usInfoLevel, PVOID pInfoBuf,
			     USHORT cbInfoBuf, ULONG ulReserved);
APIRET	APIENTRY DosSetPathInfo(PSZ pszPath, USHORT usInfoLevel, PVOID pInfoBuf,
			       USHORT cbInfoBuf, USHORT usFlags,
			       ULONG ulReserved);

/* defines for dossetpathinfo -pathinfo flag */
#define DSPI_WRTTHRU	0x10	/* write through */

APIRET	APIENTRY DosQFileMode(PSZ pszFName, PUSHORT pusAttr, ULONG ulReserved);
APIRET	APIENTRY DosSetFileMode(PSZ pszFName, USHORT usAttr, ULONG ulReserved);

APIRET	APIENTRY DosEnumAttribute(USHORT fRefType, PVOID pFileRef, ULONG iStartEntry, PVOID pEnumBuf, ULONG cbBuf, PULONG pcbActual, ULONG infoLevel, ULONG reserved);

typedef struct _DENA1 {	/* _dena1 level 1 info returned from DosEnumAttribute */
    UCHAR   reserved;	      /* 0					     */
    UCHAR   cbName;	      /* length of name exculding NULL		     */
    USHORT  cbValue;	      /* length of value			     */
    UCHAR   szName[1];	      /* variable length asciiz name		     */
} DENA1;
typedef DENA1 FAR *PDENA1;

/* Infolevels for DosEnumAttribute  */
#define	ENUMEA_LEVEL_NO_VALUE	1L      /* FEA without value */
/* Reference types for DosEnumAttribute */
#define	ENUMEA_REFTYPE_FHANDLE	0       /* file handle */
#define	ENUMEA_REFTYPE_PATH	1       /* path name */
#define	ENUMEA_REFTYPE_MAX	ENUMEA_REFTYPE_PATH 

#endif /* common INCL_DOSFILEMGR */

#if (defined(INCL_DOSMEMMGR) || !defined(INCL_NOCOMMON))
/*** Memory management */

/* XLATOFF */
#ifndef INCL_NOXLATE_DOS16
#define	DosAllocSeg	Dos16AllocSeg
#define	DosFreeSeg	Dos16FreeSeg
#define	DosGetSeg	Dos16GetSeg
#define	DosGiveSeg	Dos16GiveSeg
#define	DosReallocSeg	Dos16ReallocSeg
#define	DosSizeSeg	Dos16SizeSeg
#endif /* INCL_NOXLATE_DOS16 */
/* XLATON */


APIRET	APIENTRY DosAllocSeg(USHORT cbSize, PSEL pSel, USHORT fsAlloc);
APIRET	APIENTRY DosFreeSeg(SEL sel);
APIRET	APIENTRY DosGetSeg(SEL sel);
APIRET	APIENTRY DosGiveSeg(SEL sel, PID pid, PSEL pSelRecipient);
APIRET	APIENTRY DosReallocSeg(USHORT cbNewSize, SEL sel);
APIRET	APIENTRY DosSizeSeg(SEL sel, PULONG pcbSize);

/* Segment attribute flags (used with DosAllocSeg) */

#define SEG_NONSHARED		   0x0000
#define SEG_GIVEABLE		   0x0001
#define SEG_GETTABLE		   0x0002
#define SEG_DISCARDABLE		   0x0004

#endif /* common INCL_DOSMEMMGR */

#ifdef INCL_DOSMEMMGR

/* XLATOFF */
#ifndef INCL_NOXLATE_DOS16
#define	DosAllocHuge	Dos16AllocHuge
#define	DosAllocShrSeg	Dos16AllocShrSeg
#define	DosCreateCSAlias	Dos16CreateCSAlias
#define	DosGetHugeShift	Dos16GetHugeShift
#define	DosGetShrSeg	Dos16GetShrSeg
#define	DosLockSeg	Dos16LockSeg
#define	DosMemAvail	Dos16MemAvail
#define	DosReallocHuge	Dos16ReallocHuge
#define	DosSubAlloc	Dos16SubAlloc
#define	DosSubFree	Dos16SubFree
#define	DosSubSet	Dos16SubSet
#define	DosUnlockSeg	Dos16UnlockSeg
#endif /* INCL_NOXLATE_DOS16 */
/* XLATON */

APIRET	APIENTRY DosAllocHuge(USHORT cSegs, USHORT cbPartialSeg, PSEL psel,
			     USHORT cMaxSegs, USHORT fsAlloc);
APIRET	APIENTRY DosReallocHuge(USHORT cSegs, USHORT cbPartialSeg, SEL sel);
APIRET	APIENTRY DosGetHugeShift(PUSHORT pusShiftCount);

APIRET	APIENTRY DosAllocShrSeg(USHORT cbSeg, PSZ pszSegName, PSEL psel);

APIRET	APIENTRY DosLockSeg(SEL sel);
APIRET	APIENTRY DosUnlockSeg(SEL sel);

APIRET	APIENTRY DosGetShrSeg(PSZ pszSegName, PSEL psel);

APIRET	APIENTRY DosMemAvail(PULONG pcbFree);
APIRET	APIENTRY DosCreateCSAlias(SEL selDS, PSEL pselCS);

APIRET	APIENTRY DosSubAlloc(SEL sel, PUSHORT pusOffset, USHORT cb);
APIRET	APIENTRY DosSubFree(SEL sel, USHORT offBlock, USHORT cb);
APIRET	APIENTRY DosSubSet(SEL sel, USHORT fFlags, USHORT cbNew);

#endif /* INCL_DOSMEMMGR */

#if (defined(INCL_DOSSEMAPHORES) || !defined(INCL_NOCOMMON))

/*** Semaphore support */

#define SEM_INDEFINITE_WAIT	  -1L
#define SEM_IMMEDIATE_RETURN	   0L

/* XLATOFF */
#ifndef INCL_NOXLATE_DOS16
#define	DosSemClear	Dos16SemClear
#define	DosSemRequest	Dos16SemRequest
#define	DosSemSet	Dos16SemSet
#define	DosSemSetWait	Dos16SemSetWait
#define	DosSemWait	Dos16SemWait
#endif /* INCL_NOXLATE_DOS16 */
/* XLATON */

APIRET	APIENTRY DosSemClear(HSEM hsem);
APIRET	APIENTRY DosSemSet(HSEM hsem);
APIRET	APIENTRY DosSemWait(HSEM hsem, LONG lTimeOut);
APIRET	APIENTRY DosSemSetWait(HSEM hsem, LONG lTimeOut);
APIRET	APIENTRY DosSemRequest(HSEM hsem, LONG lTimeOut);

#endif /* common INCL_DOSSEMAPHORES */

#ifdef INCL_DOSSEMAPHORES

typedef LHANDLE HSYSSEM;	/* hssm */
typedef HSYSSEM FAR *PHSYSSEM;

#define CSEM_PRIVATE		   0
#define CSEM_PUBLIC		   1

/* XLATOFF */
#ifndef INCL_NOXLATE_DOS16
#define	DosCloseSem	Dos16CloseSem
#define	DosCreateSem	Dos16CreateSem
#define	DosFSRamSemClear	Dos16FSRamSemClear
#define	DosFSRamSemRequest	Dos16FSRamSemRequest
#define	DosMuxSemWait	Dos16MuxSemWait
#define	DosOpenSem	Dos16OpenSem
#endif /* INCL_NOXLATE_DOS16 */
/* XLATON */

APIRET	APIENTRY DosCreateSem(USHORT fExclusive,PHSYSSEM phsem,PSZ pszSemName);
APIRET	APIENTRY DosOpenSem(PHSEM phsem, PSZ pszSemName);
APIRET	APIENTRY DosCloseSem(HSEM hsem);

typedef struct _MUXSEM {	/* mxs */
    USHORT  zero;
    HSEM    hsem;
} MUXSEM;
typedef MUXSEM FAR *PMUXSEM;

typedef struct _MUXSEMLIST {	/* mxsl */
    USHORT  cmxs;
    MUXSEM  amxs[16];
} MUXSEMLIST;
typedef MUXSEMLIST FAR *PMUXSEMLIST;

/*
 * Since a MUXSEMLIST structure is actually a variable length
 * structure, the following macro may be used to define a MUXSEMLIST
 * structure having size elements, named "name".
 */
#define DEFINEMUXSEMLIST(name, size) \
    struct {			     \
	USHORT cmxs;		     \
	MUXSEM amxs[size];	     \
    } name;

/*
 * This function actually takes a far pointer to a MUXSEMLIST structure
 * as its second parameter, but in order to allow its use with the
 * DEFINEMUXSEMLIST macro, it is declared here as PVOID.
 */
APIRET	APIENTRY DosMuxSemWait(PUSHORT pisemCleared,PVOID pmsxl,LONG lTimeOut);


/*** Fast safe ram semaphores */

typedef struct _DOSFSRSEM {	/* dosfsrs */
    USHORT  cb;
    PID	    pid;
    TID	    tid;
    USHORT  cUsage;
    USHORT  client;
    ULONG   sem;
} DOSFSRSEM;
typedef DOSFSRSEM FAR *PDOSFSRSEM;

APIRET	APIENTRY DosFSRamSemRequest(PDOSFSRSEM pdosfsrs, LONG lTimeOut);
APIRET	APIENTRY DosFSRamSemClear(PDOSFSRSEM pdosfsrs);

#endif /* INCL_DOSSEMAPHORES */

#if (defined(INCL_DOSDATETIME) || !defined(INCL_NOCOMMON))

/*** Time support */

typedef struct _DATETIME {	/* date */
    UCHAR   hours;
    UCHAR   minutes;
    UCHAR   seconds;
    UCHAR   hundredths;
    UCHAR   day;
    UCHAR   month;
    USHORT  year;
    SHORT   timezone;
    UCHAR   weekday;
} DATETIME;
typedef DATETIME FAR *PDATETIME;

/* XLATOFF */
#ifndef INCL_NOXLATE_DOS16
#define	DosGetDateTime	Dos16GetDateTime
#define	DosSetDateTime	Dos16SetDateTime
#endif /* INCL_NOXLATE_DOS16 */
/* XLATON */

APIRET	APIENTRY DosGetDateTime(PDATETIME pdatetime);
APIRET	APIENTRY DosSetDateTime(PDATETIME pdatetime);

#endif /* common INCL_DOSDATETIME */

#ifdef INCL_DOSDATETIME

typedef SHANDLE HTIMER;
typedef HTIMER FAR *PHTIMER;

/* XLATOFF */
#ifndef INCL_NOXLATE_DOS16
#define	DosTimerAsync	Dos16TimerAsync
#define	DosTimerStart	Dos16TimerStart
#define	DosTimerStop	Dos16TimerStop
#endif /* INCL_NOXLATE_DOS16 */
/* XLATON */

APIRET	APIENTRY DosTimerAsync(ULONG ulTime, HSEM hsem, PHTIMER phtimer);
APIRET	APIENTRY DosTimerStart(ULONG ulTime, HSEM hsem, PHTIMER phtimer);
APIRET	APIENTRY DosTimerStop(HTIMER htimer);

#endif /* INCL_DOSDATETIME */


/*** Module manager */

#ifdef INCL_DOSMODULEMGR

/* XLATOFF */
#ifndef INCL_NOXLATE_DOS16
#define	DosFreeModule	Dos16FreeModule
#define	DosGetModHandle	Dos16GetModHandle
#define	DosGetModName	Dos16GetModName
#define	DosGetProcAddr	Dos16GetProcAddr
#define	DosLoadModule	Dos16LoadModule
#define	DosQueryProcType	Dos16QueryProcType
#endif /* INCL_NOXLATE_DOS16 */
/* XLATON */

APIRET	APIENTRY DosLoadModule(PSZ pszFailName, USHORT cbFileName,
			      PSZ pszModName, PHMODULE phmod);
APIRET	APIENTRY DosFreeModule(HMODULE hmod);
APIRET	APIENTRY DosGetProcAddr(HMODULE hmod, PSZ pszProcName,
			       PFN FAR * ppfnProcAddr);
APIRET	APIENTRY DosGetModHandle(PSZ pszModName, PHMODULE phMod);
APIRET	APIENTRY DosGetModName(HMODULE hmod, USHORT cbBuf, PCHAR pchBuf);

#define	PT_16BIT	0L
#define	PT_32BIT	1L

APIRET	APIENTRY DosQueryProcType(HMODULE hmod, ULONG ordinal, PSZ pszName, PULONG pulprocType);

APIRET	APIENTRY DosInitializePorthole(ULONG pfmLibInit, ULONG lenttype);

typedef struct _MODHDR {    /* Module Header Info  */
USHORT	flags;
USHORT	flagsothers;
USHORT	exetype;
USHORT	expver;
USHORT	cbrsrc;
} MODHDR;
typedef MODHDR	*PMODHDR;

#define	HEADER_EXEINFO		1L
#define	HEADER_READRSRCTBL	2L
#define	HEADER_READSECTOR	3L

APIRET	APIENTRY DosQueryHeaderInfo(HMODULE hmod, ULONG i, VOID FAR *pbuf, ULONG cb, ULONG subfunc);

#endif /* INCL_DOSMODULEMGR */

#if (defined(INCL_DOSRESOURCES) || !defined(INCL_NOCOMMON))

/*** Resource support */

/* Predefined resource types */

#define RT_POINTER		   1   /* mouse pointer shape		      */
#define RT_BITMAP		   2   /* bitmap			      */
#define RT_MENU			   3   /* menu template			      */
#define RT_DIALOG		   4   /* dialog template		      */
#define RT_STRING		   5   /* string tables			      */
#define RT_FONTDIR		   6   /* font directory		      */
#define RT_FONT			   7   /* font				      */
#define RT_ACCELTABLE		   8   /* accelerator tables		      */
#define RT_RCDATA		   9   /* binary data			      */
#define RT_MESSAGE		   10  /* error mesage tables		      */
#define RT_DLGINCLUDE		   11  /* dialog include file name	      */
#define RT_VKEYTBL		   12  /* key to vkey tables		      */
#define RT_KEYTBL		   13  /* key to UGL tables		      */
#define RT_CHARTBL		   14
#define RT_DISPLAYINFO		   15  /* screen display information	      */

#define RT_FKASHORT		   16  /* function key area short form	      */
#define RT_FKALONG		   17  /* function key area long form	      */

#define RT_HELPTABLE		   18  /* Help table for Cary Help manager    */
#define RT_HELPSUBTABLE		   19  /* Help subtable for Cary Help manager */

#define RT_FDDIR		   20  /* DBCS uniq/font driver directory     */
#define RT_FD			   21  /* DBCS uniq/font driver		      */

#define RT_MAX			   22  /* 1st unused Resource Type	      */


#endif /* common INCL_DOSRESOURCES */

#ifdef INCL_DOSRESOURCES

/* XLATOFF */
#ifndef INCL_NOXLATE_DOS16
#define	DosFreeResource	Dos16FreeResource
#define	DosGetResource	Dos16GetResource
#define	DosGetResource2	Dos16GetResource2
#define	DosQueryResourceSize	Dos16QueryResourceSize
#endif /* INCL_NOXLATE_DOS16 */
/* XLATON */

APIRET	APIENTRY DosGetResource(HMODULE hmod, USHORT idType, USHORT idName,
			       PSEL psel);
APIRET	APIENTRY DosGetResource2(HMODULE hmod, USHORT idType, USHORT idName,
				PVOID FAR * ppData);
APIRET	APIENTRY DosFreeResource(PVOID pData);
APIRET	APIENTRY DosQueryResourceSize(HMODULE hmod, ULONG idt, ULONG idn,
					     PULONG pulsize);

#endif /* INCL_DOSRESOURCES */


/*** NLS Support */

#ifdef INCL_DOSNLS

typedef struct _COUNTRYCODE {	/* ctryc */
    USHORT  country;
    USHORT  codepage;
} COUNTRYCODE;
typedef COUNTRYCODE FAR *PCOUNTRYCODE;

typedef struct _COUNTRYINFO {	/* ctryi */
    USHORT  country;
    USHORT  codepage;
    USHORT  fsDateFmt;
    CHAR    szCurrency[5];
    CHAR    szThousandsSeparator[2];
    CHAR    szDecimal[2];
    CHAR    szDateSeparator[2];
    CHAR    szTimeSeparator[2];
    UCHAR   fsCurrencyFmt;
    UCHAR   cDecimalPlace;
    UCHAR   fsTimeFmt;
    USHORT  abReserved1[2];
    CHAR    szDataSeparator[2];
    USHORT  abReserved2[5];
} COUNTRYINFO;
typedef COUNTRYINFO FAR *PCOUNTRYINFO;

/* XLATOFF */
#ifndef INCL_NOXLATE_DOS16
#define	DosCaseMap	Dos16CaseMap
#define	DosGetCollate	Dos16GetCollate
#define	DosGetCp	Dos16GetCp
#define	DosGetCtryInfo	Dos16GetCtryInfo
#define	DosGetDBCSEv	Dos16GetDBCSEv
#define	DosSetCp	Dos16SetCp
#define	DosSetProcCp	Dos16SetProcCp
#endif /* INCL_NOXLATE_DOS16 */
/* XLATON */

APIRET	APIENTRY DosGetCtryInfo(USHORT cbBuf, PCOUNTRYCODE pctryc,
			       PCOUNTRYINFO pctryi, PUSHORT pcbCtryInfo);
APIRET	APIENTRY DosGetDBCSEv(USHORT cbBuf, PCOUNTRYCODE pctryc, PCHAR pchBuf);
APIRET	APIENTRY DosCaseMap(USHORT usLen, PCOUNTRYCODE pctryc, PCHAR pchStr);
APIRET	APIENTRY DosGetCollate(USHORT cbBuf, PCOUNTRYCODE pctryc, PCHAR pchBuf,
			      PUSHORT pcbTable);
APIRET	APIENTRY DosGetCp(USHORT cbBuf, PUSHORT pBuf, PUSHORT pcbCodePgLst);
APIRET	APIENTRY DosSetCp(USHORT usCodePage, USHORT usReserved);
APIRET	APIENTRY DosSetProcCp(USHORT usCodePage, USHORT usReserved);

#endif /* INCL_DOSNLS */


/*** Signal support */

#ifdef INCL_DOSSIGNALS

/* Signal Numbers for DosSetSigHandler	*/

#define SIG_CTRLC		   1	   /* Control C			 */
#define SIG_BROKENPIPE		   2	   /* Broken Pipe		 */
#define SIG_KILLPROCESS		   3	   /* Program Termination	 */
#define SIG_CTRLBREAK		   4	   /* Control Break		 */
#define SIG_PFLG_A		   5	   /* Process Flag A		 */
#define SIG_PFLG_B		   6	   /* Process Flag B		 */
#define SIG_PFLG_C		   7	   /* Process Flag C		 */
#define SIG_CSIGNALS		   8	   /* number of signals plus one */

/* Flag Numbers for DosFlagProcess */

#define PFLG_A			   0	   /* Process Flag A		 */
#define PFLG_B			   1	   /* Process Flag B		 */
#define PFLG_C			   2	   /* Process Flag C		 */

/* Signal actions */

#define SIGA_KILL		   0
#define SIGA_IGNORE		   1
#define SIGA_ACCEPT		   2
#define SIGA_ERROR		   3
#define SIGA_ACKNOWLEDGE	   4

/* DosHoldSignal constants */

#define HLDSIG_ENABLE		   0
#define HLDSIG_DISABLE		   1

/* DosFlagProcess codes */

#define FLGP_SUBTREE		   0
#define FLGP_PID		   1

typedef VOID (PASCAL FAR *PFNSIGHANDLER)(USHORT, USHORT);

/* XLATOFF */
#ifndef INCL_NOXLATE_DOS16
#define	DosFlagProcess	Dos16FlagProcess
#define	DosHoldSignal	Dos16HoldSignal
#define	DosSendSignal	Dos16SendSignal
#define	DosSetSigHandler	Dos16SetSigHandler
#endif /* INCL_NOXLATE_DOS16 */
/* XLATON */

APIRET	APIENTRY DosSetSigHandler(PFNSIGHANDLER pfnSigHandler,
				 PFNSIGHANDLER FAR * ppfnPrev, PUSHORT pfAction,
				 USHORT fAction, USHORT usSigNum);
APIRET	APIENTRY DosFlagProcess(PID pid, USHORT fScope, USHORT usFlagNum,
			       USHORT usFlagArg);
APIRET	APIENTRY DosHoldSignal(USHORT fDisable);
APIRET	APIENTRY DosSendSignal(USHORT idProcess, USHORT usSigNumber);

#endif /* INCL_DOSSIGNALS */


/*** Monitor support */

#ifdef INCL_DOSMONITORS

typedef SHANDLE HMONITOR;	/* hmon */
typedef HMONITOR FAR *PHMONITOR;

/* XLATOFF */
#ifndef INCL_NOXLATE_DOS16
#define	DosMonClose	Dos16MonClose
#define	DosMonOpen	Dos16MonOpen
#define	DosMonRead	Dos16MonRead
#define	DosMonReg	Dos16MonReg
#define	DosMonWrite	Dos16MonWrite
#endif /* INCL_NOXLATE_DOS16 */
/* XLATON */

APIRET	APIENTRY DosMonOpen(PSZ pszDevName, PHMONITOR phmon);
APIRET	APIENTRY DosMonClose(HMONITOR hmon);
APIRET	APIENTRY DosMonReg(HMONITOR hmon, PBYTE pbInBuf, PBYTE pbOutBuf,
			  USHORT fPosition, USHORT usIndex);
APIRET	APIENTRY DosMonRead(PBYTE pbInBuf, USHORT fWait, PBYTE pbDataBuf,
			   PUSHORT pcbData);
APIRET	APIENTRY DosMonWrite(PBYTE pbOutBuf, PBYTE pbDataBuf, USHORT cbData);

#endif /* INCL_DOSMONITORS */


/*** Pipe and queue support */

#ifdef INCL_DOSQUEUES
#if (defined(INCL_DOSFILEMGR) || !defined(INCL_NOCOMMON))

typedef SHANDLE HQUEUE;		/* hq */
typedef HQUEUE FAR *PHQUEUE;

/* XLATOFF */
#ifndef INCL_NOXLATE_DOS16
#define	DosCloseQueue	Dos16CloseQueue
#define	DosCreateQueue	Dos16CreateQueue
#define	DosMakePipe	Dos16MakePipe
#define	DosOpenQueue	Dos16OpenQueue
#define	DosPeekQueue	Dos16PeekQueue
#define	DosPurgeQueue	Dos16PurgeQueue
#define	DosQueryQueue	Dos16QueryQueue
#define	DosReadQueue	Dos16ReadQueue
#define	DosWriteQueue	Dos16WriteQueue
#endif /* INCL_NOXLATE_DOS16 */
/* XLATON */

APIRET	APIENTRY DosMakePipe(PHFILE phfRead, PHFILE phfWrite, USHORT cb);
APIRET	APIENTRY DosCloseQueue(HQUEUE hqueue);
APIRET	APIENTRY DosCreateQueue(PHQUEUE phqueue, USHORT fQueueOrder,
			       PSZ pszQueueName);
APIRET	APIENTRY DosOpenQueue(PUSHORT ppidOwner, PHQUEUE phqueue,
			     PSZ pszQueueName);
APIRET	APIENTRY DosPeekQueue(HQUEUE hqueue, PULONG pqresc, PUSHORT pcbElement,
			     PPVOID ppBuf, PUSHORT pElemCode, UCHAR fWait,
			     PBYTE pbElemPrty, ULONG hsem);
APIRET	APIENTRY DosPurgeQueue(HQUEUE hqueue);
APIRET	APIENTRY DosQueryQueue(HQUEUE hqueue, PUSHORT pcElem);
APIRET	APIENTRY DosReadQueue(HQUEUE hqueue, PULONG pqresc, PUSHORT pcbElem,
			     PPVOID ppBuf, USHORT usElem, UCHAR fWait,
			     PBYTE pbElemPrty, HSEM hsem);
APIRET	APIENTRY DosWriteQueue(HQUEUE hqueue, USHORT usRequest, USHORT cbBuf,
			      PVOID pBuf, UCHAR fPriority);

#else /* INCL_DOSFILEMGR || !INCL_NOCOMMON */
#error PHFILE not defined - define INCL_DOSFILEMGR or undefine INCL_NOCOMMON
#endif /* INCL_DOSFILEMGR || !INCL_NOCOMMON */
#endif /* INCL_DOSQUEUES */

#ifdef INCL_DOSMISC

/* definitions for DOSSEARCHPATH control word */
#define DSP_IMPLIEDCUR		   1 /* current dir will be searched first */
#define DSP_PATHREF		   2 /* from envirnoment variable	   */
#define DSP_IGNORENETERR	   4 /* ignore net errs & continue search  */

/* definition for DOSQSYSINFO */
#define Q_MAX_PATH_LENGTH	  (0) /* index for query max path length   */

/* definitions for DosError - combine with | */
#define	FERR_DISABLEHARDERR	0x00000000L	/* disable hard error popups */
#define	FERR_ENABLEHARDERR	0x00000001L	/* enable hard error popups */
#define	FERR_ENABLEEXCEPTION	0x00000000L	/* enable exception popups */
#define	FERR_DISABLEEXCEPTION	0x00000002L	/* disable exception popups */

/* XLATOFF */
#ifndef INCL_NOXLATE_DOS16
#define	DosDynamicTrace	Dos16DynamicTrace
#define	DosErrClass	Dos16ErrClass
#define	DosError	Dos16Error
#define	DosGetEnv	Dos16GetEnv
#define	DosGetMachineMode	Dos16GetMachineMode
#define	DosGetMessage	Dos16GetMessage
#define	DosGetVersion	Dos16GetVersion
#define	DosInsMessage	Dos16InsMessage
#define	DosPutMessage	Dos16PutMessage
#define	DosQSysInfo	Dos16QSysInfo
#define	DosQueryMessageCP	Dos16QueryMessageCP
#define	DosScanEnv	Dos16ScanEnv
#define	DosSearchPath	Dos16SearchPath
#define	DosSetVec	Dos16SetVec
#define	DosSysTrace	Dos16SysTrace
#endif /* INCL_NOXLATE_DOS16 */
/* XLATON */

APIRET	APIENTRY DosError(USHORT fEnable);
APIRET	APIENTRY DosSetVec(USHORT usVecNum, PFN pfnFun, PFN FAR *ppfnPrev);
APIRET	APIENTRY DosGetMessage(PCHAR FAR * ppchVTable, USHORT usVCount,
			      PCHAR pchBuf, USHORT cbBuf, USHORT usMsgNum,
			      PSZ pszFileName, PUSHORT pcbMsg);
APIRET	APIENTRY DosErrClass(USHORT usErrCode, PUSHORT pusClass,
			    PUSHORT pfsAction, PUSHORT pusLocus);
APIRET	APIENTRY DosInsMessage(PCHAR FAR * ppchVTable, USHORT usVCount,
			      PSZ pszMsg, USHORT cbMsg, PCHAR pchBuf,
			      USHORT cbBuf, PUSHORT pcbMsg);
APIRET	APIENTRY DosPutMessage(USHORT hf, USHORT cbMsg, PCHAR pchMsg);
APIRET	APIENTRY DosQueryMessageCP(PCHAR pb, USHORT cb, PSZ pszFilename,
			      PUSHORT cbBuf);
APIRET	APIENTRY DosSysTrace(USHORT, USHORT, USHORT, PCHAR);
APIRET	APIENTRY DosDynamicTrace(USHORT, PBYTE, PBYTE);
APIRET	APIENTRY DosQSysInfo(USHORT index, PVOID pBuf, USHORT cbBuf);
APIRET	APIENTRY DosGetEnv(PSEL pselEnv, PUSHORT pOffsetCmd);
APIRET	APIENTRY DosScanEnv(PSZ pszVarName, PSZ	FAR * ppszResult);
APIRET	APIENTRY DosSearchPath(USHORT fsSearch, PSZ pszPath, PSZ pszFName,
			      PBYTE pBuf, USHORT cbBuf);
APIRET	APIENTRY DosGetVersion(PUSHORT pVer);
APIRET	APIENTRY DosGetMachineMode(PBYTE pMachMode);

#endif /* INCL_DOSMISC */


/*** Session manager support */

#ifdef INCL_DOSSESMGR

typedef struct _STARTDATA {	/* stdata */
    USHORT  Length;
    USHORT  Related;
    USHORT  FgBg;
    USHORT  TraceOpt;
    PSZ	    PgmTitle;
    PSZ	    PgmName;
    PBYTE   PgmInputs;
    PBYTE   TermQ;
    PBYTE   Environment;
    USHORT  InheritOpt;
    USHORT  SessionType;
    PSZ	    IconFile;
    ULONG   PgmHandle;
    USHORT  PgmControl;
    USHORT  InitXPos;
    USHORT  InitYPos;
    USHORT  InitXSize;
    USHORT  InitYSize;
    USHORT  Reserved;
    PSZ     ObjectBuffer;
    ULONG   ObjectBuffLen;
} STARTDATA;
typedef STARTDATA FAR *PSTARTDATA;

#define	SSF_RELATED_INDEPENDENT	0
#define	SSF_RELATED_CHILD	1

#define	SSF_FGBG_FORE		0
#define	SSF_FGBG_BACK		1

#define	SSF_TRACEOPT_NONE	0
#define	SSF_TRACEOPT_TRACE	1
#define	SSF_TRACEOPT_TRACEALL	2

#define	SSF_INHERTOPT_SHELL	0
#define	SSF_INHERTOPT_PARENT	1

/* note that these types are identical to those in pmshl.h for PROG_* */
#define	SSF_TYPE_DEFAULT	0
#define	SSF_TYPE_FULLSCREEN	1
#define	SSF_TYPE_WINDOWABLEVIO	2
#define	SSF_TYPE_PM		3
#define	SSF_TYPE_VDM		4
#define	SSF_TYPE_GROUP		5
#define	SSF_TYPE_DLL		6
#define	SSF_TYPE_WINDOWEDVDM	7
#define	SSF_TYPE_PDD		8
#define	SSF_TYPE_VDD		9

/* note that these flags are identical to those in pmshl.h for SHE_* */
#define	SSF_CONTROL_VISIBLE	0x0000
#define	SSF_CONTROL_INVISIBLE	0x0001
#define	SSF_CONTROL_MAXIMIZE	0x0002
#define	SSF_CONTROL_MINIMIZE	0x0004
#define	SSF_CONTROL_NOAUTOCLOSE	0x0008
#define	SSF_CONTROL_SETPOS	0x8000

typedef struct _STATUSDATA {	/* stsdata */
    USHORT  Length;
    USHORT  SelectInd;
    USHORT  BondInd;
} STATUSDATA;
typedef STATUSDATA FAR *PSTATUSDATA;

typedef	struct _REGISTERDATA {	/* regdata */
    USHORT	Length;
    USHORT	NotifType;
    PSZ		DDName;
} REGISTERDATA;
typedef REGISTERDATA FAR *PREGISTERDATA;

/* XLATOFF */
#ifndef INCL_NOXLATE_DOS16
#define	DosQAppType	Dos16QAppType
#define	DosSMRegisterDD	Dos16SMRegisterDD
#define	DosSelectSession	Dos16SelectSession
#define	DosSetSession	Dos16SetSession
#define	DosStartSession	Dos16StartSession
#define	DosStopSession	Dos16StopSession
#endif /* INCL_NOXLATE_DOS16 */
/* XLATON */

APIRET	APIENTRY	DosSMRegisterDD(PREGISTERDATA);
APIRET	APIENTRY DosStartSession(PSTARTDATA pstdata, PUSHORT pidSession,
				PUSHORT ppid);
APIRET	APIENTRY DosSetSession(USHORT idSession, PSTATUSDATA pstsdata);
APIRET	APIENTRY DosSelectSession(USHORT idSession, ULONG ulReserved);
APIRET	APIENTRY DosStopSession(USHORT fScope, USHORT idSession,
			       ULONG ulReserved);
APIRET	APIENTRY DosQAppType(PSZ pszPrgName, PUSHORT pusType);

#endif /* INCL_DOSSESMGR */

#if (defined(INCL_DOSSESMGR) || defined(INCL_DOSFILEMGR))

/* AppType returned in by DosQueryAppType in pFlags as follows		*/
#define	FAPPTYP_NOTSPEC		0x0000
#define	FAPPTYP_NOTWINDOWCOMPAT	0x0001
#define	FAPPTYP_WINDOWCOMPAT	0x0002
#define	FAPPTYP_WINDOWAPI	0x0003
#define	FAPPTYP_BOUND		0x0008
#define	FAPPTYP_DLL		0x0010
#define	FAPPTYP_DOS		0x0020
#define	FAPPTYP_PHYSDRV		0x0040	/* physical device driver	*/
#define	FAPPTYP_VIRTDRV		0x0080	/* virtual device driver	*/
#define	FAPPTYP_PROTDLL		0x0100	/* 'protected memory' dll	*/
#define	FAPPTYP_32BIT		0x4000
#define	FAPPTYP_EXETYPE		FAPPTYP_WINDOWAPI

#define FAPPTYP_RESERVED	~(FAPPTYP_WINDOWAPI | FAPPTYP_BOUND | FAPPTYP_DLL | FAPPTYP_DOS | FAPPTYP_PHYSDRV | FAPPTYP_VIRTDRV | FAPPTYP_PROTDLL | FAPPTYP_32BIT)

#ifdef INCL_DOSFILEMGR

#define EAT_APPTYP_PMAPI	0x00		/* Uses PM API */
#define EAT_APPTYP_DOS		0x01		/* DOS APP */
#define EAT_APPTYP_PMW		0x02		/* Window compatible */
#define EAT_APPTYP_NOPMW	0x03		/* Not Window compatible */
#define EAT_APPTYP_EXETYPE	0x03		/* EXE type mask */
#define EAT_APPTYP_RESERVED	~(EAT_APPTYP_EXETYPE)

#endif /* INCL_DOSFILEMGR */

#endif /* INCL_DOSSESMGR || INCL_DOSFILEMGR */

/*** Device support */

#ifdef INCL_DOSDEVICES

/* XLATOFF */
#ifndef INCL_NOXLATE_DOS16
#define	DosCLIAccess	Dos16CLIAccess
#define	DosDevConfig	Dos16DevConfig
#define	DosDevIOCtl	Dos16DevIOCtl
#define	DosDevIOCtl2	Dos16DevIOCtl2
#define	DosPhysicalDisk	Dos16PhysicalDisk
#define	DosPortAccess	Dos16PortAccess
#define	DosR2StackRealloc	Dos16R2StackRealloc
#define	DosSystemService	Dos16SystemService
#define	DosCallback	Dos16Callback
#endif /* INCL_NOXLATE_DOS16 */
/* XLATON */


APIRET	APIENTRY DosDevConfig(PVOID pDevInfo, USHORT usItem, USHORT usReserved);
#define	DEVINFO_PRINTER		0	/* Number of printers attached */
#define	DEVINFO_RS232		1	/* Number of RS232 ports */
#define	DEVINFO_FLOPPY		2	/* Number of diskette drives */
#define	DEVINFO_COPROCESSOR	3	/* Presence of math coprocessor */
#define	DEVINFO_SUBMODEL	4	/* PC Submodel Type */
#define	DEVINFO_MODEL		5	/* PC Model Type */
#define	DEVINFO_ADAPTER		6	/* Primary display adapter type */
#define	DEVINFO_COPROCESSORTYPE	7	/* Type of coprocessor functionality */

APIRET	APIENTRY DosDevIOCtl(PVOID pData, PVOID pParms, USHORT usFun,
			    USHORT usCategory, HFILE hDev);
APIRET	APIENTRY DosDevIOCtl2(PVOID pData, USHORT cbData, PVOID pParm,
			     USHORT cbParm, USHORT usFun, USHORT usCategory,
			     HFILE hDev);
APIRET	APIENTRY DosSystemService(USHORT usCategory, PVOID pRequest,
				 PVOID pResponse);

APIRET	APIENTRY DosCLIAccess(VOID);
APIRET	APIENTRY DosPortAccess(USHORT usReserved, USHORT fRelease,
			      USHORT usFirstPort, USHORT usLastPort);
APIRET	APIENTRY DosPhysicalDisk(USHORT usFun, PVOID pOut, USHORT cbOut,
				PVOID pParm, USHORT cbParm);
#define	INFO_COUNT_PARTITIONABLE_DISKS	1	/* # of partitionable disks */
#define	INFO_GETIOCTLHANDLE		2	/* Obtain handle	    */
#define	INFO_FREEIOCTLHANDLE		3	/* Release handle	    */

APIRET	APIENTRY DosR2StackRealloc(USHORT cbStack);
VOID	APIENTRY DosCallback(PFN pfn);

#endif /* INCL_DOSDEVICES */


/*** DosNamedPipes API Support */

#ifdef INCL_DOSNMPIPES

typedef SHANDLE HPIPE;		/* hp */
typedef HPIPE FAR *PHPIPE;

typedef struct _AVAILDATA   {	    /* AVAILDATA  */
    USHORT	cbpipe;		    /* bytes left in the pipe		  */
    USHORT	cbmessage;	    /* bytes left in current message	  */
} AVAILDATA;
typedef AVAILDATA FAR *PAVAILDATA;

typedef struct _PIPEINFO {		/* nmpinf */
    USHORT cbOut;			/* length of outgoing I/O buffer */
    USHORT cbIn;			/* length of incoming I/O buffer */
    BYTE   cbMaxInst;			/* maximum number of instances	 */
    BYTE   cbCurInst;			/* current number of instances	 */
    BYTE   cbName;			/* length of pipe name		 */
    CHAR   szName[1];			/* start of name		 */
} PIPEINFO;
typedef PIPEINFO FAR *PPIPEINFO;

typedef struct _PIPESEMSTATE {	/* nmpsmst */
    BYTE   fStatus;		/* type of record, 0 = EOI, 1 = read ok, */
				/* 2 = write ok, 3 = pipe closed	 */
    BYTE   fFlag;		/* additional info, 01 = waiting thread	 */
    USHORT usKey;		/* user's key value                      */
    USHORT usAvail;		/* available data/space if status = 1/2	 */
} PIPESEMSTATE;
typedef PIPESEMSTATE FAR *PPIPESEMSTATE;

#define	NP_INDEFINITE_WAIT	-1
#define	NP_DEFAULT_WAIT		0L

/* DosPeekNmPipe() pipe states */

#define	NP_STATE_DISCONNECTED	0x0001
#define	NP_STATE_LISTENING	0x0002
#define	NP_STATE_CONNECTED	0x0003
#define	NP_STATE_CLOSING	0x0004

/* DosCreateNPipe open modes */

#define NP_ACCESS_INBOUND       0x0000
#define NP_ACCESS_OUTBOUND      0x0001
#define NP_ACCESS_DUPLEX        0x0002
#define NP_INHERIT              0x0000
#define NP_NOINHERIT            0x0080
#define NP_WRITEBEHIND          0x0000
#define NP_NOWRITEBEHIND        0x4000

/* DosCreateNPipe and DosQueryNPHState state */

#define NP_READMODE_BYTE        0x0000
#define NP_READMODE_MESSAGE     0x0100
#define NP_TYPE_BYTE            0x0000
#define NP_TYPE_MESSAGE         0x0400
#define NP_END_CLIENT           0x0000
#define NP_END_SERVER           0x4000
#define NP_WAIT                 0x0000
#define NP_NOWAIT               0x8000
#define NP_UNLIMITED_INSTANCES  0x00FF


/* XLATOFF */
#ifndef INCL_NOXLATE_DOS16
#define	DosCallNmPipe	Dos16CallNmPipe
#define	DosConnectNmPipe	Dos16ConnectNmPipe
#define	DosDisConnectNmPipe	Dos16DisConnectNmPipe
#define	DosMakeNmPipe	Dos16MakeNmPipe
#define	DosPeekNmPipe	Dos16PeekNmPipe
#define	DosQNmPHandState	Dos16QNmPHandState
#define	DosQNmPipeInfo	Dos16QNmPipeInfo
#define	DosQNmPipeSemState	Dos16QNmPipeSemState
#define	DosSetNmPHandState	Dos16SetNmPHandState
#define	DosSetNmPipeSem	Dos16SetNmPipeSem
#define	DosTransactNmPipe	Dos16TransactNmPipe
#define	DosWaitNmPipe	Dos16WaitNmPipe
#endif /* INCL_NOXLATE_DOS16 */
/* XLATON */

APIRET	APIENTRY DosCallNmPipe(PSZ pszName, PVOID pInBuf, USHORT cbInBuf,
			      PVOID pbOutBuf, USHORT cbOutBuf, PUSHORT pcbRead,
			      ULONG ulTimeOut);
APIRET	APIENTRY DosConnectNmPipe(HPIPE hp);
APIRET	APIENTRY DosDisConnectNmPipe(HPIPE hp);
APIRET	APIENTRY DosMakeNmPipe(PSZ pszName, PHPIPE php, USHORT fsOpenMode,
			      USHORT fsPipeMode, USHORT cbOutBuf,
			      USHORT cbInBuf, ULONG ulTimeOut);
APIRET	APIENTRY DosPeekNmPipe(HPIPE hp, PVOID pBuf, USHORT cbBuf,
			      PUSHORT pcbRead, PAVAILDATA pcbAvail,
			      PUSHORT pfsState);
APIRET	APIENTRY DosQNmPHandState(HPIPE hp, PUSHORT pfsState);
APIRET	APIENTRY DosQNmPipeInfo(HPIPE hp, USHORT usInfoLevel, PVOID pBuf,
			       USHORT cb);
APIRET	APIENTRY DosQNmPipeSemState(HSEM hsem, PPIPESEMSTATE pnpss, USHORT cb);
APIRET	APIENTRY DosSetNmPHandState(HPIPE hp, USHORT fsState);
APIRET	APIENTRY DosSetNmPipeSem(HPIPE hp, HSEM hsem, USHORT usKeyVal);
APIRET	APIENTRY DosTransactNmPipe(HPIPE hp, PVOID bOutBuf, USHORT cbOut,
				  PVOID pInBuf, USHORT cbIn, PUSHORT pcbRead);
APIRET	APIENTRY DosWaitNmPipe(PSZ pszName, ULONG ulTimeOut);

/* values in npss_status */
#define NPSS_EOI		   0	 /* End Of Information	  */
#define NPSS_RDATA		   1	 /* read data available	  */
#define NPSS_WSPACE		   2	 /* write space available */
#define NPSS_CLOSE		   3	 /* pipe in CLOSING state */

/* values in npss_flag */
#define NPSS_WAIT		   0x01	 /* waiting thread on end of pipe */

/* defined bits in pipe mode */
#define NP_NBLK			   0x8000 /* non-blocking read/write */
#define NP_SERVER		   0x4000 /* set if server end	     */
#define NP_WMESG		   0x0400 /* write messages	     */
#define NP_RMESG		   0x0100 /* read as messages	     */
#define NP_ICOUNT		   0x00FF /* instance count field    */


/*	Named pipes may be in one of several states depending on the actions
 *	that have been taken on it by the server end and client end.  The
 *	following state/action table summarizes the valid state transitions:
 *
 *	Current state		Action			Next state
 *
 *	 <none>		    server DosMakeNmPipe	DISCONNECTED
 *	 DISCONNECTED	    server connect		LISTENING
 *	 LISTENING	    client open			CONNECTED
 *	 CONNECTED	    server disconn		DISCONNECTED
 *	 CONNECTED	    client close		CLOSING
 *	 CLOSING	    server disconn		DISCONNECTED
 *	 CONNECTED	    server close		CLOSING
 *	 <any other>	    server close		<pipe deallocated>
 *
 *	If a server disconnects his end of the pipe, the client end will enter a
 *	special state in which any future operations (except close) on the file
 *	descriptor associated with the pipe will return an error.
 */

/*
 *	Values for named pipe state
 */

#define NP_DISCONNECTED		   1	/* after pipe creation or Disconnect */
#define NP_LISTENING		   2	/* after DosNmPipeConnect	     */
#define NP_CONNECTED		   3	/* after Client open		     */
#define NP_CLOSING		   4	/* after Client or Server close	     */


#endif /* INCL_DOSNMPIPES */

/*** DosProfile API support */

#ifdef INCL_DOSPROFILE

/* DosProfile ordinal number */

#define PROF_ORDINAL		   133

/* DosProfile usType */

#define PROF_SYSTEM		   0
#define PROF_USER		   1
#define PROF_USEDD		   2
#define PROF_KERNEL		   4
#define PROF_VERBOSE		   8
#define PROF_ENABLE		   16

/* DosProfile usFunc */

#define PROF_ALLOC		   0
#define PROF_CLEAR		   1
#define PROF_ON			   2
#define PROF_OFF		   3
#define PROF_DUMP		   4
#define PROF_FREE		   5

/* DosProfile tic count granularity (DWORD) */

#define PROF_SHIFT		   2

/* DosProfile module name string length	    */

#define PROF_MOD_NAME_SIZE	   10

/* DosProfile error code for end of data    */

#define PROF_END_OF_DATA	   13

#endif /* INCL_DOSPROFILE */


/*** Virtual DOS Machine API support */

#ifdef INCL_DOSMVDM

/* XLATOFF */
#ifndef INCL_NOXLATE_DOS16
#define DosOpenVDD		Dos16OpenVDD
#define DosRequestVDD		Dos16RequestVDD
#define DosCloseVDD		Dos16CloseVDD
#define	DosQueryDOSProperty	Dos16QueryDOSProperty
#define	DosSetDOSProperty	Dos16SetDOSProperty
#endif /* INCL_NOXLATE_DOS16 */
/* XLATON */

typedef LHANDLE   HVDD;     /* hvdd */
typedef HVDD FAR *PHVDD;    /* phvdd */

APIRET	APIENTRY DosOpenVDD(PSZ pszvdd, PHVDD phvdd);

APIRET	APIENTRY DosRequestVDD(HVDD hvdd, SGID sgid, USHORT cmd,
			       ULONG cbInput, PVOID pInput,
			       ULONG cbOutput, PVOID pOutput);

APIRET	APIENTRY DosCloseVDD(HVDD hVDD);

APIRET	APIENTRY DosQueryDOSProperty(SGID sgid, PSZ pszName,
				     USHORT cb, PSZ pch);

APIRET	APIENTRY DosSetDOSProperty(SGID sgid, PSZ pszName,
				   USHORT cb, PSZ pch);

#endif /* INCL_DOSMVDM */
