/*static char *SCCSID = "@(#)os2std.h	1.00 89/10/30";*/
/***************************************************************************\
*
* Module Name: OS2STD.H
*
* OS/2 Standard Definitions file
*
* Copyright (c) 1987,1989  IBM Corporation
* Copyright (c) 1987,1989  Microsoft Corporation
*
\***************************************************************************/

/* This file contains definitions that are more dependant upon the	*/
/* definition of the c language than upon the implementation of OS|2.	*/
/* It is similar to stdio.h.						*/

/* XLATOFF */
#if (defined(M_I86SM) || defined(M_I86MM))
/* XLATON */
#define	 NULL	 0
/* XLATOFF */
#else
#if (defined(M_I86L) || defined(M_I86CM) || defined(M_I86LM) || defined(M_I86HM))
#define	 NULL	 0L
#else
#define	 NULL	 0
#endif
#endif
/* XLATON */

#define FALSE	0
#define TRUE	1
