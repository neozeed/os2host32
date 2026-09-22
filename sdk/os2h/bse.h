/*static char *SCCSID = "@(#)bse.h   13.7 90/05/02";*/
/****************************** Module Header ******************************\
*
* Module Name: BSE.H
*
* This file includes the definitions necessary for writing Base OS/2 applications.
*
* Copyright (c) 1987  Microsoft Corporation
* Copyright (c) 1987  IBM Corporation
*
*
* ===========================================================================
*
* The following symbols are used in this file for conditional sections.
*
*   INCL_BASE	-  ALL of OS/2 Base
*   INCL_DOS	-  OS/2 DOS Kernel
*   INCL_SUB	-  OS/2 VIO/KBD/MOU
*   INCL_DOSERRORS -  OS/2 Errors	  - only included if symbol defined
*
\***************************************************************************/

#define INCL_BASEINCLUDED

/* if INCL_BASE defined then define all the symbols */
#ifdef INCL_BASE
    #define INCL_DOS
    #define INCL_SUB
    #define INCL_DOSERRORS
#endif /* INCL_BASE */

#ifndef OS2_INCLUDED
#if !(defined(INCL_32) || defined(INCL_16))
#ifdef M_I386
    #define INCL_32
#else /* not M_I386 */
    #define INCL_16
#endif /* M_I386 */
#endif /* INCL_32 || INCL_16 */
#endif /* OS2_INCLUDED */

/* XLATOFF */
#ifdef INCL_32
#if defined(INCL_16)
#error message ("Illegal combination of API Flags - 32 && 16")
#endif /* || INCL_16 */
#endif /* INCL_32 */
/* XLATON */

#include <bsedos.h>	  /* Base definitions */
#ifndef INCL_32
#include <bsesub.h>	  /* VIO/KBD/MOU definitions */
#endif /* INCL_32 */
#include <bseerr.h>	  /* Base error code definitions */

#ifdef INCL_ORDINALS
#include <bseord.h>	/* ordinals */
#endif /* INCL_ORDINALS */
