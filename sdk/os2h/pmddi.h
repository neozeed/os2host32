/*static char *SCCSID = "@(#)pmddi.h   13.13 90/04/02";*/
/*******************************Module*Header*******************************\
* Module Name: PMDDI.H							    *
*									    *
* Include file that declares entry points, macros, and types for the	    *
* Graphics Engine.							    *
*									    *
* Version 1.10 Copyright International Business Machines Corp. 1981, 1988   *
* Version 1.10 Copyright Microsoft Corp. 1981, 1988			    *
*									    *
*===========================================================================*
*									    *
* The following symbols are used in this file for conditional sections:	    *
*									    *
*     INCL_DDIGRE   - if you don't want OS2DEF, PMGPI, and PMDEV included   *
*     INCL_DDIDEFS  - if you don't want the above, but do want GRE structs  *
*									    *
* It is expected that callers of the Engine will want to define INCL_DDIGRE *
* and possibly INCL_DDIDEFS.  The Engine itself and device drivers define   *
* neither of these.							    *
*									    *
* Further defines must be made to specify which of the GRE function macros  *
* should be defined.  INCL_GREALL causes all of them to be defined.	    *
*									    *
*     INCL_GRE_ARCS    - Arcs functions					    *
*     INCL_GRE_LINES	   - Line functions				    *
*     INCL_GRE_MARKERS	   - etc.					    *
*     INCL_GRE_SCANS							    *
*     INCL_GRE_BITMAPS							    *
*     INCL_GRE_STRINGS							    *
*     INCL_GRE_PATHS							    *
*     INCL_GRE_PICK							    *
*     INCL_GRE_CLIP							    *
*     INCL_GRE_REGIONS							    *
*     INCL_GRE_XFORMS							    *
*     INCL_GRE_DEVMISC							    *
*     INCL_GRE_COLORTABLE						    *
*     INCL_GRE_DEVICE							    *
*     INCL_GRE_DCS							    *
*     INCL_GRE_SETID							    *
*     INCL_GRE_FONTS							    *
*     INCL_GRE_JOURNALING						    *
*     INCL_GRE_LCID							    *
*     INCL_GRE_DEVSUPPORT						    *
*     INCL_GRE_PALETTE							    *
\***************************************************************************/

/* exported Engine DDI functions */

ULONG PASCAL FAR SetDriverInfo( ULONG, LHANDLE, ULONG, HDC );
ULONG PASCAL FAR GetDriverInfo( LHANDLE, ULONG, HDC );
ULONG PASCAL FAR PostDeviceModes( PDRIVDATA, PSZ, PSZ, PSZ, ULONG );
BOOL PASCAL FAR GreInitialize(VOID);

/* define common types in the Engine and DDI */

typedef struct _RECTS { /* rcs */
    POINTS pts1;
    POINTS pts2;
} RECTS;
typedef RECTS FAR *PRECTS;

typedef struct _POINTFX { /* ptfx */
    FIXED x;
    FIXED y;
} POINTFX;
typedef POINTFX FAR *PPOINTFX;

typedef struct _RECTFX { /* rcfx */
    POINTFX ptfx1;
    POINTFX ptfx2;
} RECTFX;
typedef RECTFX FAR *PRECTFX;

typedef struct _XFORM { /* xform */
    FIXED fxM11;
    FIXED fxM12;
    FIXED fxM21;
    FIXED fxM22;
    LONG  lM41;
    LONG  lM42;
} XFORM;
typedef XFORM FAR *PXFORM;

typedef LONG LCID;	/* locally-coded id */
typedef LCID FAR *PLCID;
typedef LONG PHID;	/* path id */
typedef ULONG HDEVPAL;

/* get GRE function macros */
/* have INCL_GREALL defined to get all of these */

#ifdef INCL_GREALL
#define INCL_GRE_ARCS
#define INCL_GRE_LINES
#define INCL_GRE_MARKERS
#define INCL_GRE_SCANS
#define INCL_GRE_BITMAPS
#define INCL_GRE_STRINGS
#define INCL_GRE_PATHS
#define INCL_GRE_PICK
#define INCL_GRE_CLIP
#define INCL_GRE_REGIONS
#define INCL_GRE_XFORMS
#define INCL_GRE_DEVMISC
#define INCL_GRE_COLORTABLE
#define INCL_GRE_DEVICE
#define INCL_GRE_DCS
#define INCL_GRE_SETID
#define INCL_GRE_FONTS
#define INCL_GRE_JOURNALING
#define INCL_GRE_LCID
#define INCL_GRE_DEVSUPPORT
#define INCL_GRE_PALETTE
#endif /* INCL_GREALL */

