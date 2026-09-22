/*static char *SCCSID = "@(#)bsedos.h	13.61 90/05/05";*/
/***************************************************************************\
*
* Module Name: BSEDOS.H
*
* OS/2 Base Include File
*
* Copyright (c) 1987  Microsoft Corporation
* Copyright (c) 1987  IBM Corporation
*
*****************************************************************************
*
* Subcomponents marked with "+" are partially included by default
*
*   #define:		    To include:
*
* + INCL_DOSPROCESS	    Process and thread support
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

/* XLATOFF */
#if defined(INCL_16)
#pragma message ("32-bit Base API included when using 16-bit compiler")
#endif /* INCL_32 */
/* XLATON */
#endif /* INCL_BASEINCLUDED */

#ifdef INCL_32	/* This #ifdef brackets the rest of this entire */
			/* file.  It is used to include either the	*/
			/* 32-bit or 16-bit base definitions.  No	*/
			/* definitions (other than 16/32-bit		*/
			/* determination) must be placed above this.	*/

#ifdef INCL_DOS

#define INCL_DOSPROCESS
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

/*** General services */

APIRET	APIENTRY	DosBeep(ULONG freq, ULONG dur);

/*** Process and Thread support */

VOID	APIENTRY	DosExit(ULONG action, ULONG result);

/* DosExit codes */

#define EXIT_THREAD	    0
#define EXIT_PROCESS	    1

#endif /* common INCL_DOSPROCESS stuff */

#ifdef INCL_DOSPROCESS

/* XLATOFF */
#define DosCwait	DosWaitChild
#define DosSetPrty	DosSetPriority
/* XLATON */

#include <bsetib.h>

typedef VOID (*PFNTHREAD)(VOID);

APIRET	APIENTRY	DosCreateThread(PTID ptid, PFNTHREAD pfn, ULONG param, ULONG flag, ULONG cbStack);

APIRET	APIENTRY	DosResumeThread(TID tid);

APIRET	APIENTRY	DosSuspendThread(TID tid);

APIRET	APIENTRY	DosGetThreadInfo(PTIB *pptib,PPIB *pppib);

/* Action code values */

#define DCWA_PROCESS	    0
#define DCWA_PROCESSTREE    1

/* Wait option values */

#define DCWW_WAIT   0
#define DCWW_NOWAIT 1

typedef struct _RESULTCODES {	  /* resc */
    ULONG codeTerminate;
    ULONG codeResult;
} RESULTCODES;
typedef RESULTCODES	*PRESULTCODES;

APIRET	APIENTRY	DosWaitChild(ULONG action, ULONG option, PRESULTCODES pres, PPID ppid, PID pid);

APIRET	APIENTRY	DosWaitThread(PPID ppid, ULONG option);

APIRET	APIENTRY	DosSleep(ULONG msec);

APIRET	APIENTRY	DosDebug(PVOID pdbgbuf);


/* codeTerminate values (also passed to ExitList routines) */

#define TC_EXIT		 0
#define TC_HARDERROR	 1
#define TC_TRAP		 2
#define TC_KILLPROCESS	 3

typedef VOID	(*PFNEXITLIST)(ULONG);

APIRET	APIENTRY	DosEnterCritSec(VOID);

APIRET	APIENTRY	DosExitCritSec(VOID);

APIRET	APIENTRY	DosExitList(ULONG ordercode, PFNEXITLIST pfn);

/* DosExitList functions */

#define EXLST_ADD	1
#define EXLST_REMOVE	2
#define EXLST_EXIT	3

APIRET	APIENTRY	DosExecPgm(PCHAR pObjname, LONG cbObjname, ULONG execFlag, PSZ pArg, PSZ pEnv, PRESULTCODES pRes, PSZ pName);

/* DosExecPgm functions */

#define EXEC_SYNC	    0
#define EXEC_ASYNC	    1
#define EXEC_ASYNCRESULT    2
#define EXEC_TRACE	    3
#define EXEC_BACKGROUND	    4
#define EXEC_LOAD	    5


APIRET	APIENTRY	DosSetPriority(ULONG scope, ULONG class, LONG delta, ULONG PorTid);

/* Priority scopes */

#define PRTYS_PROCESS	    0
#define PRTYS_PROCESSTREE   1
#define PRTYS_THREAD	    2

/* Priority classes */

#define PRTYC_NOCHANGE	    0
#define PRTYC_IDLETIME	    1
#define PRTYC_REGULAR	    2
#define PRTYC_TIMECRITICAL  3
#define PRTYC_FOREGROUNDSERVER	4

/* Priority deltas */

#define PRTYD_MINIMUM	   -31
#define PRTYD_MAXIMUM	    31

APIRET	APIENTRY	DosKillProcess(ULONG action, PID pid);

#define DKP_PROCESSTREE	    0
#define DKP_PROCESS	    1

#endif /* INCL_DOSPROCESS */

#ifndef INCL_SAADEFS

/*
 * CCHMAXPATH is the maximum fully qualified path name length including
 * the drive letter, colon, backslashes and terminating NULL.
 */
#define CCHMAXPATH	260

/*
 * CCHMAXPATHCOMP is the maximum individual path component name length
 * including a terminating NULL.
 */
#define CCHMAXPATHCOMP	256

#endif	/* !INCL_SAADEFS */

#if (defined(INCL_DOSFILEMGR) || !defined(INCL_NOCOMMON))

/*** File manager */

/* DosSetFilePtr() file position codes */

#define FILE_BEGIN	0x0000	/* Move relative to beginning of file */
#define FILE_CURRENT	0x0001	/* Move relative to current fptr position */
#define FILE_END	0x0002	/* Move relative to end of file */

/* DosFindFirst/Next Directory handle types */

#define HDIR_SYSTEM	1	/* Use system handle (1) */
#define HDIR_CREATE	(-1)	/* Allocate a new, unused handle */

/* DosCopy control bits; may be or'ed together */
#define DCPY_EXISTING	0x00001		/* Copy even if target exists */
#define DCPY_APPEND	0x00002		/* Append to existing file, do not replace */
#define DCPY_FAILEAS	0x00004 	/* Fail if EAs not supported on target*/

/* DosOpen/DosQFHandState/DosQueryFileInfo et al file attributes; also */
/* known as Dso File Mode bits... */
#define FILE_NORMAL	0x0000
#define FILE_READONLY	0x0001
#define FILE_HIDDEN	0x0002
#define FILE_SYSTEM	0x0004
#define FILE_DIRECTORY	0x0010
#define FILE_ARCHIVED	0x0020

/* DosOpen() actions */

#define FILE_EXISTED	0x0001
#define FILE_CREATED	0x0002
#define FILE_TRUNCATED	0x0003

/* DosOpen() open flags */
#define FILE_OPEN		   0x0001
#define FILE_TRUNCATE		   0x0002
#define FILE_CREATE		   0x0010

/*     this nibble applies if file already exists		 xxxx */

#define OPEN_ACTION_FAIL_IF_EXISTS     0x0000  /* ---- ---- ---- 0000 */
#define OPEN_ACTION_OPEN_IF_EXISTS     0x0001  /* ---- ---- ---- 0001 */
#define OPEN_ACTION_REPLACE_IF_EXISTS  0x0002  /* ---- ---- ---- 0010 */

