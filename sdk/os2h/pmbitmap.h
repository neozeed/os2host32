/***************************************************************************\
*
* Module Name: PMBITMAP.H
*
* OS/2 Presentation Manager Bit Map, Icon and Pointer type declarations.
*
* Copyright (c) International Business Machines Corporation 1981, 1988, 1989
* Copyright (c) Microsoft Corporation 1981, 1988, 1989
*
\***************************************************************************/

/* XLATOFF */
#ifndef BITMAPS_INCLUDED    /* multiple include protection */

#pragma pack(2)      /* pack on wordboundary */
/* XLATON */

#define BITMAPS_INCLUDED

/* rastor operations defined for GpiBitBlt */
#define ROP_SRCCOPY                0x00CCL
#define ROP_SRCPAINT               0x00EEL
#define ROP_SRCAND                 0x0088L
#define ROP_SRCINVERT              0x0066L
#define ROP_SRCERASE               0x0044L
#define ROP_NOTSRCCOPY             0x0033L
#define ROP_NOTSRCERASE            0x0011L
#define ROP_MERGECOPY              0x00C0L
#define ROP_MERGEPAINT             0x00BBL
#define ROP_PATCOPY                0x00F0L
#define ROP_PATPAINT               0x00FBL
#define ROP_PATINVERT              0x005AL
#define ROP_DSTINVERT              0x0055L
#define ROP_ZERO                   0x0000L
#define ROP_ONE                    0x00FFL

/* Blt options for GpiBitBlt */
#define BBO_OR                          0L
#define BBO_AND                         1L
#define BBO_IGNORE                      2L

/* Fill options for GpiFloodFill */
#define FF_BOUNDARY                     0L
#define FF_SURFACE                      1L

/* error return for GpiSetBitmap */
#define HBM_ERROR            ((HBITMAP)-1L)

#ifndef INCL_DDIDEFS

/*** bitmap and pel functions */
/* XLATOFF */
#ifdef INCL_16
    #define GpiBitBlt Gpi16BitBlt
    #define GpiDeleteBitmap Gpi16DeleteBitmap
    #define GpiLoadBitmap Gpi16LoadBitmap
    #define GpiSetBitmap Gpi16SetBitmap
    #define GpiWCBitBlt Gpi16WCBitBlt
#endif /* INCL_16 */
/* XLATON */
LONG     APIENTRY GpiBitBlt( HPS hpsTarget, HPS hpsSource, LONG lCount
                           , PPOINTL aptlPoints, LONG lRop, ULONG flOptions );
BOOL     APIENTRY GpiDeleteBitmap( HBITMAP hbm );
HBITMAP  APIENTRY GpiLoadBitmap( HPS hps, HMODULE Resource, USHORT idBitmap
                               , LONG lWidth, LONG lHeight );
HBITMAP  APIENTRY GpiSetBitmap(HPS hps, HBITMAP hbm );
LONG     APIENTRY GpiWCBitBlt( HPS hpsTarget, HBITMAP hbmSource, LONG lCount
                             , PPOINTL aptlPoints, LONG lRop, ULONG flOptions );

#endif /* no INCL_DDIDEFS */

#ifdef INCL_GPIBITMAPS

/* usage flags for GpiCreateBitmap */
#define CBM_INIT        0x0004L

/* bitmap parameterization used by GpiCreateBitmap and others */
typedef struct _BITMAPINFOHEADER {      /* bmp */
    ULONG  cbFix;
    USHORT cx;
    USHORT cy;
    USHORT cPlanes;
    USHORT cBitCount;
} BITMAPINFOHEADER;
typedef BITMAPINFOHEADER FAR *PBITMAPINFOHEADER;

/* RGB data for _BITMAPINFO struct */
typedef struct _RGB {           /* rgb */
    BYTE bBlue;
    BYTE bGreen;
    BYTE bRed;
} RGB;

/* bitmap data used by GpiSetBitmapBits and others */
typedef struct _BITMAPINFO {    /* bmi */
    ULONG  cbFix;
    USHORT cx;
    USHORT cy;
    USHORT cPlanes;
    USHORT cBitCount;
    RGB    argbColor[1];
} BITMAPINFO;
typedef BITMAPINFO FAR *PBITMAPINFO;

#define     BCA_UNCOMP      0L

#define     BRU_METRIC      0L

#define     BRA_BOTTOMUP    0L

#define     BRH_NOTHALFTONED    0L
#define     BRH_ERRORDIFFUSION  1L
#define     BRH_PANDA           2L
#define     BRH_SUPERCIRCLE     3L

#define     BCE_RGB         0L