#ifdef INCL_GRE_DEVMISC
#define INCL_GRE_DEVMISC1
#define INCL_GRE_DEVMISC2
#define INCL_GRE_DEVMISC3
#endif	/* INCL_GRE_DEVMISC */

/* Command Flags for high word of FunN */

#ifdef INCL_DDICOMFLAGS
#define COM_DRAW    0x0001
#define COM_BOUND   0x0002
#define COM_CORRELATE	0x0004
#define COM_ALT_BOUND	0x0008
#define COM_AREA    0x0010
#define COM_PATH    0x0020
#define COM_TRANSFORM	0x0040
#define COM_RECORDING	0x0080
#define COM_DEVICE  0x0100
#endif /* INCL_DDICOMFLAGS */

#ifdef INCL_GRE_ARCS

/* BoxBoundary */
/* BoxInterior */
/* BoxBoth */

typedef struct _BOXPARAMS {    /* boxp */
    POINTL ptl;
    SIZEL  sizl;
} BOXPARAMS;
typedef BOXPARAMS FAR *PBOXPARAMS;
#endif /* INCL_GRE_ARCS */

#ifdef INCL_GRE_CLIP

/* CopyClipRegion */

#define COPYCRGN_ALLINTERSECT	0L
#define COPYCRGN_VISRGN	    1L
#define COPYCRGN_CLIPRGN    2L

/* SetupDC */

#define SETUPDC_VISRGN		0x00000001L
#define SETUPDC_ORIGIN		0x00000002L
#define SETUPDC_ACCUMBOUNDSON	0x00000004L
#define SETUPDC_ACCUMBOUNDSOFF	0x00000008L
#define SETUPDC_RECALCCLIP	0x00000010L
#define SETUPDC_SETOWNER	0x00000020L
#define SETUPDC_CLEANDC		0x00000040L

#endif /* INCL_GRE_CLIP */

#ifdef INCL_GRE_XFORMS
/* QueryViewportSize */

typedef struct _VIEWPORTSIZE { /* vs */
    ULONG cx;
    ULONG cy;
} VIEWPORTSIZE;
typedef VIEWPORTSIZE FAR *PVIEWPORTSIZE;

#endif /* INCL_GRE_XFORMS */

#ifdef INCL_GRE_DEVSUPPORT

/* Constants for GreInitializeAttributes */

#define INAT_DEFAULTATTRIBUTES	1L
#define INAT_CURRENTATTRIBUTES	2L

/* InvalidateVisRegion */

typedef struct _DC_BLOCK { /* ivr */
    ULONG hdc;
    ULONG hddc;
} DC_BLOCK;
typedef DC_BLOCK FAR *PDC_BLOCK;

#endif /* INCL_GRE_DEVSUPPORT */

#ifdef INCL_DDIMISC

/* Display information resource structure (RT_DISPLAYINFO) */

typedef struct _DISPLAYINFO {	 /* dspinfo */
    USHORT cb;
    SHORT cxIcon;
    SHORT cyIcon;
    SHORT cxPointer;
    SHORT cyPointer;
    SHORT cxBorder;
    SHORT cyBorder;
    SHORT cxHSlider;
    SHORT cyVSlider;
    SHORT cxSizeBorder;
    SHORT cySizeBorder;
    SHORT cxDeviceAlign;
    SHORT cyDeviceAlign;
} DISPLAYINFO;
typedef DISPLAYINFO FAR *PDISPLAYINFO;

/* Parameters for the DC Enable function */

typedef struct _DENPARAMS { /* den */
    ULONG ulStateInfo;
    ULONG ulType;
    ULONG ulHDC;
} DENPARAMS;
typedef DENPARAMS FAR *PDENPARAMS;


typedef struct _STYLERATIO { /* sr */
    BYTE dx;
    BYTE dy;
} STYLERATIO;
typedef STYLERATIO FAR *PSTYLERATIO;

/* Options flags for SetGlobalAttribute */

#define GATTR_DEFAULT		   1L