/*     this nibble applies if file does not exist	    xxxx      */

#define OPEN_ACTION_FAIL_IF_NEW	       0x0000  /* ---- ---- 0000 ---- */
#define OPEN_ACTION_CREATE_IF_NEW      0x0010  /* ---- ---- 0001 ---- */

/* DosOpen/DosSetFHandState flags */

#define OPEN_ACCESS_READONLY	    0x0000  /* ---- ---- ---- -000 */
#define OPEN_ACCESS_WRITEONLY	    0x0001  /* ---- ---- ---- -001 */
#define OPEN_ACCESS_READWRITE	    0x0002  /* ---- ---- ---- -010 */
#define OPEN_SHARE_DENYREADWRITE    0x0010  /* ---- ---- -001 ---- */
#define OPEN_SHARE_DENYWRITE	    0x0020  /* ---- ---- -010 ---- */
#define OPEN_SHARE_DENYREAD	    0x0030  /* ---- ---- -011 ---- */
#define OPEN_SHARE_DENYNONE	    0x0040  /* ---- ---- -100 ---- */
#define OPEN_FLAGS_NOINHERIT	    0x0080  /* ---- ---- 1--- ---- */
#define OPEN_FLAGS_NO_LOCALITY	    0x0000  /* ---- -000 ---- ---- */
#define OPEN_FLAGS_SEQUENTIAL	    0x0100  /* ---- -001 ---- ---- */
#define OPEN_FLAGS_RANDOM	    0x0200  /* ---- -010 ---- ---- */
#define OPEN_FLAGS_RANDOMSEQUENTIAL 0x0300  /* ---- -011 ---- ---- */
#define OPEN_FLAGS_NO_CACHE	    0x1000  /* ---1 ---- ---- ---- */
#define OPEN_FLAGS_FAIL_ON_ERROR    0x2000  /* --1- ---- ---- ---- */
#define OPEN_FLAGS_WRITE_THROUGH    0x4000  /* -1-- ---- ---- ---- */
#define OPEN_FLAGS_DASD		    0x8000  /* 1--- ---- ---- ---- */


/* DosSearchPath() constants */

#define SEARCH_PATH	       0x0000
#define SEARCH_CUR_DIRECTORY   0x0001
#define SEARCH_ENVIRONMENT     0x0002
#define SEARCH_IGNORENETERRS   0x0004

/************************************************************
DosFileIO
=========================================
************************************************************/
/* File IO command words */
#define FIO_LOCK	0	/* Lock Files */
#define FIO_UNLOCK	1	/* Unlock Files */
#define FIO_SEEK	2	/* Seek (set file ptr) */
#define FIO_READ	3	/* File Read */
#define FIO_WRITE	4	/* File Write */

/* Lock Sharing Modes */
#define FIO_NOSHARE	0	/* None */
#define FIO_SHAREREAD	1	/* Read-Only */

typedef struct	_FIOLOCKCMD {		/* FLC File Lock Cmd prefix */
    USHORT	usCmd;		/* Cmd = FIO_LOCK */
    ULONG	cTimeOut;	/* in Msec */
    USHORT	cLockCnt;	/* Lock records that follow */
} FIOLOCKCMD;
typedef FIOLOCKCMD FAR *PFIOLOCKCMD;


typedef struct	_FIOLOCKREC {		/* FLR File Lock Record */
    USHORT	fShare;			/* FIO_NOSHARE or FIO_SHAREREAD */
    ULONG	cbStart;		/* Starting offset for lock region */
    ULONG	cbLength;		/* Length of lock region */
} FIOLOCKREC;
typedef FIOLOCKREC FAR *PIOLOCKREC;


typedef struct	_FIOUNLOCKCMD {		/* FUC File Unlock Cmd prefix */
    USHORT	usCmd;			/* Cmd = FIO_UNLOCK */
    USHORT	cUnlockCnt;		/* Unlock records that follow */
} FIOUNLOCKCMD;
typedef FIOUNLOCKCMD FAR *PFIOUNLOCKCMD;


typedef struct	_FIOUNLOCKREC {		/* FUR File Unlock Record */
    ULONG	cbStart;		/* Starting offset for unlock region */
    ULONG	cbLength;		/* Length of unlock region */
} FIOUNLOCKREC;
typedef FIOUNLOCKREC FAR *PFIOUNLOCKREC;


typedef struct	_FIOSEEKCMD {		/* FSC Seek command structure */
    USHORT	usCmd;			/* Cmd = FIO_SEEK */
    USHORT	fsMethod;		/* One of&gml FPM_BEGINNING,
					   FPM_CURRENT, or FPM_END */
    ULONG	cbDistance;		/* Byte offset for seek */
    ULONG	cbNewPosition;		/* Bytes from start of file after
					   seek */
} FIOSEEKCMD;
typedef FIOSEEKCMD FAR *PFIOSEEKCMD;


typedef struct	_FIOREADWRITE {		/* RWC Read & Write command structure */
    USHORT	usCmd;			/* Cmd = FIO_READ or FIO_WRITE */
    PVOID	pbBuffer;		/* Pointer to data buffer */
    ULONG	cbBufferLen;		/* Bytes in buffer or max size */
    ULONG	cbActualLen;		/* Bytes actually read/written */
} FIOREADWRITE;
typedef FIOREADWRITE FAR *PFIOREADWRITE;


/************************************************************
EA Info Levels & Find First/Next
=========================================
API's: DosFindFirst, DosQueryFileInfo, DosQueryPathInfo, DosSetFileInfo,
       DosSetPathInfo
************************************************************/

/* File info levels&gml All listed API's */
#define FIL_STANDARD		1	/* Info level 1, standard file info */
#define FIL_QUERYEASIZE		2	/* Level 2, return Full EA size */
#define FIL_QUERYEASFROMLIST	3	/* Level 3, return requested EA's */
#define FIL_QUERYALLEAS		4	/* Level 4, return all EA's */

/* File info levels: Dos...PathInfo only */
#define FIL_QUERYFULLNAME	5	/* Level 5, return fully qualified
					   name of file */
#define FIL_NAMEISVALID		6	/* Level 6, check validity of file/path
					   name for this FSD */


/* DosFsAttach() */
/* Attact or detach */
#define FS_ATTACH	0	/* Attach file server */
#define FS_DETACH	1	/* Detach file server */


/* DosFsCtl() */
/* Routing type */
#define FSCTL_HANDLE	1	/* File Handle directs req routing */
#define FSCTL_PATHNAME	2	/* Path Name directs req routing */
#define FSCTL_FSDNAME	3	/* FSD Name directs req routing */


/* DosQueryFSAttach() */
/* Information level types (defines method of query) */
#define FSAIL_QUERYNAME 1	/* Return data for a Drive or Device */
#define FSAIL_DEVNUMBER 2	/* Return data for Ordinal Device # */
#define FSAIL_DRVNUMBER 3	/* Return data for Ordinal Drive # */