typedef struct _BITMAPINFOHEADER2 {     /* bmp2  */
    ULONG  cbFix;            /* Length of structure                       */
    ULONG  cx;               /* Bit-map width in pels                     */
    ULONG  cy;               /* Bit-map height in pels                    */
    USHORT cPlanes;          /* Number of bit planes                      */
    USHORT cBitCount;        /* Number of bits per pel within a plane     */
    ULONG  ulCompression;    /* Compression scheme used to store the bitmap */
    ULONG  cbImage;          /* Length of bit-map storage data in bytes   */
    ULONG  cxResolution;     /* x resolution of target device             */
    ULONG  cyResolution;     /* y resolution of target device             */
    ULONG  cclrUsed;         /* Number of color indices used              */
    ULONG  cclrImportant;    /* Number of important color indices         */
    USHORT usUnits;          /* Units of measure                          */
    USHORT usReserved;       /* Reserved                                  */
    USHORT usRecording;      /* Recording algorithm                       */
    USHORT usRendering;      /* Halftoning algorithm                      */
    ULONG  cSize1;           /* Size value 1                              */
    ULONG  cSize2;           /* Size value 2                              */
    ULONG  ulColorEncoding;  /* Color encoding                            */
    ULONG  ulIdentifier;     /* Reserved for application use              */
} BITMAPINFOHEADER2;
typedef BITMAPINFOHEADER2 FAR *PBITMAPINFOHEADER2;

typedef struct _RGB2 {           /* rgb2 */
    BYTE bBlue;                 /* Blue component of the color definition */
    BYTE bGreen;                /* Green component of the color definition*/
    BYTE bRed;                  /* Red component of the color definition  */
    BYTE fcOptions;             /* Reserved, must be zero                 */
} RGB2;
typedef RGB2 FAR *PRGB2;

typedef struct _BITMAPINFO2 {   /* bmi2 */
    ULONG  cbFix;            /* Length of fixed portion of structure      */
    ULONG  cx;               /* Bit-map width in pels                     */
    ULONG  cy;               /* Bit-map height in pels                    */
    USHORT cPlanes;          /* Number of bit planes                      */
    USHORT cBitCount;        /* Number of bits per pel within a plane     */
    ULONG  ulCompression;    /* Compression scheme used to store the bitmap */
    ULONG  cbImage;          /* Length of bit-map storage data in bytes   */
    ULONG  cxResolution;     /* x resolution of target device             */
    ULONG  cyResolution;     /* y resolution of target device             */
    ULONG  cclrUsed;         /* Number of color indices used              */
    ULONG  cclrImportant;    /* Number of important color indices         */
    USHORT usUnits;          /* Units of measure                          */
    USHORT usReserved;       /* Reserved                                  */
    USHORT usRecording;      /* Recording algorithm                       */
    USHORT usRendering;      /* Halftoning algorithm                      */
    ULONG  cSize1;           /* Size value 1                              */
    ULONG  cSize2;           /* Size value 2                              */
    ULONG  ulColorEncoding;  /* Color encoding                            */
    ULONG  ulIdentifier;     /* Reserved for application use              */
} BITMAPINFO2;
typedef BITMAPINFO2 FAR *PBITMAPINFO2;

/* error return code for GpiSet/QueryBitmapBits */
#define BMB_ERROR                     (-1L)

#ifndef INCL_DDIDEFS

/*** bitmap and pel functions */
/* XLATOFF */
#ifdef INCL_16
    #define GpiCreateBitmap Gpi16CreateBitmap
    #define GpiSetBitmapBits Gpi16SetBitmapBits
    #define GpiSetBitmapDimension Gpi16SetBitmapDimension
    #define GpiSetBitmapId Gpi16SetBitmapId
    #define GpiQueryBitmapBits Gpi16QueryBitmapBits
    #define GpiQueryBitmapDimension Gpi16QueryBitmapDimension
    #define GpiQueryBitmapHandle Gpi16QueryBitmapHandle
    #define GpiQueryBitmapParameters Gpi16QueryBitmapParameters
    #define GpiQueryBitmapInfoHeader Gpi16QueryBitmapInfoHeader
    #define GpiQueryDeviceBitmapFormats Gpi16QueryDeviceBitmapFormats
    #define GpiSetPel Gpi16SetPel
    #define GpiQueryPel Gpi16QueryPel
    #define GpiFloodFill Gpi16FloodFill
    #define GpiDrawBits Gpi16DrawBits
#endif /* INCL_16 */
/* XLATON */
HBITMAP APIENTRY GpiCreateBitmap( HPS hps, PBITMAPINFOHEADER2 pbmpNew
                                , ULONG flOptions, PBYTE pbInitData
                                , PBITMAPINFO2 pbmiInfoTable );
LONG    APIENTRY GpiSetBitmapBits( HPS hps, LONG lScanStart, LONG lScans
                                 , PBYTE pbBuffer, PBITMAPINFO2 pbmiInfoTable );