/* Attribute Types for SetGlobalAttribute */

#define ATYPE_COLOR	       1L
#define ATYPE_BACK_COLOR	   2L
#define ATYPE_MIX_MODE		   3L
#define ATYPE_BACK_MIX_MODE	   4L

/* Options for CharStringPos */

#define CHS_START_XY	       0x00000020L
#define CHS_ATTR_INFO	       0x00000040L

typedef struct _CSP_INFO { /* csp */
    LONG  cSize;
    LONG  lColor;
    LONG  lBackColor;
} CSP_INFO;
typedef CSP_INFO FAR *PCSP_INFO;

/* Set/GetProcessControl */

#define PCTL_DRAW	   0x00000001L
#define PCTL_BOUND	   0x00000002L
#define PCTL_CORRELATE	       0x00000004L
#define PCTL_USERBOUNDS	       0x00000008L
#define PCTL_AREA	   0x00000010L

/* ResetBounds */

#define RB_GPI		   0x00000001L
#define RB_USER		   0x00000002L

/* GetBoundsData */

#define GBD_GPI		    0L
#define GBD_USER	    1L

/* EndArea Cancel Option */

#define EA_DRAW		    0x00000000L
#define EA_CANCEL	    0x00000001L

/* Bitblt Style */

#define BLTMODE_SRC_BITMAP	     0x00010000L
#define BLTMODE_ATTRS_PRES	     0x00020000L
#define BLTMODE_NO_PAL_MAP	     0x00080000L
#define BBO_TARGWORLD		     0x00000100L

typedef struct _BITBLTPARAMETERS { /* bbp */
    RECTL rclTarg;
    RECTL rclSrc;
} BITBLTPARAMETERS;
typedef BITBLTPARAMETERS FAR *PBITBLTPARAMETERS;

typedef struct _BITBLTATTRS { /* bba */
    LONG cSize;
    LONG lColor;
    LONG lBackColor;
} BITBLTATTRS;
typedef BITBLTATTRS FAR *PBITBLTATTRS;

/* LCIDs */

#define LCID_AVIO_1	       (-2L)
#define LCID_AVIO_2	       (-3L)
#define LCID_AVIO_3	       (-4L)

#define LCID_RANGE_GPI		1L
#define LCID_RANGE_AVIO		2L
#define LCID_RANGE_BOTH		3L
#define LCID_GRAPHICS_MIN	1
#define LCID_GRAPHICS_MAX	254

#define LCIDT_NONE               0L

/* ResetDC */

#define RDC_RGBMODE		0x1L
#define RDC_SETOWNERTOSHELL	0x2L

/* SetRandomXform */

#define SX_UNITY	    0L
#define SX_CAT_AFTER		1L
#define SX_CAT_BEFORE		2L
#define SX_OVERWRITE		3L

/* transform accelerators			*/
/*  These bits are only valid if the MATRIX_SIMPLE bit is set.	    */
/*  The X and Y negate flags are only meaningful if MATRIX_UNITS is set.*/

#define MATRIX_SIMPLE	    0x0001L	/* two entries are zero */
#define MATRIX_UNITS	    0x0002L	/* all entries are +1 or -1 */
#define MATRIX_XY_EXCHANGE  0x0004L	/* zeros are on the diagonal*/
#define MATRIX_X_NEGATE	    0x0008L	/* X is hit by negative */
#define MATRIX_Y_NEGATE	    0x0010L	/* Y is hit by negative */
#define MATRIX_TRANSLATION  0x0020L	/* non-zero translation */

/* NotifyClipChange */

#define NCC_CLEANDC	    0x0002L	/* clear DC dirty bit */

/* NotifyTransformChange */

typedef struct _NOTIFYTRANSFORMDATA { /* ntd */
    USHORT usType;
    XFORM  xform;
} NOTIFYTRANSFORMDATA;
typedef NOTIFYTRANSFORMDATA FAR *PNOTIFYTRANSFORMDATA;


/* ColorTable */

#define LCOL_SYSCOLORS	    0x0010L


/* query device caps */

typedef struct _QCDARRAY { /* qcd */
    LONG    iFormat;
    LONG    iSmallest;
    LONG    iLargest;
    LONG    cAvailable;
    LONG    cSpecifiable;
    LONG    iMax;
} QCDARRAY;
typedef QCDARRAY FAR *PQCDARRAY;