/* Item types (from data structure item "iType") */
#define FSAT_CHARDEV	1	/* Resident character device */
#define FSAT_PSEUDODEV	2	/* Pusedu-character device */
#define FSAT_LOCALDRV	3	/* Local drive */
#define FSAT_REMOTEDRV	4	/* Remote drive attached to FSD */

typedef struct	_FSQBUFFER {	/* fsqbuf Data structure for QFSAttach*/
    USHORT	iType;		/* Item type */
    USHORT	cbName;		/* Length of item name, sans NULL */
    UCHAR	szName[1];	/* ASCIIZ item name */
    USHORT	cbFSDName;	/* Length of FSD name, sans NULL */
    UCHAR	szFSDName[1];	/* ASCIIZ FSD name */
    USHORT	cbFSAData;	/* Length of FSD Attach data returned */
    UCHAR	rgFSAData[1];	/* FSD Attach data from FSD */
} FSQBUFFER;
typedef FSQBUFFER FAR *PFSQBUFFER;


/***********
File System Drive Information&gml DosQueryFSInfo DosSetFSInfo
***********/

/* FS Drive Info Levels */
#define FSIL_ALLOC	1	/* Drive allocation info (Query only) */
#define FSIL_VOLSER	2	/* Drive Volum/Serial info */

/* DosQueryFHType() */
/* Handle classes (low 8 bits of Handle Type) */
#define FHT_DISKFILE	0x0000		/* Disk file handle */
#define FHT_CHRDEV	0x0001		/* Character device handle */
#define FHT_PIPE	0x0002		/* Pipe handle */

/* Handle bits (high 8 bits of Handle Type) */
#define FHB_DSKREMOTE		0x8000	/* Remote disk */
#define FHB_CHRDEVREMOTE	0x8000	/* Remote character device */
#define FHB_PIPEREMOTE		0x8000	/* Remote pipe */

#ifndef INCL_SAADEFS

/* File time and date types */

typedef struct _FTIME {		/* ftime */
    USHORT twosecs : 5;
    USHORT minutes : 6;
    USHORT hours   : 5;
} FTIME;
typedef FTIME	*PFTIME;

typedef struct _FDATE {		/* fdate */
    USHORT day	   : 5;
    USHORT month   : 4;
    USHORT year	   : 7;
} FDATE;
typedef FDATE	*PFDATE;

#endif /* INCL_SAADEFS */

typedef struct _VOLUMELABEL {	 /* vol */
    BYTE cch;
    CHAR szVolLabel[12];
} VOLUMELABEL;
typedef VOLUMELABEL FAR *PVOLUMELABEL;

typedef struct _FSINFO {    /* fsinf */
    FDATE fdateCreation;
    FTIME ftimeCreation;
    VOLUMELABEL vol;
} FSINFO;
typedef FSINFO FAR *PFSINFO;

/* HANDTYPE values */

#define HANDTYPE_FILE	  0x0000
#define HANDTYPE_DEVICE	  0x0001
#define HANDTYPE_PIPE	  0x0002
#define HANDTYPE_NETWORK  0x8000

typedef struct _FILELOCK {    /* flock */
    LONG lOffset;
    LONG lRange;
} FILELOCK;
typedef FILELOCK FAR *PFILELOCK;

typedef SHANDLE HFILE;	   /* hf */
typedef HFILE	*PHFILE;

#ifndef INCL_SAADEFS

typedef struct _FILEFINDBUF {	/* findbuf */
    FDATE  fdateCreation;
    FTIME  ftimeCreation;
    FDATE  fdateLastAccess;
    FTIME  ftimeLastAccess;
    FDATE  fdateLastWrite;
    FTIME  ftimeLastWrite;
    ULONG  cbFile;
    ULONG  cbFileAlloc;
    USHORT attrFile;
    UCHAR  cchName;
    CHAR   achName[CCHMAXPATHCOMP];
} FILEFINDBUF;
typedef FILEFINDBUF	*PFILEFINDBUF;

#pragma pack(2)

typedef struct _FILEFINDBUF2 {	/* findbuf2 */
    FDATE  fdateCreation;
    FTIME  ftimeCreation;
    FDATE  fdateLastAccess;
    FTIME  ftimeLastAccess;
    FDATE  fdateLastWrite;
    FTIME  ftimeLastWrite;
    ULONG  cbFile;
    ULONG  cbFileAlloc;
    USHORT attrFile;
    ULONG  cbList;
    UCHAR  cchName;
    CHAR   achName[CCHMAXPATHCOMP];
} FILEFINDBUF2;

#pragma pack()

typedef FILEFINDBUF2	*PFILEFINDBUF2;

/* extended attribute structures */

typedef struct _GEA {	    /* gea */
    BYTE cbName;	    /* name length not including NULL */
    CHAR szName[1];	    /* attribute name */
} GEA;
typedef GEA	*PGEA;

typedef struct _GEALIST {   /* geal */
    ULONG cbList;	    /* total bytes of structure including full list */
    GEA list[1];	    /* variable length GEA structures */
} GEALIST;
typedef GEALIST* PGEALIST;

typedef struct _FEA {	    /* fea */
    BYTE fEA;		    /* flags				  */
    BYTE cbName;	    /* name length not including NULL */
    USHORT cbValue;	    /* value length */
} FEA;
typedef FEA	*PFEA;

/* flags for _FEA.fEA */

#define FEA_NEEDEA 0x80	    /* need EA bit */

typedef struct _FEALIST {   /* feal */
    ULONG cbList;	    /* total bytes of structure including full list */
    FEA list[1];	    /* variable length FEA structures */
} FEALIST;
typedef FEALIST* PFEALIST;

typedef struct _EAOP {	    /* eaop */
    PGEALIST fpGEAList;	    /* general EA list */
    PFEALIST fpFEAList;	    /* full EA list */
    ULONG oError;
} EAOP;
typedef EAOP* PEAOP;

/*
 * Equates for the types of EAs that follow the convention that we have
 * established.
 *
 * Values 0xFFFE thru 0x8000 are reserved.
 * Values 0x0000 thru 0x7fff are user definable.
 * Value  0xFFFC is not used
 */

#define EAT_BINARY	0xFFFE		/* length preceeded binary */
#define EAT_ASCII	0xFFFD		/* length preceeded ASCII */
#define EAT_BITMAP	0xFFFB		/* length preceeded bitmap */
#define EAT_METAFILE	0xFFFA		/* length preceeded metafile */
#define EAT_ICON	0xFFF9		/* length preceeded icon */
#define EAT_EA		0xFFEE		/* length preceeded ASCII */
					/* name of associated data (#include) */
#define EAT_MVMT	0xFFDF		/* multi-valued, multi-typed field */
#define EAT_MVST	0xFFDE		/* multi-valued, single-typed field */
#define EAT_ASN1	0xFFDD		/* ASN.1 field */

#endif	/* !INCL_SAADEFS */

APIRET	APIENTRY	DosOpen(PSZ pszFile, PHFILE pHfile, PULONG pulAction, ULONG cbSize, ULONG attr, ULONG flag, ULONG mode, PEAOP peaop);

APIRET	APIENTRY	DosClose(HFILE hFile);

