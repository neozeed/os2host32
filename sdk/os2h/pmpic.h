/***************************************************************************\
*
*   Copyright (c) IBM Corporation 1987, 1989
*   Copyright (c) MicroSoft Corporation 1987,1989
*
* Module Name: PMPIC.H
*
* OS/2 Presentation Manager Picture function declarations
*
*
\***************************************************************************/

#if !(defined(INCL_32) || defined(INCL_16))
#ifdef M_I386
    #define INCL_32
#else /* not M_I386 */
    #define INCL_16
#endif /* M_I386 */
#endif /* INCL_32 || INCL_16 */

/* XLATOFF */
#ifdef INCL_16
    #define PicPrint Pic16Print
    #define PicIchg Pic16Ichg
#endif /* INCL_16 */
/* XLATON */

#ifdef INCL_16

/* type of picture to print */

#define PIP_MF       1L
#define PIP_PIF      2L

BOOL APIENTRY PicPrint(HAB hab, PSZ pszFilename, LONG lType, PSZ pszParams);

/* type of conversion required */

#define PIC_PIFTOMET 0L
#define PIC_SSTOFONT 2L

BOOL  APIENTRY PicIchg(HAB hab, PSZ pszFilename1, PSZ pszFilename2, LONG lType);

#endif /* INCL_16 */