#define CAPS_MIX_OR	   0x00000001L
#define CAPS_MIX_COPY	       0x00000002L
#define CAPS_MIX_UNDERPAINT    0x00000004L
#define CAPS_MIX_XOR	       0x00000008L
#define CAPS_MIX_INVISIBLE     0x00000010L
#define CAPS_MIX_AND	       0x00000020L
#define CAPS_MIX_OTHER	       0x00000040L

#define CAPS_DEV_FONT_SIM_BOLD		1L	// for CAPS_DEVICE_FONT_SIM
#define CAPS_DEV_FONT_SIM_ITALIC	2L
#define CAPS_DEV_FONT_SIM_UNDERSCORE	4L
#define CAPS_DEV_FONT_SIM_STRIKEOUT	8L

#define CAPS_BACKMIX_OR	    0x00000001L
#define CAPS_BACKMIX_COPY   0x00000002L
#define CAPS_BACKMIX_UNDERPAINT 0x00000004L
#define CAPS_BACKMIX_XOR    0x00000008L
#define CAPS_BACKMIX_INVISIBLE	0x00000010L


/*#define CAPS_RASTER_BITBLT	 0x00000001L	defined in pmdev.h */
/*#define CAPS_RASTER_BANDING	 0x00000002L	*/
/*#define CAPS_RASTER_STRETCHBLT 0x00000004L	*/
/*#define CAPS_RASTER_SETPEL	 0x00000010L	*/
#define CAPS_FONT_OUTLINE_MANAGE 16L
#define CAPS_FONT_IMAGE_MANAGE	 32L
#define SFONT_RASTER	     100
#define SFONT_OUTLINE	     101
#define FONT		1000	   /* must not conflict with RT_XXX */
		       /* constants in BSEDOS.H */

/* DCCaps */

#define DCCAPS_LINE	   0x0100
#define DCCAPS_CURVE	       0x0200
#define DCCAPS_AREA	   0x0400
#define DCCAPS_MARKER	       0x0800
#define DCCAPS_TEXT	   0x1000

/* DeviceDeleteBitmap */
#define BITMAP_USAGE_TRANSLATE	0x0004

/* DeleteBitmap return structure */
typedef struct _DELETERETURN { /* dr */
    ULONG      pInfo;
    ULONG      pBits;
} DELETERETURN;
typedef DELETERETURN FAR *PDELETERETURN;

/* Short Line Header */

#define SLH_FORMAT_IS_16_DOT_16 1
#define PSL_YMAJOR 0x8000	/* bit mask for usStyle */

typedef struct _SHORTLINEHEADER { /* slh */
    USHORT     usStyle;
    USHORT     usFormat;
    POINTS     ptsStart;
    POINTS     ptsStop;
    SHORT      sxLeft;
    SHORT      sxRight;
    struct _SHORTLINEHEADER FAR * pslhNext;
    struct _SHORTLINEHEADER FAR * pslhPrev;
} SHORTLINEHEADER;
typedef SHORTLINEHEADER FAR *PSHORTLINEHEADER;

/* Short Line */

typedef struct _SHORTLINE { /* sl */
    SHORTLINEHEADER slh;
    SHORT	ax[1];
} SHORTLINE;
typedef SHORTLINE FAR *PSHORTLINE;

typedef struct _SCANDATA { /* sd */
    PSHORTLINE pslFirstLeft;
    PSHORTLINE pslLastLeft;
    PSHORTLINE pslFirstRight;
    PSHORTLINE pslLastRight;
    ULONG      c;
    RECTL      rclBound;
} SCANDATA;
typedef SCANDATA FAR *PSCANDATA;

/* Index for Set/GetDriverInfo */

#define DI_HDC	       0x00000000L
#define DI_HBITMAP     0x00000001L

#endif	/* INCL_DDIMISC */

#ifdef INCL_DDIMISC2

/* RealizeFont */

#define REALIZE_FONT		1   /* To be removed */
#define REALIZE_ENGINE_FONT	2
#define DELETE_FONT		3

#define RF_DEVICE_FONT		1
#define RF_LOAD_ENGINE_FONT	2
#define RF_DELETE_FONT		3
#define RF_DELETE_ENGINE_FONT	4

#endif	/* INCL_DDIMISC2 */