APIRET	APIENTRY	DosRead(HFILE hFile, PVOID pBuffer, ULONG cbRead, PULONG pcbActual);

APIRET	APIENTRY	DosWrite(HFILE hFile, PVOID pBuffer, ULONG cbWrite, PULONG pcbActual);

/* File time and date types */

typedef struct _FILESTATUS {	/* fsts */
    FDATE  fdateCreation;
    FTIME  ftimeCreation;
    FDATE  fdateLastAccess;
    FTIME  ftimeLastAccess;
    FDATE  fdateLastWrite;
    FTIME  ftimeLastWrite;
    ULONG  cbFile;
    ULONG  cbFileAlloc;
    USHORT attrFile;
} FILESTATUS;
typedef FILESTATUS	*PFILESTATUS;

#pragma pack(2)

typedef struct _FILESTATUS2 {	/* fsts2 */
    FDATE  fdateCreation;
    FTIME  ftimeCreation;
    FDATE  fdateLastAccess;
    FTIME  ftimeLastAccess;
    FDATE  fdateLastWrite;
    FTIME  ftimeLastWrite;
    ULONG  cbFile;
    ULONG  cbFileAlloc;
    USHORT attrFile;
    ULONG  cbList;
} FILESTATUS2;

#pragma pack()

typedef FILESTATUS2	*PFILESTATUS2;

typedef struct _FSALLOCATE {	/* fsalloc */
    ULONG  idFileSystem;
    ULONG  cSectorUnit;
    ULONG  cUnit;
    ULONG  cUnitAvail;
    USHORT cbSector;
} FSALLOCATE;
typedef FSALLOCATE	*PFSALLOCATE;

typedef SHANDLE HDIR;	     /* hdir */
typedef HDIR	*PHDIR;

/* XLATOFF */
#define DosOpen2	DosOpen
#define	DosFindFirst2	DosFindFirst
#define DosQFHandState	DosQueryFHState
#define DosSetFHandState	DosSetFHState
#define DosQHandType	DosQueryHType
#define DosQFSAttach	DosQueryFSAttach
#define DosNewSize	DosSetFileSize
#define DosBufReset	DosResetBuffer
#define DosChgFilePtr	DosSetFilePtr
#define DosFileLocks	DosSetFileLocks
#define DosMkDir	DosCreateDir
#define DosMkDir2	DosCreateDir
#define DosRmDir	DosDeleteDir
#define DosSelectDisk	DosSetDefaultDisk
#define DosQCurDisk	DosQueryCurrentDisk
#define DosChDir	DosSetCurrentDir
#define DosQCurDir	DosQueryCurrentDir
#define DosQFSInfo	DosQueryFSInfo
#define DosQVerify	DosQueryVerify
#define DosQFileInfo	DosQueryFileInfo
#define DosQPathInfo	DosQueryPathInfo
/* XLATON */

APIRET	APIENTRY	DosDelete(PSZ pszFile);

APIRET	APIENTRY	DosDupHandle(HFILE hFile, PHFILE pHfile);

APIRET	APIENTRY	DosQueryFHState(HFILE hFile, PULONG pMode);

APIRET	APIENTRY	DosSetFHState(HFILE hFile, ULONG mode);

APIRET	APIENTRY	DosQueryHType(HFILE hFile, PULONG pType, PULONG pAttr);

APIRET	APIENTRY	DosFindFirst(PSZ pszMatch, PHDIR pHdir, ULONG attr, PFILEFINDBUF pfindbuf, ULONG cbBuf, PULONG pcSearch, ULONG infolevel);

APIRET	APIENTRY	DosFindNext(HDIR hDir, PFILEFINDBUF pFindbuf, ULONG cbBuf, PULONG pcSearch);

APIRET	APIENTRY	DosFindClose(HDIR hDir);

APIRET	APIENTRY	DosFSAttach(PSZ pszDevice, PSZ pszFilesystem, PVOID pData, ULONG cbData, ULONG flag);

APIRET	APIENTRY	DosQueryFSAttach(PSZ pszDevice, ULONG ordinal, ULONG infolevel, PVOID pFSAttr, PULONG cbFSAttr);

APIRET	APIENTRY	DosFSCtl(PVOID pData, ULONG cbData, PULONG pcbData, PVOID pParms, ULONG cbParms, PULONG pcbParms, ULONG function, PSZ pszRoute, HFILE hFile, ULONG method);

APIRET	APIENTRY	DosSetFileSize(HFILE hFile, ULONG cbSize);

APIRET	APIENTRY	DosResetBuffer(HFILE hFile);

APIRET	APIENTRY	DosSetFilePtr(HFILE hFile, LONG ib, ULONG method, PULONG ibActual);

APIRET	APIENTRY	DosSetFileLocks(HFILE hFile, PFILELOCK pflUnlock, PFILELOCK pflLock);

APIRET	APIENTRY	DosMove(PSZ pszOld, PSZ pszNew);

APIRET	APIENTRY	DosCopy(PSZ pszOld, PSZ pszNew, ULONG option);

APIRET	APIENTRY	DosEditName(ULONG metalevel, PSZ pszSource, PSZ pszEdit, PBYTE pszTarget, ULONG cbTarget);

APIRET	APIENTRY	DosFileIO(HFILE hFile, PVOID pCmd, ULONG cbCmd, PLONG ibError);

APIRET	APIENTRY	DosCreateDir(PSZ pszDir, PEAOP peaop);

APIRET	APIENTRY	DosDeleteDir(PSZ pszDir);

APIRET	APIENTRY	DosSetDefaultDisk(ULONG disknum);

APIRET	APIENTRY	DosQueryCurrentDisk(PULONG pdisknum, PULONG plogical);

APIRET	APIENTRY	DosSetCurrentDir(PSZ pszDir);

APIRET	APIENTRY	DosQueryCurrentDir(ULONG disknum, PBYTE pBuf, PULONG pcbBuf);

APIRET	APIENTRY	DosQueryFSInfo(ULONG disknum, ULONG infolevel, PVOID pBuf, ULONG cbBuf);

APIRET	APIENTRY	DosSetFSInfo(ULONG disknum, ULONG infolevel, PVOID pBuf, ULONG cbBuf);

APIRET	APIENTRY	DosQueryVerify(PBOOL32 pBool);

APIRET	APIENTRY	DosSetVerify(BOOL32);

APIRET	APIENTRY	DosSetMaxFH(ULONG cFH);

APIRET  APIENTRY        DosSetRelMaxFH(PLONG pcbReqCount, PULONG pcbCurMaxFH);

APIRET	APIENTRY	DosQueryFileInfo(HFILE hFile, ULONG infolevel, PVOID pBuf, ULONG cbBuf);

APIRET	APIENTRY	DosSetFileInfo(HFILE hFile, ULONG infolevel, PVOID pBuf, ULONG cbBuf);

APIRET	APIENTRY	DosQueryPathInfo(PSZ pszPath, ULONG infolevel, PVOID pBuf, ULONG cbBuf);

APIRET	APIENTRY	DosSetPathInfo(PSZ pszPath, ULONG infolevel, PVOID pBuf, ULONG cbBuf, ULONG usFlags);

