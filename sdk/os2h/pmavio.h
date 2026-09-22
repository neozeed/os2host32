/***************************************************************************\
*
*   Copyright (c) IBM Corporation 1987, 1989
*   Copyright (c) MicroSoft Corporation 1987,1989
*
* Module Name: PMAVIO.H
*
* OS/2 Presentation Manager AVIO constants, types and function declarations
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

#ifdef INCL_16

/* XLATOFF */
    #define VioAssociate Vio16Associate
    #define VioCreateLogFont Vio16CreateLogFont
    #define VioCreatePS Vio16CreatePS
    #define VioDeleteSetId Vio16DeleteSetId
    #define VioDestroyPS Vio16DestroyPS
    #define VioGetDeviceCellSize Vio16GetDeviceCellSize
    #define VioGetOrg Vio16GetOrg
    #define VioQueryFonts Vio16QueryFonts
    #define VioQuerySetIds Vio16QuerySetIds
    #define VioSetDeviceCellSize Vio16SetDeviceCellSize
    #define VioSetOrg Vio16SetOrg
    #define VioShowPS Vio16ShowPS
    #define WinDefAVioWindowProc Win16DefAVioWindowProc
/* XLATON */

typedef USHORT HVPS;        /* hpvs */
typedef HVPS FAR *PHVPS;    /* phpvs */

USHORT  APIENTRY VioAssociate(HDC hdc, HVPS hvps);
USHORT  APIENTRY VioCreateLogFont(PFATTRS pfatattrs, LONG llcid, PSTR8 pName, HVPS hvps);
USHORT  APIENTRY VioCreatePS(PHVPS phvps, SHORT sdepth, SHORT swidth
                            , SHORT sFormat, SHORT sAttrs, HVPS hvpsReserved);
USHORT  APIENTRY VioDeleteSetId(LONG llcid, HVPS hvps);
USHORT  APIENTRY VioDestroyPS(HVPS hvps);
USHORT  APIENTRY VioGetDeviceCellSize(PSHORT psHeight, PSHORT psWidth, HVPS hvps);
USHORT  APIENTRY VioGetOrg(PSHORT psRow, PSHORT psColumn, HVPS hvps);
USHORT  APIENTRY VioQueryFonts(PLONG plRemfonts, PFONTMETRICS afmMetrics
                              , LONG lMetricsLength, PLONG plFonts
                              , PSZ pszFacename, ULONG flOptions, HVPS hvps);
USHORT  APIENTRY VioQuerySetIds(PLONG allcids, PSTR8 pNames
                               , PLONG alTypes, LONG lcount, HVPS hvps);
USHORT  APIENTRY VioSetDeviceCellSize(SHORT sHeight, SHORT sWidth, HVPS hvps);
USHORT  APIENTRY VioSetOrg(SHORT sRow, SHORT sColumn, HVPS hvps);
USHORT  APIENTRY VioShowPS(SHORT sDepth, SHORT sWidth, SHORT soffCell, HVPS hvps);

/************************ Public Function ******************************\
 * WinDefAVioWindowProc -- Default message processing for AVio PS's
\***********************************************************************/

MRESULT APIENTRY WinDefAVioWindowProc(HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2);

#endif /* INCL_16 */