#ifdef INCL_DDIBUNDLES

/* Device Line Bundle */

typedef struct _LINEDEFS { /* ldef */
    ULONG      defType;
} LINEDEFS;
typedef LINEDEFS FAR *PLINDEFS;

typedef struct _DLINEBUNDLE { /* dlbnd */
    SHORT      cAttr;
    SHORT      cDefs;
    LINEBUNDLE lbnd;
    LINEDEFS   ldef;
} DLINEBUNDLE;
typedef DLINEBUNDLE FAR *PDLINEBUNDLE;

/* Device Area Bundle */

typedef struct _AREADEFS { /* adef */
    ULONG      defSet;
    UINT       fFlags;
    UINT       CodePage;
} AREADEFS;
typedef AREADEFS FAR *PAREADEFS;

typedef struct _DAREABUNDLE { /* dabnd */
    SHORT      cAttr;
    SHORT      cDefs;
    AREABUNDLE abnd;
    AREADEFS   adef;
} DAREABUNDLE;
typedef DAREABUNDLE FAR *PDAREABUNDLE;

/* Device Character Bundle */

typedef struct _CHARDEFS { /* cdef */
    ULONG      defSet;
    UINT       fFlags;
    UINT       CodePage;
    UINT       charSpacing;
} CHARDEFS;
typedef CHARDEFS FAR *PCHARDEFS;

typedef struct _DCHARBUNDLE { /* dcbnd */
    SHORT      cAttr;
    SHORT      cDefs;
    CHARBUNDLE cbnd;
    CHARDEFS   cdef;
} DCHARBUNDLE;
typedef DCHARBUNDLE FAR *PDCHARBUNDLE;

/* Device Image Bundle */

#ifdef BOGUS
typedef struct _IMAGEDEFS { /* idef */
} IMAGEDEFS;
#endif	/* BOGUS */

typedef struct _DIMAGEBUNDLE { /* dibnd */
    SHORT      cAttr;
    SHORT      cDefs;
    IMAGEBUNDLE ibnd;
/*    IMAGEDEFS	  idef; */
} DIMAGEBUNDLE;
typedef DIMAGEBUNDLE FAR *PDIMAGEBUNDLE;

/* Device Marker Bundle */

typedef struct _MARKERDEFS { /* mdef */
    ULONG      defSet;
    UINT       fFlags;
    UINT       CodePage;
} MARKERDEFS;
typedef MARKERDEFS FAR *PMARKERDEFS;

typedef struct _DMARKERBUNDLE { /* dmbnd */
    SHORT      cAttr;
    SHORT      cDefs;
    MARKERBUNDLE mbnd;
    MARKERDEFS	 mdef;
} DMARKERBUNDLE;
typedef DMARKERBUNDLE FAR *PDMARKERBUNDLE;


#endif /* INCL_DDIBUNDLES */

/* This stuff should go away soon.  use INCL_FONTFILEFORMAT and include os2.h */
#ifdef INCL_DDIFONTSTRUCS
#include <pmfont.h>
#endif /* INCL_DDIFONTSTRUCS */

/* This stuff should go away soon.  use INCL_BITMAPFILEFORMAT and include os2.h */
#ifdef INCL_DDIBITMAPFILE
#include <pmbitmap.h>
#endif /* INCL_DDIBITMAPFILE */

#ifdef INCL_DDIPATHS

/* Signatures of Path data structures */

#define CURVE_IDENTIFIER	0x43
#define SUBPATH_IDENTIFIER	0x53
#define PATH_IDENTIFIER		0x50
#define PATHSEGMENT_IDENTIFIER	0x5350

/* Curve types */

#define LINE_IDENTIFIER		0x4C
#define FILLET_SHARP_IDENTIFIER 0x46
#define FILLET_EQN_IDENTIFIER	0x45
#define CURVEATTR_IDENTIFIER	0x41

/* Subpath types */

#define SUBPATH_CLOSED		0x43
#define SUBPATH_OPEN		0x4F

/* Path types */

#define BEGINAREA_IDENTIFIER	    0x41
#define BEGINPATH_IDENTIFIER	    0x50
#define PATHSEGMENT_FORMAT_16_16    1

/* Flags for curve data structures */