/* defines for DosSetPathInfo -pathinfo flag */
#define DSPI_WRTTHRU	0x10	/* write through */


APIRET	APIENTRY	DosEnumAttribute(ULONG fRefType, PVOID pFileRef, ULONG iStartEntry, PVOID pEnumBuf, ULONG cbBuf, PULONG pcbActual, ULONG infoLevel);

typedef struct _DENA1 {	/* _dena1 level 1 info returned from DosEnumAttribute */
    UCHAR	reserved;	/* 0 */
    UCHAR	cbName;		/* length of name exculding NULL */
    USHORT	cbValue;	/* length of value */
    UCHAR	szName[1];	/* variable length asciiz name */
} DENA1;
typedef DENA1 FAR *PDENA1;

/* Infolevels for DosEnumAttribute  */
#define	ENUMEA_LEVEL_NO_VALUE	1L	/* FEA without value */
/* Reference types for DosEnumAttribute */
#define	ENUMEA_REFTYPE_FHANDLE	0	/* file handle */
#define	ENUMEA_REFTYPE_PATH	1	/* path name */
#define	ENUMEA_REFTYPE_MAX	ENUMEA_REFTYPE_PATH

#endif /* common INCL_DOSFILEMGR */

#if (defined(INCL_DOSMEMMGR) || !defined(INCL_NOCOMMON))
/*** Memory management */

APIRET	APIENTRY	DosAllocMem(PPVOID ppb, ULONG cb, ULONG flag);

APIRET	APIENTRY	DosFreeMem(PVOID pb);

APIRET	APIENTRY	DosSetMem(PVOID pb, ULONG cb, ULONG flag);

APIRET	APIENTRY	DosGiveSharedMem(PVOID pb, PID pid, ULONG flag);

APIRET	APIENTRY	DosGetSharedMem(PVOID pb, ULONG flag);

APIRET	APIENTRY	DosGetNamedSharedMem(PPVOID ppb, PSZ pszName, ULONG flag);

APIRET	APIENTRY	DosAllocSharedMem(PPVOID ppb, PSZ pszName, ULONG cb, ULONG flag);

APIRET	APIENTRY	DosQueryMem(PVOID pb, PULONG pcb, PULONG pFlag);

APIRET	APIENTRY	DosSubAlloc(PVOID pbBase, PPVOID ppb, ULONG cb);

APIRET	APIENTRY	DosSubFree(PVOID pbBase, PVOID pb, ULONG cb);

APIRET	APIENTRY	DosSubSet(PVOID pbBase, ULONG flag, ULONG cb);

APIRET	APIENTRY	DosSubUnset(PVOID pbBase);

#include <bsememf.h>	/* get flags for API				*/

#endif /* INCL_DOSMEMMGR */

#if (defined(INCL_DOSSEMAPHORES) || !defined(INCL_NOCOMMON))

/*
 *     32-bit Semaphore Support
 */

/* Semaphore Attributes */

#define DC_SEM_SHARED	0x01	  /* DosCreateMutex, DosCreateEvent, and     */
				  /*   DosCreateMuxWait use it to indicate   */
				  /*   whether the semaphore is shared or    */
				  /*   private when the PSZ is null	     */
#define DCMW_WAIT_ANY	0x02	  /* DosCreateMuxWait option for wait on any */
				  /*   event/mutex to occur		     */
#define DCMW_WAIT_ALL	0x04	  /* DosCreateMuxWait option for wait on all */
				  /*   events/mutexs to occur		     */

#define SEM_INDEFINITE_WAIT	-1L
#define SEM_IMMEDIATE_RETURN	 0L

typedef struct _PSEMRECORD {	/* psr */
    HSEM	hsemCur;
    ULONG	ulUser;
} SEMRECORD;
typedef SEMRECORD	*PSEMRECORD;

#endif /* common INCL_DOSSEMAPHORES */

#ifdef INCL_DOSSEMAPHORES

typedef	 ULONG	  HMTX;		   /* hmtx */
typedef	 HMTX	  *PHMTX;
typedef	 ULONG	  HEV;		   /* hev */
typedef	 HEV	  *PHEV;
typedef	 ULONG	  HMUX;		   /* hmux */
typedef	 HMUX	  *PHMUX;
typedef	 VOID FAR *HSEM;	   /* hsem */
typedef	 HSEM FAR *PHSEM;

APIRET	APIENTRY	DosCreateEventSem (PSZ pszName, PHEV phev, ULONG flAttr, BOOL32 fState);
APIRET	APIENTRY	DosOpenEventSem (PSZ pszName, PHEV phev);
APIRET	APIENTRY	DosCloseEventSem (HEV hev);
APIRET	APIENTRY	DosResetEventSem (HEV hev, PULONG pulPostCt);
APIRET	APIENTRY	DosPostEventSem (HEV hev);
APIRET	APIENTRY	DosWaitEventSem (HEV hev, ULONG ulTimeout);
APIRET	APIENTRY	DosQueryEventSem (HEV hev, PULONG pulPostCt);

APIRET	APIENTRY	DosCreateMutexSem (PSZ pszName, PHMTX phmtx, ULONG flAttr, BOOL32 fState);
APIRET	APIENTRY	DosOpenMutexSem (PSZ pszName, PHMTX phmtx);
APIRET	APIENTRY	DosCloseMutexSem (HMTX hmtx);
APIRET	APIENTRY	DosRequestMutexSem (HMTX hmtx, ULONG ulTimeout);
APIRET	APIENTRY	DosReleaseMutexSem (HMTX hmtx);
APIRET	APIENTRY	DosQueryMutexSem (HMTX hmtx, PID *ppid, TID *ptid, PULONG pulCount);

APIRET	APIENTRY	DosCreateMuxWaitSem (PSZ pszName, PHMUX phmux, ULONG cSemRec, PSEMRECORD pSemRec, ULONG flAttr);
APIRET	APIENTRY	DosOpenMuxWaitSem (PSZ pszName, PHMUX phmux);
APIRET	APIENTRY	DosCloseMuxWaitSem (HMUX hmux);
APIRET	APIENTRY	DosWaitMuxWaitSem (HMUX hmux, ULONG ulTimeout, PULONG pulUser);
APIRET	APIENTRY	DosAddMuxWaitSem (HMUX hmux, PSEMRECORD pSemRec);
APIRET	APIENTRY	DosDeleteMuxWaitSem (HMUX hmux, HSEM hSem);
APIRET	APIENTRY	DosQueryMuxWaitSem (HMUX hmux, PULONG pcSemRec, PSEMRECORD pSemRec, PULONG pflAttr);

#endif /* INCL_DOSSEMAPHORES */

#if (defined(INCL_DOSDATETIME) || !defined(INCL_NOCOMMON))

/*** Time support */

