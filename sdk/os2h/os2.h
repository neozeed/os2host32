/****************************** Module Header ******************************\
*
*   Copyright (c) IBM Corporation 1987, 1989
*   Copyright (c) MicroSoft Corporation 1987,1989
*
* Module Name: OS2.H
*
* This is the top level include file that includes all the files necessary
* for writing an OS/2 application.
*
\***************************************************************************/

#define OS2_INCLUDED

#if !(defined(INCL_32) || defined(INCL_16))
#ifdef M_I386
    #define INCL_32
#else /* not M_I386 */
    #define INCL_16
#endif /* M_I386 */
#endif /* INCL_32 || INCL_16 */

/* XLATOFF */
#if (defined(INCL_32) && defined(INCL_16))
#error message ("Illegal combination of API Flags - 32 && 16")
#endif /* INCL_32 && INCL_16 */
/* XLATON */

/* Common definitions */

#include <os2def.h>

/* OS/2 Base Include File */

#ifndef INCL_NOBASEAPI
#include <bse.h>
#endif /* INCL_NOBASEAPI */

/* OS/2 Presentation Manager Include File */

#ifndef INCL_NOPMAPI
#include <pm.h>
#endif /* INCL_NOPMAPI */