#define CURVE_FIRST_IN_SUBPATH	    0x0001
#define CURVE_DO_FIRST_PEL	0x0002
#define CURVE_GOES_UP		0x0004
#define CURVE_IS_HORIZONTAL	0x0008
#define CURVE_IS_X_MAJOR	0x0010
#define CURVE_GOES_LEFT		0x0020
#define CURVE_FIRST_CARVED	0x0040
#define CURVE_HALF_COOKED	0x0400
#define CURVE_IS_ABOVE_OR_RIGHT 0x0800
/* Flags for SubPath data structures */

#define SUBPATH_DO_FIRST_PEL	    0x0002

/* Flags for Path data structures */

#define PATH_HAS_LINES_PRESENT	    0x4000
#define PATH_HAS_CONICS_PRESENT	    0x8000

/* Data structures to support the Path API */

typedef struct _CURVE { /* cv */
    BYTE    bIdent;
    BYTE    bType;
    USHORT  usStyle;
    USHORT  fs;
    struct  _CURVE NEAR *npcvNext;
    struct  _CURVE NEAR *npcvPrev;
    struct  _CURVE NEAR *npcvAttrs;
    POINTFX ptfxA;
    POINTFX ptfxC;
    BYTE    Reserved2[16];
} CURVE;
typedef CURVE FAR *PCURVE;
typedef CURVE NEAR *NPCURVE;

typedef struct _LINE { /* ln */
    BYTE    bIdent;
    BYTE    bType;
    USHORT  usStyle;
    USHORT  fs;
    CURVE   NEAR *npcvNext;
    CURVE   NEAR *npcvPrev;
    CURVE   NEAR *npcvAttrs;
    POINTFX ptfxA;
    POINTFX ptfxC;
    POINTS  ptsA;
    POINTS  ptsC;
    FIXED   lRslope;
    BYTE    Reserved2[4];
} LINE;
typedef LINE FAR *PLINE;
typedef LINE NEAR *NPLINE;

typedef struct _FILLETEQN { /* fse */
    BYTE    bIdent;
    BYTE    bType;
    USHORT  usReferences;
    POINTS  ptsA;
    POINTS  ptsC;
    POINTS  ptsB;
    USHORT  usNumerator;
    USHORT  usDenominator;
    LONG    lAlpha;
    LONG    lBeta;
    LONG    lGamma;
    LONG    lDelta;
    LONG    lEpsilon;
    LONG    lZeta;
} FILLETEQN;
typedef FILLETEQN FAR *PFILLETEQN;
typedef FILLETEQN NEAR *NPFILLETEQN;

typedef struct _FILLETSHARP { /* fs */
    BYTE    bIdent;
    BYTE    bType;
    USHORT  usStyle;
    USHORT  fs;
    CURVE   NEAR *npcvNext;
    CURVE   NEAR *npcvPrev;
    CURVE   NEAR *npcvAttrs;
    POINTFX ptfxA;
    POINTFX ptfxC;
    POINTFX ptfxB;
    FIXED   lSharpness;
    FILLETEQN NEAR *npEquation;
    BYTE    Reserved2[2];
} FILLETSHARP;
typedef FILLETSHARP FAR *PFILLETSHARP;
typedef FILLETSHARP NEAR *NPFILLETSHARP;

#ifdef INCL_GPIPRIMITIVES
typedef struct _CURVEATTR { /* cva */
    BYTE	bIdent;
    BYTE	bType;
    ULONG	flAttrs;	/* which fields have new info */
    BYTE	Reserved1[4];
    CURVE	NEAR *npcvAttrs;
    ULONG	flDefs;		/* which fields have default info */
    LINEBUNDLE	lbnd;		/* the info */
    BYTE	Reserved2[2];
} CURVEATTR;
typedef CURVEATTR FAR *PCURVEATTR;
typedef CURVEATTR NEAR *NPCURVEATTR;
#endif /* INCL_GPIPRIMITIVES */

typedef struct _SUBPATH { /* sp */
    BYTE    bIdent;
    BYTE    bType;
    USHORT  usStyle;
    USHORT  fs;
    struct  _SUBPATH NEAR *npspNext;
    struct  _SUBPATH NEAR *npspPrev;
    USHORT  ccv;
    ULONG   flFlags;
    CURVE   NEAR *npcvFirst;
    CURVE   NEAR *npcvLast;
    RECTS   rcsBounding;
    CURVE   NEAR *npcvAttrs;
    BYTE    Reserved1[14];
} SUBPATH;
typedef SUBPATH FAR *PSUBPATH;
typedef SUBPATH NEAR *NPSUBPATH;