typedef struct _DATETIME {    /* date */
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
typedef DATETIME	*PDATETIME;

APIRET	APIENTRY	DosGetDateTime(PDATETIME pdt);

APIRET	APIENTRY	DosSetDateTime(PDATETIME pdt);

#endif /* common INCL_DOSDATETIME */

#ifdef INCL_DOSDATETIME

/* XLATOFF */
#define DosTimerAsync	DosAsyncTimer
#define DosTimerStart	DosStartTimer
#define DosTimerStop	DosStopTimer
/* XLATON */

typedef SHANDLE HTIMER;
typedef HTIMER	*PHTIMER;

APIRET	APIENTRY	DosAsyncTimer(ULONG msec, HSEM hsem, PHTIMER phtimer);

APIRET	APIENTRY	DosStartTimer(ULONG msec, HSEM hsem, PHTIMER phtimer);

APIRET	APIENTRY	DosStopTimer(HTIMER htimer);

#endif /* INCL_DOSDATETIME */


/*** Module manager */

#ifdef INCL_DOSMODULEMGR

/* XLATOFF */
#define DosGetProcAddr		DosQueryProcAddr
#define DosGetModHandle		DosQueryModuleHandle
#define DosGetModName		DosQueryModuleName
/* XLATON */

APIRET	APIENTRY	DosLoadModule(PSZ pszName, ULONG cbName, PSZ pszModname, PHMODULE phmod);

APIRET	APIENTRY	DosFreeModule(HMODULE hmod);

APIRET	APIENTRY	DosQueryProcAddr(HMODULE hmod, ULONG ordinal, PSZ pszName,PFN* ppfn);

APIRET	APIENTRY	DosQueryModuleHandle(PSZ pszModname, PHMODULE phmod);

APIRET	APIENTRY	DosQueryModuleName(HMODULE hmod, ULONG cbName, PCHAR pch);

#define	PT_16BIT	0
#define	PT_32BIT	1

APIRET	APIENTRY	DosQueryProcType(HMODULE hmod, ULONG ordinal, PSZ pszName, PULONG pulproctype);

#endif /* INCL_DOSMODULEMGR */

#if (defined(INCL_DOSRESOURCES) || !defined(INCL_NOCOMMON))

/*** Resource support */

/* Predefined resource types */

#define RT_POINTER	1   /* mouse pointer shape */
#define RT_BITMAP	2   /* bitmap */
#define RT_MENU		3   /* menu template */
#define RT_DIALOG	4   /* dialog template */
#define RT_STRING	5   /* string tables */
#define RT_FONTDIR	6   /* font directory */
#define RT_FONT		7   /* font */
#define RT_ACCELTABLE	8   /* accelerator tables */
#define RT_RCDATA	9   /* binary data */
#define RT_MESSAGE	10  /* error msg     tables */
#define RT_DLGINCLUDE	11  /* dialog include file name */
#define RT_VKEYTBL	12  /* key to vkey tables */
#define RT_KEYTBL	13  /* key to UGL tables */
#define RT_CHARTBL	14  /* glyph to character tables */
#define RT_DISPLAYINFO	15  /* screen display information */

#define RT_FKASHORT	16  /* function key area short form */
#define RT_FKALONG	17  /* function key area long form */

#define RT_HELPTABLE	18  /* Help table for Cary Help manager */
#define RT_HELPSUBTABLE 19  /* Help subtable for Cary Help manager */

#define RT_FDDIR	20  /* DBCS uniq/font driver directory */
#define RT_FD		21  /* DBCS uniq/font driver */

#define RT_MAX		22  /* 1st unused Resource Type */


#define RF_ORDINALID	0x80000000L	/* ordinal id flag in resource table */

#endif /* common INCL_DOSRESOURCES */

#ifdef INCL_DOSRESOURCES

/* XLATOFF */
#define DosGetResource2 DosGetResource
/* XLATON */

APIRET	APIENTRY	DosGetResource(HMODULE hmod, ULONG idType, ULONG idName, PPVOID ppb);

APIRET	APIENTRY	DosFreeResource(PVOID pb);

APIRET	APIENTRY	DosQueryResourceSize(HMODULE hmod, ULONG idt, ULONG idn, PULONG pulsize);

#endif /* INCL_DOSRESOURCES */


/*** NLS Support */

#ifdef INCL_DOSNLS

typedef struct _COUNTRYCODE { /* ctryc */
    ULONG	country;
    ULONG	codepage;
} COUNTRYCODE;
typedef COUNTRYCODE	*PCOUNTRYCODE;

typedef struct _COUNTRYINFO { /* ctryi */
    ULONG	country;
    ULONG	codepage;
    ULONG	fsDateFmt;
    CHAR	szCurrency[5];
    CHAR	szThousandsSeparator[2];
    CHAR	szDecimal[2];
    CHAR	szDateSeparator[2];
    CHAR	szTimeSeparator[2];
    UCHAR	fsCurrencyFmt;
    UCHAR	cDecimalPlace;
    UCHAR	fsTimeFmt;
    USHORT	abReserved1[2];
    CHAR	szDataSeparator[2];
    USHORT	abReserved2[5];
} COUNTRYINFO;
typedef COUNTRYINFO	*PCOUNTRYINFO;

/* XLATOFF */
#define DosGetCtryInfo	DosQueryCtryInfo
#define DosGetDBCSEv	DosQueryDBCSEnv
#define DosCaseMap	DosMapCase
#define DosGetCollate	DosQueryCollate
#define DosGetCp	DosQueryCp
#define DosSetProcCp	DosSetProcessCp
/* XLATON */

APIRET	APIENTRY	DosQueryCtryInfo(ULONG cb, PCOUNTRYCODE pcc, PCOUNTRYINFO pci, PULONG pcbActual);

APIRET	APIENTRY	DosQueryDBCSEnv(ULONG cb, PCOUNTRYCODE pcc, PCHAR pBuf);

APIRET	APIENTRY	DosMapCase(ULONG cb, PCOUNTRYCODE pcc, PCHAR pch);

APIRET	APIENTRY	DosQueryCollate(ULONG cb, PCOUNTRYCODE pcc, PCHAR pch, PULONG pcch);

APIRET	APIENTRY	DosQueryCp(ULONG cb, PULONG arCP, PULONG pcCP);

APIRET	APIENTRY	DosSetProcessCp(ULONG cp);

#endif /* INCL_DOSNLS */


/*** Signal support */

#ifdef INCL_DOSSIGNALS

/* DosSetSigExceptionFocus codes */

#define	SIG_UNSETFOCUS 0
#define	SIG_SETFOCUS 1

#include <bsexcpt.h>

APIRET	APIENTRY	DosSetExceptionHandler(PEHS phandler);

APIRET	APIENTRY	DosUnsetExceptionHandler(PEHS phandler);

APIRET	APIENTRY	DosRaiseException(PXCPT pexcept);

APIRET	APIENTRY	DosSendSignalException(PID pid, ULONG exception);

APIRET	APIENTRY	DosUnwindException(PEHS phandler);

APIRET	APIENTRY	DosSetSignalExceptionFocus(BOOL32 flag, PULONG pulTimes);

APIRET	APIENTRY	DosEnterMustComplete(PULONG pulNesting);

APIRET	APIENTRY	DosExitMustComplete(PULONG pulNesting);

#endif /* INCL_DOSSIGNALS */


/*** Pipe and queue support */

#ifdef INCL_DOSQUEUES
#if (defined(INCL_DOSFILEMGR) || !defined(INCL_NOCOMMON))

typedef SHANDLE HQUEUE;	 /* hq */
typedef HQUEUE	*PHQUEUE;
typedef struct _REQUESTDATA {	/* reqqdata */
    PID		pid;
    ULONG	ulData;
} REQUESTDATA;
typedef REQUESTDATA	*PREQUESTDATA;

#define	QUE_FIFO	0L
#define	QUE_LIFO	1L
#define	QUE_PRIORITY	2L

/* XLATOFF */
#define DosMakePipe	DosCreatePipe
/* XLATON */

APIRET	APIENTRY	DosCreatePipe(PHFILE phfRead, PHFILE phfWrite, ULONG cb);

APIRET	APIENTRY	DosCloseQueue(HQUEUE hq);

APIRET	APIENTRY	DosCreateQueue(PHQUEUE phq, ULONG priority, PSZ pszName);

APIRET	APIENTRY	DosOpenQueue(PPID ppid, PHQUEUE phq, PSZ pszName);

APIRET	APIENTRY	DosPeekQueue(HQUEUE hq, PREQUESTDATA pRequest, PULONG pcbData, PPVOID ppbuf, PULONG element, BOOL32 nowait, PBYTE ppriority, HSEM hsem);

APIRET	APIENTRY	DosPurgeQueue(HQUEUE hq);

APIRET	APIENTRY	DosQueryQueue(HQUEUE hq, PULONG pcbEntries);

APIRET	APIENTRY	DosReadQueue(HQUEUE hq, PREQUESTDATA pRequest, PULONG pcbData, PPVOID ppbuf, ULONG element, BOOL32 wait, PBYTE ppriority, HSEM hsem);

APIRET	APIENTRY	DosWriteQueue(HQUEUE hq, ULONG request, ULONG cbData, PVOID pbData, ULONG priority);

#else /* INCL_DOSFILEMGR || !INCL_NOCOMMON */
#error PHFILE not defined - define INCL_DOSFILEMGR or undefine INCL_NOCOMMON
#endif /* INCL_DOSFILEMGR || !INCL_NOCOMMON */
#endif /* INCL_DOSQUEUES */

#ifdef INCL_DOSMISC

/* definitions for DosSearchPath control word */
#define DSP_IMPLIEDCUR		1 /* current dir will be searched first */
#define DSP_PATHREF		2 /* from env.variable */
#define DSP_IGNORENETERR	4 /* ignore net errs & continue search */

/* indices for DosQuerySysInfo */
#define	QSV_MAX_PATH_LENGTH	1
#define	Q_MAX_PATH_LENGTH	QSV_MAX_PATH_LENGTH
#define	QSV_MAX_TEXT_SESSIONS	2
#define	QSV_MAX_PM_SESSIONS	3
#define	QSV_MAX_VDM_SESSIONS	4
#define	QSV_BOOT_DRIVE		5	/* 1=A, 2=B, etc. */
#define	QSV_DYN_PRI_VARIATION	6	/* 0=Absolute, 1=Dynamic */
#define	QSV_MAX_WAIT		7	/* seconds */
#define	QSV_MIN_SLICE		8	/* milli seconds */
#define	QSV_MAX_SLICE		9	/* milli seconds */
#define	QSV_PAGE_SIZE		10
#define	QSV_VERSION_MAJOR	11
#define	QSV_VERSION_MINOR	12
#define	QSV_VERSION_REVISION	13	/* Revision letter */
#define	QSV_MS_COUNT		14	/* Free running millisecond counter */
#define	QSV_TIME_LOW		15	/* Low dword of time in seconds */
#define	QSV_TIME_HIGH		16	/* High dword of time in seconds */
#define	QSV_TOTPHYSMEM		17	/* Physical memory on system */
#define	QSV_TOTRESMEM		18	/* Resident memory on system */
#define	QSV_TOTAVAILMEM		19	/* Available memory for all processes */
#define	QSV_MAXPRMEM		20	/* Avail private mem for calling proc */
#define	QSV_MAXSHMEM		21	/* Avail shared mem for calling proc */
#define	QSV_TIMER_INTERVAL	22	/* Timer interval in tenths of ms */
#define	QSV_MAX			QSV_TIMER_INTERVAL

/* definitions for DosError - combine with | */
#define	FERR_DISABLEHARDERR	0x00000000L	/* disable hard error popups */
#define	FERR_ENABLEHARDERR	0x00000001L	/* enable hard error popups */
#define	FERR_ENABLEEXCEPTION	0x00000000L	/* enable exception popups */
#define	FERR_DISABLEEXCEPTION	0x00000002L	/* disable exception popups */

/* XLATOFF */
#define DosInsMessage	DosInsertMessage
#define DosQSysInfo	DosQuerySysInfo
/* XLATON */

APIRET	APIENTRY	DosError(ULONG error);

APIRET	APIENTRY	DosGetMessage(PCHAR* pTable, ULONG cTable, PCHAR pBuf, ULONG cbBuf, ULONG msgnumber, PSZ pszFile, PULONG pcbMsg);

APIRET	APIENTRY	DosErrClass(ULONG code, PULONG pClass, PULONG pAction, PULONG pLocus);

APIRET	APIENTRY	DosInsertMessage(PCHAR* pTable, ULONG cTable, PSZ pszMsg, ULONG cbMsg, PCHAR pBuf, ULONG cbBuf, PULONG pcbMsg);

APIRET	APIENTRY	DosPutMessage(HFILE hfile, ULONG cbMsg, PCHAR pBuf);

APIRET	APIENTRY	DosQuerySysInfo(ULONG iStart, ULONG iLast, PVOID pBuf, ULONG cbBuf);

APIRET	APIENTRY	DosScanEnv(PSZ pszName, PSZ *ppszValue);

APIRET	APIENTRY	DosSearchPath(ULONG flag, PSZ pszPathOrName, PSZ pszFilename, PBYTE pBuf, ULONG cbBuf);

APIRET	APIENTRY	DosQueryMessageCP(PCHAR pb, ULONG cb, PSZ pszFilename, PULONG cbBuf);

APIRET	APIENTRY	DosDynamicTrace(ULONG lCmd, PBYTE preqUser, PBYTE presUser);

#endif /* INCL_DOSMISC */


/*** Session manager support */

#ifdef INCL_DOSSESMGR

typedef struct _STARTDATA {   /* stdata */
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
    PSZ	    ObjectBuffer;
    ULONG   ObjectBuffLen;
} STARTDATA;
typedef STARTDATA	*PSTARTDATA;

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

typedef struct _STATUSDATA { /* stsdata */
    USHORT Length;
    USHORT SelectInd;
    USHORT BondInd;
} STATUSDATA;
typedef STATUSDATA	*PSTATUSDATA;

/* XLATOFF */
#define DosQAppType	DosQueryAppType
/* XLATON */

APIRET	APIENTRY	DosStartSession(PSTARTDATA psd, PULONG pidSession, PPID ppid);

APIRET	APIENTRY	DosSetSession(ULONG idSession, PSTATUSDATA psd);

APIRET	APIENTRY	DosSelectSession(ULONG idSession);

APIRET	APIENTRY	DosStopSession(ULONG scope, ULONG idSession);

APIRET	APIENTRY	DosQueryAppType(PSZ pszName, PULONG pFlags);

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
#define DosDevIOCtl2	DosDevIOCtl
/* XLATON */

APIRET	APIENTRY	DosDevConfig(PVOID pdevinfo, ULONG item);
#define	DEVINFO_PRINTER		0	/* Number of printers attached */
#define	DEVINFO_RS232		1	/* Number of RS232 ports */
#define	DEVINFO_FLOPPY		2	/* Number of diskette drives */
#define	DEVINFO_COPROCESSOR	3	/* Presence of math coprocessor */
#define	DEVINFO_SUBMODEL	4	/* PC Submodel Type */
#define	DEVINFO_MODEL		5	/* PC Model Type */
#define	DEVINFO_ADAPTER		6	/* Primary display adapter type */
#define	DEVINFO_COPROCESSORTYPE	7	/* Type of coprocessor functionality */

APIRET  APIENTRY        DosDevIOCtl(HFILE hDevice, ULONG category, ULONG function, PVOID pParams, ULONG cbParmLenMax, PULONG pcbParmLen, PVOID pData, ULONG cbDataLenMax, PULONG pcbDataLen);


APIRET	APIENTRY	DosPhysicalDisk(ULONG function, PVOID pBuf, ULONG cbBuf, PVOID pParams, ULONG cbParams);
#define	INFO_COUNT_PARTITIONABLE_DISKS	1	/* # of partitionable disks */
#define	INFO_GETIOCTLHANDLE		2	/* Obtain handle	    */
#define	INFO_FREEIOCTLHANDLE		3	/* Release handle	    */

#endif /* INCL_DOSDEVICES */


/*** DosNamedPipes API Support */

#ifdef INCL_DOSNMPIPES

/*** Data structures used with named pipes ***/

typedef SHANDLE HPIPE;	   /* hp */
typedef HPIPE	*PHPIPE;

typedef struct _AVAILDATA {		/* AVAILDATA */
    USHORT	cbpipe;			/* bytes left in the pipe */
    USHORT	cbmessage;		/* bytes left in the current message */
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
#define DosCallNmPipe	DosCallNPipe
#define DosConnectNmPipe	DosConnectNPipe
#define DosDisConnectNmPipe	DosDisConnectNPipe
#define DosMakeNmPipe	DosCreateNPipe
#define DosPeekNmPipe	DosPeekNPipe
#define DosQNmPHandState	DosQueryNPHState
#define DosQNmPipeInfo	DosQueryNPipeInfo
#define DosQNmPipeSemState	DosQueryNPipeSemState
#define DosRawReadNmPipe	DosRawReadNPipe
#define DosRawWriteNmPipe	DosRawWriteNPipe
#define DosSetNmPHandState	DosSetNPHState
#define DosSetNmPipeSem DosSetNPipeSem
#define DosTransactNmPipe	DosTransactNPipe
#define DosWaitNmPipe	DosWaitNPipe
/* XLATON */

APIRET	APIENTRY	DosCallNPipe(PSZ pszName, PVOID pInbuf, ULONG cbIn, PVOID pOutbuf, ULONG cbOut, PULONG pcbActual, ULONG msec);

APIRET	APIENTRY	DosConnectNPipe(HPIPE hpipe);

APIRET	APIENTRY	DosDisConnectNPipe(HPIPE hpipe);

APIRET	APIENTRY	DosCreateNPipe(PSZ pszName, PHPIPE pHpipe, ULONG openmode, ULONG pipemode, ULONG cbInbuf, ULONG cbOutbuf, ULONG msec);

APIRET	APIENTRY	DosPeekNPipe(HPIPE hpipe, PVOID pBuf, ULONG cbBuf, PULONG pcbActual, PAVAILDATA pAvail, PULONG pState);

APIRET	APIENTRY	DosQueryNPHState(HPIPE hpipe, PULONG pState);

APIRET	APIENTRY	DosQueryNPipeInfo(HPIPE hpipe, ULONG infolevel, PVOID pBuf, ULONG cbBuf);

APIRET	APIENTRY	DosQueryNPipeSemState(HSEM hsem, PPIPESEMSTATE pnpss, ULONG cbBuf);

APIRET	APIENTRY	DosRawReadNPipe(PSZ pszName, ULONG cb, PULONG pLen, PVOID pBuf);

APIRET	APIENTRY	DosRawWriteNPipe(PSZ pszName, ULONG cb);

APIRET	APIENTRY	DosSetNPHState(HPIPE hpipe, ULONG state);

APIRET	APIENTRY	DosSetNPipeSem(HPIPE hpipe, HSEM hsem, ULONG key);

APIRET	APIENTRY	DosTransactNPipe(HPIPE hpipe, PVOID pOutbuf, ULONG cbOut, PVOID pInbuf, ULONG cbIn, PULONG pcbRead);

APIRET	APIENTRY	DosWaitNPipe(PSZ pszName, ULONG msec);

/* values in fStatus */
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

#define PROF_ORDINAL	 133

/* DosProfile usType */

#define PROF_SYSTEM	 0
#define PROF_USER	 1
#define PROF_USEDD	 2
#define PROF_KERNEL	 4
#define PROF_VERBOSE	 8
#define PROF_ENABLE	16

/* DosProfile usFunc */

#define PROF_ALLOC	 0
#define PROF_CLEAR	 1
#define PROF_ON		 2
#define PROF_OFF	 3
#define PROF_DUMP	 4
#define PROF_FREE	 5

/* DosProfile tic count granularity (DWORD) */

#define PROF_SHIFT	 2

/* DosProfile module name string length */

#define PROF_MOD_NAME_SIZE   10

/* DosProfile error code for end of data */

#define PROF_END_OF_DATA     13

#endif /* INCL_DOSPROFILE */


/*** Virtual DOS Machine API support */

#ifdef INCL_DOSMVDM

typedef LHANDLE	  HVDD;	    /* hvdd */
typedef HVDD FAR *PHVDD;    /* phvdd */

APIRET	APIENTRY DosOpenVDD(PSZ pszVDD, PHVDD phvdd);

APIRET	APIENTRY DosRequestVDD(HVDD hvdd, SGID sgid, ULONG cmd,
			       ULONG cbInput, PVOID pInput,
			       ULONG cbOutput, PVOID pOutput);

APIRET	APIENTRY DosCloseVDD(HVDD hvdd);

APIRET	APIENTRY DosQueryDOSProperty(SGID sgid, PSZ pszName,
				     ULONG cb, PSZ pch);

APIRET	APIENTRY DosSetDOSProperty(SGID sgid, PSZ pszName,
				   ULONG cb, PSZ pch);
#endif /* INCL_DOSMVDM */


/* The following #else/#endif corresponds to a #if near the top of this */
/* file.  The next three lines include the 16-bit version of this file, */
/* hence these lines MUST be the last lines in this include file.	*/
#else /* not INCL_32 */

#ifdef INCL_16
#include <bsedos16.h>
#endif /* INCL_16 */

#endif /* INCL_32 */
