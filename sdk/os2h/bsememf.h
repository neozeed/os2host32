/*static char *SCCSID = "@(#)bsememf.h	1.5 90/05/09";*/
/***************************************************************************\
*
* Module Name: BSEMEMF.H
*
* OS/2 Base MEMory Flags include file
*
* Copyright (c) 1989  Microsoft Corporation
* Copyright (c) 1989  IBM Corporation
*
************************************************************************/

/* Access protection */
#define PAG_READ	0x00000001	/* read access */
#define PAG_WRITE	0x00000002	/* write access */
#define PAG_EXECUTE	0x00000004	/* execute access */
#define PAG_GUARD	0x00000008	/* guard protection */
#define PAG_DEFAULT	0x00000400	/* default (initial) access */

/* Commit */
#define PAG_COMMIT	0x00000010	/* commit storage */
#define PAG_DECOMMIT	0x00000020	/* decommit storage */

/* Allocation attributes */
#define OBJ_TILE	0x00000040	/* tile object */
#define OBJ_PROTECTED	0x00000080	/* protect object
					   NOTE: This flag is NOT available at
					   the api level */
#define OBJ_GETTABLE	0x00000100	/* gettable by other processes */
#define OBJ_GIVEABLE	0x00000200	/* giveable to other processes */

#define fPERM		(PAG_EXECUTE + PAG_READ + PAG_WRITE)
#define fSHARE		(OBJ_GETTABLE + OBJ_GIVEABLE)

/* DosAllocMem flags */
#define fALLOC		(OBJ_TILE + PAG_COMMIT + fPERM)

/* DosAllocSharedMem flags */
#define fALLOCSHR	(OBJ_TILE + PAG_COMMIT + fSHARE + fPERM)

/* DosGetNamedSharedMem flags */
#define fGETNMSHR	(fPERM)

/* DosGetSharedMem flags */
#define fGETSHR		(fPERM)

/* DosGiveSharedMem flags */
#define fGIVESHR	(fPERM)

/* DosSetMem flags */
#define fSET		(PAG_COMMIT + PAG_DECOMMIT + PAG_DEFAULT + fPERM)

/* Dos32SubSet flags */
#define	DOSSUB_INIT		0x01	/* initialize memory object for	*/
					/* suballocation		*/
#define	DOSSUB_GROW		0x02	/* increase size of memory pool	*/
					/* for suballocation		*/
#define	DOSSUB_SPARSE_OBJ	0x04	/* indicator for DosSub to	*/
					/* manage the commitment of	*/
					/* pages spanned by the memory	*/
					/* pool				*/
#define	DOSSUB_SERIALIZE	0x08	/* indicates that access to the	*/
					/* memory pool is to be		*/
					/* serialized by DosSub		*/

/* Allocation type (returned from DosQueryMem) */
#define PAG_SHARED	0x00002000	/* shared object */
#define PAG_FREE	0x00004000	/* pages are free */
#define PAG_BASE	0x00010000	/* first page in object */