#define PH_FORMAT_IS_16_DOT_16	1

typedef struct _PATH { /* ph */
    BYTE    bIdent;
    BYTE    bType;
    USHORT  usFormat;
    USHORT  usStyle;
    USHORT  fs;
    SUBPATH NEAR *npspFirst;
    SUBPATH NEAR *npspLast;
    USHORT  csp;
    ULONG   flFlags;
    USHORT  usDimension;
    BYTE    bSubpathType;
    LONG    alColor;
    USHORT  ausMixMode;
    USHORT  ausDefault;
    POINTL  aptlRefPoint ;
    CURVE   NEAR *npcvAttrs;
    BYTE    Reserved1[7];
} PATH;
typedef PATH FAR *PPATH;
typedef PATH NEAR *NPPATH;

typedef struct _PATHSEGMENT { /* phs */
    USHORT  usIdent;
	 SHORT	Reserved0;
    CURVE   NEAR *npcvFree;
    USHORT  ccvFree;
    USHORT  cReferences;
    USHORT  usSize;
    PATH    NEAR *npph;
    BYTE    Reserved1[2];
    FSRSEM  fsrs;
} PATHSEGMENT;
typedef PATHSEGMENT FAR *PPATHSEGMENT;
typedef PATHSEGMENT NEAR *NPPATHSEGMENT;

/* Argument to DrawCookedPath, etc. */

typedef struct _PIPELINEINFO { /* pi */
    CURVE   FAR *pcv;
    ULONG   ccv;
} PIPELINEINFO;
typedef PIPELINEINFO FAR *PPIPELINEINFO;
typedef PIPELINEINFO NEAR *NPPIPELINEINFO;
#endif	/* INCL_DDIPATHS */

#ifdef INCL_GRE_JOURNALING
#define JNL_TEMP_FILE	    0x00000001L
#define JNL_PERM_FILE	    0x00000002L
#define JNL_ENGINERAM_FILE  0x00000004L
#define JNL_USERRAM_FILE    0x00000008L
#define JNL_DRAW_OPTIMIZATION	0x00000010L
#define JNL_BOUNDS_OPTIMIZATION 0x00000020L
#endif	/* INCL_GRE_JOURNALING */


#ifdef INCL_GRE_DEVICE

/* QueryDeviceBitmaps */

typedef struct _BITMAPFORMAT { /* bmf */
    ULONG cPlanes;
    ULONG cBitCount;
} BITMAPFORMAT;
typedef BITMAPFORMAT FAR *PBITMAPFORMAT;

#endif /* INCL_GRE_DEVICE */

#ifdef INCL_GRE_PALETTE

typedef struct _PALETTEINFOHEADER { /* palinfohdr */
	ULONG  flCmd;	      // options from creation
	ULONG  ulFormat;      // specifies format of entries at creation
	LONG   lStart;	      // starting index from creation
	LONG   clColorData;   // number of elements supplied at creation
} PALETTEINFOHEADER;
typedef PALETTEINFOHEADER NEAR *NPPALETTEINFOHEADER;
typedef PALETTEINFOHEADER FAR  *PPALETTEINFOHEADER;

typedef struct _PALETTEINFO { /* palinfo */
	ULONG  flCmd;	      // options from creation
	ULONG  ulFormat;      // specifies format of entries at creation
	LONG   lStart;	      // starting index from creation
	LONG   clColorData;   // number of elements supplied at creation
	RGB2   argb[1];       // the palette entries
} PALETTEINFO;
typedef PALETTEINFO NEAR *NPPALETTEINFO;
typedef PALETTEINFO FAR  *PPALETTEINFO;

/* flType values for RealizePalette */
#define RP_FOREGROUND		   1
#define RP_FULL_DEFAULT_MODE	   2
#define RP_RESTRICTED_DEFAULT_MODE 4

#endif

#ifdef INCL_GRE_BITMAPS

#define LR_CLIPPED   2
#define LR_NOTBORDER 0
#define LR_BORDER    1
#define LR_LEFT      2
#define LR_RIGHT     4

#endif

#include <pmddim.h>