BOOL    APIENTRY GpiSetBitmapDimension( HBITMAP hbm, PSIZEL psizlBitmapDimension );
BOOL    APIENTRY GpiSetBitmapId( HPS hps, HBITMAP hbm, LONG lLcid );
LONG    APIENTRY GpiQueryBitmapBits( HPS hps, LONG lScanStart, LONG lScans
                                   , PBYTE pbBuffer, PBITMAPINFO2 pbmiInfoTable );
BOOL    APIENTRY GpiQueryBitmapDimension( HBITMAP hbm, PSIZEL psizlBitmapDimension );
HBITMAP APIENTRY GpiQueryBitmapHandle( HPS hps, LONG lLcid );
BOOL    APIENTRY GpiQueryBitmapParameters( HBITMAP hbm
                                         , PBITMAPINFOHEADER pbmpData );
BOOL    APIENTRY GpiQueryBitmapInfoHeader( HBITMAP hbm
                                         , PBITMAPINFOHEADER2 pbmpData );
BOOL    APIENTRY GpiQueryDeviceBitmapFormats( HPS hps, LONG lCount
                                            , PLONG alArray );

LONG    APIENTRY GpiSetPel( HPS hps, PPOINTL pptlPoint );
LONG    APIENTRY GpiQueryPel( HPS hps, PPOINTL pptlPoint );

LONG    APIENTRY GpiFloodFill( HPS hps, LONG lOptions, LONG lColor );
LONG    APIENTRY GpiDrawBits( HPS hps, PVOID pBits,
                              PBITMAPINFO2 pbmiInfoTable, LONG lCount,
                              PPOINTL aptlPoints, LONG lRop,
                              ULONG flOptions );

#endif /* no INCL_DDIDEFS */

/*
 * This is the file format structure for Bit Maps, Pointers and Icons
 * as stored in the resource file of a PM application.
 *
 * Notes on file format:
grepos2ch BITMAPFILEHEADER entry is immediately followed by the color table
 *   for the bit map bits it references.
 *   Icons and Pointers contain two BITMAPFILEHEADERs for each ARRAYHEADER
 *   item.  The first one is for the ANDXOR mask, the second is for the
 *   COLOR mask.  All offsets are absolute based on the start of the FILE.
 */

typedef struct _BITMAPFILEHEADER { /* bfh */
    USHORT    usType;
    ULONG     cbSize;
    SHORT     xHotspot;
    SHORT     yHotspot;
    ULONG     offBits;
    BITMAPINFOHEADER bmp;
} BITMAPFILEHEADER;
typedef BITMAPFILEHEADER FAR *PBITMAPFILEHEADER;

/*
 * This is the 1.2 device independent format header
 */
typedef struct _BITMAPARRAYFILEHEADER {    /* bafh */
    USHORT    usType;
    ULONG     cbSize;
    ULONG     offNext;
    USHORT    cxDisplay;
    USHORT    cyDisplay;
    BITMAPFILEHEADER bfh;
} BITMAPARRAYFILEHEADER;
typedef BITMAPARRAYFILEHEADER FAR *PBITMAPARRAYFILEHEADER;


typedef struct _BITMAPFILEHEADER2 { /* bfh2 */
    USHORT    usType;
    ULONG     cbSize;
    SHORT     xHotspot;
    SHORT     yHotspot;
    ULONG     offBits;
    BITMAPINFOHEADER2 bmp2;
} BITMAPFILEHEADER2;
typedef BITMAPFILEHEADER2 FAR *PBITMAPFILEHEADER2;


typedef struct _BITMAPARRAYFILEHEADER2 { /* bafh2 */
    USHORT    usType;
    ULONG     cbSize;
    ULONG     offNext;
    USHORT    cxDisplay;
    USHORT    cyDisplay;
    BITMAPFILEHEADER2 bfh2;
} BITMAPARRAYFILEHEADER2;
typedef BITMAPARRAYFILEHEADER2 FAR *PBITMAPARRAYFILEHEADER2;

/*
 * These are the identifying values that go in the usType field of the
 * BITMAPFILEHEADER(2) and BITMAPARRAYFILEHEADER(2).
 * (BFT_ => Bit map File Type)
 */
#define BFT_ICON           0x4349   /* 'IC' */
#define BFT_BMAP           0x4d42   /* 'BM' */
#define BFT_POINTER        0x5450   /* 'PT' */
#define BFT_COLORICON      0x4943   /* 'CI' */
#define BFT_COLORPOINTER   0x5043   /* 'CP' */
#define BFT_BITMAPARRAY    0x4142   /* 'BA' */

#endif /* INCL_GPIBITMAPS */

/* XLATOFF */
#pragma pack()    /* reset to default packing */

#endif /* BITMAPS_INCLUDED */
/* XLATON */
