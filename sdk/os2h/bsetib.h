/*	SCCSID = @(#)tib.h	  13.4	90/03/26 */
/***************************************************************************\
*
* Module Name: BSETIB.H
*
* OS/2 Thread Information Block Include File
*
* Copyright (c) 1989  Microsoft Corporation
* Copyright (c) 1989  IBM Corporation
*
*****************************************************************************


/*
 *	Thread Information Block (TIB)
 */


struct tib_s {				/* TIB Thread Information Block */
	ULONG	tib_ultid;		/* Thread I.D. */
	ULONG	tib_ulpri;		/* Thread priority */
	PVOID	tib_pstack;		/* Pointer to base of stack */
	ULONG	tib_cbstack;		/* Maximum size of stack */
	PVOID	tib_pexchain;		/* Head of exception handler chain */
	USHORT	tib_usMCCount;		/* Must Complete count */
	USHORT	tib_fMCForceFlag;	/* Must Complete force flag */
};


/* XLATOFF */
typedef struct tib_s	tib_t;
typedef struct tib_s	TIB;
typedef struct tib_s	*PTIB;
/* XLATON */


/*
 *	Process Information Block (PIB)
 */


struct pib_s {				/* PIB Process Information Block */
	ULONG	pib_ulpid;		/* Process I.D. */
	ULONG	pib_ulppid;		/* Parent process I.D. */
	ULONG	pib_hmte;		/* Program (.EXE) module handle */
	PCHAR	pib_pchcmd;		/* Command line pointer */
	PCHAR	pib_pchenv;		/* Environment pointer */
	ULONG	pib_flstatus;		/* Process' status bits */
	ULONG	pib_ultype;		/* Process' type code */
};


/* XLATOFF */
typedef struct pib_s	pib_t;
typedef struct pib_s	PIB;
typedef struct pib_s	*PPIB;
/* XLATON */
