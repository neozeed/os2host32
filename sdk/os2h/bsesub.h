/*static char *SCCSID = "@(#)bsesub.h	13.5 90/03/19";*/
/***************************************************************************\
*
* Module Name: BSESUB.H
*
* OS/2 Base Include File
*
* Copyright (c) International Business Machines Corporation 1987
* Copyright (c) Microsoft Corporation 1987
*
*****************************************************************************
*
* Subcomponents marked with "+" are partially included by default
*   #define:		    To include:
*
*   INCL_KBD		    KBD
*   INCL_VIO		    VIO
*   INCL_MOU		    MOU
\***************************************************************************/

#ifdef INCL_SUB

#define INCL_KBD
#define INCL_VIO
#define INCL_MOU

#endif /* INCL_SUB */

#ifdef INCL_KBD

/* XLATOFF */
#ifdef INCL_16
#define	KbdCharIn	Kbd16CharIn
#define	KbdClose	Kbd16Close
#define	KbdDeRegister	Kbd16DeRegister
#define	KbdFlushBuffer	Kbd16FlushBuffer
#define	KbdFreeFocus	Kbd16FreeFocus
#define	KbdGetCp	Kbd16GetCp
#define	KbdGetFocus	Kbd16GetFocus
#define	KbdGetHWID	Kbd16GetHWID
#define	KbdGetStatus	Kbd16GetStatus
#define	KbdOpen		Kbd16Open
#define	KbdPeek		Kbd16Peek
#define	KbdRegister	Kbd16Register
#define	KbdSetCp	Kbd16SetCp
#define	KbdSetCustXt	Kbd16SetCustXt
#define	KbdSetFgnd	Kbd16SetFgnd
#define	KbdSetHWID	Kbd16SetHWID
#define	KbdSetStatus	Kbd16SetStatus
#define	KbdStringIn	Kbd16StringIn
#define	KbdSynch	Kbd16Synch
#define	KbdXlate	Kbd16Xlate
#endif /* INCL_16 */
/* XLATON */

typedef unsigned short	HKBD;
typedef HKBD	FAR *	PHKBD;

APIRET	APIENTRY	KbdRegister (PSZ pszModName, PSZ pszEntryPt, ULONG FunMask);

#define KR_KBDCHARIN		   0x00000001L
#define KR_KBDPEEK		   0x00000002L
#define KR_KBDFLUSHBUFFER	   0x00000004L
#define KR_KBDGETSTATUS		   0x00000008L
#define KR_KBDSETSTATUS		   0x00000010L
#define KR_KBDSTRINGIN		   0x00000020L
#define KR_KBDOPEN		   0x00000040L
#define KR_KBDCLOSE		   0x00000080L
#define KR_KBDGETFOCUS		   0x00000100L
#define KR_KBDFREEFOCUS		   0x00000200L
#define KR_KBDGETCP		   0x00000400L
#define KR_KBDSETCP		   0x00000800L
#define KR_KBDXLATE		   0x00001000L
#define KR_KBDSETCUSTXT		   0x00002000L

#define IO_WAIT			   0
#define IO_NOWAIT		   1

APIRET	APIENTRY	KbdDeRegister (void);

/* KBDKEYINFO structure, for KbdCharIn and KbdPeek */

typedef struct _KBDKEYINFO {	/* kbci */
	UCHAR	 chChar;
	UCHAR	 chScan;
	UCHAR	 fbStatus;
	UCHAR	 bNlsShift;
	USHORT	 fsState;
	ULONG	 time;
	}KBDKEYINFO;
typedef KBDKEYINFO FAR *PKBDKEYINFO;

APIRET	APIENTRY	KbdCharIn (PKBDKEYINFO pkbci, USHORT fWait, HKBD hkbd);
APIRET	APIENTRY	KbdPeek (PKBDKEYINFO pkbci, HKBD hkbd);

/* structure for KbdStringIn() */

typedef struct _STRINGINBUF {	/* kbsi */
	USHORT cb;
	USHORT cchIn;
	} STRINGINBUF;
typedef STRINGINBUF FAR *PSTRINGINBUF;

APIRET	APIENTRY	KbdStringIn (PCH pch, PSTRINGINBUF pchIn, USHORT fsWait,
			     HKBD hkbd);

APIRET	APIENTRY	KbdFlushBuffer (HKBD hkbd);

/* KBDINFO structure, for KbdSet/GetStatus */
typedef struct _KBDINFO {	/* kbst */
	USHORT cb;
	USHORT fsMask;
	USHORT chTurnAround;
	USHORT fsInterim;
	USHORT fsState;
	}KBDINFO;
typedef KBDINFO FAR *PKBDINFO;

APIRET	APIENTRY	KbdSetStatus (PKBDINFO pkbdinfo, HKBD hkbd);
APIRET	APIENTRY	KbdGetStatus (PKBDINFO pkbdinfo, HKBD hdbd);

APIRET	APIENTRY	KbdSetCp (USHORT usReserved, USHORT pidCP, HKBD hdbd);
APIRET	APIENTRY	KbdGetCp (ULONG ulReserved, PUSHORT pidCP, HKBD hkbd);

APIRET	APIENTRY	KbdOpen (PHKBD phkbd);
APIRET	APIENTRY	KbdClose (HKBD hkbd);

APIRET	APIENTRY	KbdGetFocus (USHORT fWait, HKBD hkbd);
APIRET	APIENTRY	KbdFreeFocus (HKBD hkbd);

APIRET	APIENTRY	KbdSynch (USHORT fsWait);

APIRET	APIENTRY	KbdSetFgnd(VOID);

/* structure for KbdGetHWID() */
typedef struct _KBDHWID {	/* kbhw */
	USHORT cb;
	USHORT idKbd;
	USHORT usReserved1;
	USHORT usReserved2;
	} KBDHWID;
typedef KBDHWID FAR *PKBDHWID;

APIRET	APIENTRY	KbdGetHWID (PKBDHWID pkbdhwid, HKBD hkbd);
APIRET	APIENTRY	KbdSetHWID (PKBDHWID pkbdhwid, HKBD hkbd);

/* structure for KbdXlate() */
typedef struct _KBDTRANS {	/* kbxl */
	UCHAR	   chChar;
	UCHAR	   chScan;
	UCHAR	   fbStatus;
	UCHAR	   bNlsShift;
	USHORT	   fsState;
	ULONG	   time;
	USHORT	   fsDD;
	USHORT	   fsXlate;
	USHORT	   fsShift;
	USHORT	   sZero;
	} KBDTRANS;
typedef KBDTRANS FAR *PKBDTRANS;

APIRET	APIENTRY	KbdXlate (PKBDTRANS pkbdtrans, HKBD hkbd);
APIRET	APIENTRY	KbdSetCustXt (PUSHORT usCodePage, HKBD hkbd);

#endif /* INCL_KBD */

#ifdef INCL_VIO

/* XLATOFF */
#ifdef INCL_16
#define	VioCheckCharType	Vio16CheckCharType
#define	VioDeRegister	Vio16DeRegister
#define	VioEndPopUp	Vio16EndPopUp
#define	VioGetAnsi	Vio16GetAnsi
#define	VioGetBuf	Vio16GetBuf
#define	VioGetConfig	Vio16GetConfig
#define	VioGetCp	Vio16GetCp
#define	VioGetCurPos	Vio16GetCurPos
#define	VioGetCurType	Vio16GetCurType
#define	VioGetFont	Vio16GetFont
#define	VioGetMode	Vio16GetMode
#define	VioGetPhysBuf	Vio16GetPhysBuf
#define	VioGetState	Vio16GetState
#define	VioModeUndo	Vio16ModeUndo
#define	VioModeWait	Vio16ModeWait
#define	VioPopUp	Vio16PopUp
#define	VioPrtSc	Vio16PrtSc
#define	VioPrtScToggle	Vio16PrtScToggle
#define	VioReadCellStr	Vio16ReadCellStr
#define	VioReadCharStr	Vio16ReadCharStr
#define	VioRedrawSize	Vio16RedrawSize
#define	VioRegister	Vio16Register
#define	VioSavRedrawUndo	Vio16SavRedrawUndo
#define	VioSavRedrawWait	Vio16SavRedrawWait
#define	VioScrLock	Vio16ScrLock
#define	VioScrUnLock	Vio16ScrUnLock
#define	VioScrollDn	Vio16ScrollDn
#define	VioScrollLf	Vio16ScrollLf
#define	VioScrollRt	Vio16ScrollRt
#define	VioScrollUp	Vio16ScrollUp
#define	VioSetAnsi	Vio16SetAnsi
#define	VioSetCp	Vio16SetCp
#define	VioSetCurPos	Vio16SetCurPos
#define	VioSetCurType	Vio16SetCurType
#define	VioSetFont	Vio16SetFont
#define	VioSetMode	Vio16SetMode
#define	VioSetState	Vio16SetState
#define	VioShowBuf	Vio16ShowBuf
#define	VioWrtCellStr	Vio16WrtCellStr
#define	VioWrtCharStr	Vio16WrtCharStr
#define	VioWrtCharStrAtt	Vio16WrtCharStrAtt
#define	VioWrtNAttr	Vio16WrtNAttr
#define	VioWrtNCell	Vio16WrtNCell
#define	VioWrtNChar	Vio16WrtNChar
#define	VioWrtTTY	Vio16WrtTTY
#endif /* INCL_16 */
/* XLATON */

typedef unsigned short	HVIO;
typedef HVIO	FAR *	PHVIO;

APIRET	APIENTRY	VioRegister (PSZ pszModName, PSZ pszEntryName, ULONG flFun1,
			     ULONG flFun2);

/* first parameter registration constants   */
#define VR_VIOGETCURPOS		   0x00000001L
#define VR_VIOGETCURTYPE	   0x00000002L
#define VR_VIOGETMODE		   0x00000004L
#define VR_VIOGETBUF		   0x00000008L
#define VR_VIOGETPHYSBUF	   0x00000010L
#define VR_VIOSETCURPOS		   0x00000020L
#define VR_VIOSETCURTYPE	   0x00000040L
#define VR_VIOSETMODE		   0x00000080L
#define VR_VIOSHOWBUF		   0x00000100L
#define VR_VIOREADCHARSTR	   0x00000200L
#define VR_VIOREADCELLSTR	   0x00000400L
#define VR_VIOWRTNCHAR		   0x00000800L
#define VR_VIOWRTNATTR		   0x00001000L
#define VR_VIOWRTNCELL		   0x00002000L
#define VR_VIOWRTTTY		   0x00004000L
#define VR_VIOWRTCHARSTR	   0x00008000L

#define VR_VIOWRTCHARSTRATT	   0x00010000L
#define VR_VIOWRTCELLSTR	   0x00020000L
#define VR_VIOSCROLLUP		   0x00040000L
#define VR_VIOSCROLLDN		   0x00080000L
#define VR_VIOSCROLLLF		   0x00100000L
#define VR_VIOSCROLLRT		   0x00200000L
#define VR_VIOSETANSI		   0x00400000L
#define VR_VIOGETANSI		   0x00800000L
#define VR_VIOPRTSC		   0x01000000L
#define VR_VIOSCRLOCK		   0x02000000L
#define VR_VIOSCRUNLOCK		   0x04000000L
#define VR_VIOSAVREDRAWWAIT	   0x08000000L
#define VR_VIOSAVREDRAWUNDO	   0x10000000L
#define VR_VIOPOPUP		   0x20000000L
#define VR_VIOENDPOPUP		   0x40000000L
#define VR_VIOPRTSCTOGGLE	   0x80000000L

/* second parameter registration constants  */
#define VR_VIOMODEWAIT		   0x00000001L
#define VR_VIOMODEUNDO		   0x00000002L
#define VR_VIOGETFONT		   0x00000004L
#define VR_VIOGETCONFIG		   0x00000008L
#define VR_VIOSETCP		   0x00000010L
#define VR_VIOGETCP		   0x00000020L
#define VR_VIOSETFONT		   0x00000040L
#define VR_VIOGETSTATE		   0x00000080L
#define VR_VIOSETSTATE		   0x00000100L

APIRET	APIENTRY	VioDeRegister (void);

APIRET	APIENTRY	VioGetBuf (PULONG pLVB, PUSHORT pcbLVB, HVIO hvio);

APIRET	APIENTRY	VioGetCurPos (PUSHORT pusRow, PUSHORT pusColumn, HVIO hvio);
APIRET	APIENTRY	VioSetCurPos (USHORT usRow, USHORT usColumn, HVIO hvio);

/* structure for VioSet/GetCurType() */
typedef struct _VIOCURSORINFO { /* vioci */
	USHORT	 yStart;
	USHORT	 cEnd;
	USHORT	 cx;
	USHORT	 attr;
	} VIOCURSORINFO;
typedef VIOCURSORINFO FAR *PVIOCURSORINFO;

APIRET	APIENTRY	VioGetCurType (PVIOCURSORINFO pvioCursorInfo, HVIO hvio);
APIRET	APIENTRY	VioSetCurType (PVIOCURSORINFO pvioCursorInfo, HVIO hvio);

/* structure for VioSet/GetMode() */
typedef struct _VIOMODEINFO {	/* viomi */
	USHORT cb;
	UCHAR  fbType;
	UCHAR  color;
	USHORT col;
	USHORT row;
	USHORT hres;
	USHORT vres;
	UCHAR  fmt_ID;
	UCHAR  attrib;
	ULONG  buf_addr;
	ULONG  buf_length;
	ULONG  full_length;
	ULONG  partial_length;
	PCH    ext_data_addr;
	} VIOMODEINFO;
typedef VIOMODEINFO FAR *PVIOMODEINFO;

#define VGMT_OTHER		   0x01
#define VGMT_GRAPHICS		   0x02
#define VGMT_DISABLEBURST	   0x04

APIRET	APIENTRY	VioGetMode (PVIOMODEINFO pvioModeInfo, HVIO hvio);
APIRET	APIENTRY	VioSetMode (PVIOMODEINFO pvioModeInfo, HVIO hvio);

/* structure for VioGetPhysBuf() */

typedef struct _VIOPHYSBUF {	/* viopb */
	PBYTE	 pBuf;
	ULONG	 cb;
	SEL	 asel[1];
	} VIOPHYSBUF;
typedef VIOPHYSBUF FAR *PVIOPHYSBUF;

APIRET	APIENTRY	VioGetPhysBuf (PVIOPHYSBUF pvioPhysBuf, USHORT usReserved);

APIRET	APIENTRY	VioReadCellStr (PCH pchCellStr, PUSHORT pcb, USHORT usRow,
				USHORT usColumn, HVIO hvio);
APIRET	APIENTRY	VioReadCharStr (PCH pchCellStr, PUSHORT pcb, USHORT usRow,
				USHORT usColumn, HVIO hvio);
APIRET	APIENTRY	VioWrtCellStr (PCH pchCellStr, USHORT cb, USHORT usRow,
			       USHORT usColumn, HVIO hvio);
APIRET	APIENTRY	VioWrtCharStr (PCH pchStr, USHORT cb, USHORT usRow,
			       USHORT usColumn, HVIO hvio);

APIRET	APIENTRY	VioScrollDn (USHORT usTopRow, USHORT usLeftCol,
			     USHORT usBotRow, USHORT usRightCol,
			     USHORT cbLines, PBYTE pCell, HVIO hvio);
APIRET	APIENTRY	VioScrollUp (USHORT usTopRow, USHORT usLeftCol,
			     USHORT usBotRow, USHORT usRightCol,
			     USHORT cbLines, PBYTE pCell, HVIO hvio);
APIRET	APIENTRY	VioScrollLf (USHORT usTopRow, USHORT usLeftCol,
			     USHORT usBotRow, USHORT usRightCol,
			     USHORT cbCol, PBYTE pCell, HVIO hvio);
APIRET	APIENTRY	VioScrollRt (USHORT usTopRow, USHORT usLeftCol,
			     USHORT usBotRow, USHORT usRightCol,
			     USHORT cbCol, PBYTE pCell, HVIO hvio);

APIRET	APIENTRY	VioWrtNAttr (PBYTE pAttr, USHORT cb, USHORT usRow,
			     USHORT usColumn, HVIO hvio);
APIRET	APIENTRY	VioWrtNCell (PBYTE pCell, USHORT cb, USHORT usRow,
			     USHORT usColumn, HVIO hvio);
APIRET	APIENTRY	VioWrtNChar (PCH pchChar, USHORT cb, USHORT usRow,
			     USHORT usColumn, HVIO hvio);
APIRET	APIENTRY	VioWrtTTY (PCH pch, USHORT cb, HVIO hvio);
APIRET	APIENTRY	VioWrtCharStrAtt (PCH pch, USHORT cb, USHORT usRow,
				  USHORT usColumn, PBYTE pAttr, HVIO hvio);

#define VCC_SBCSCHAR		   0
#define VCC_DBCSFULLCHAR	   1
#define VCC_DBCS1STHALF 	   2
#define VCC_DBCS2NDHALF 	   3

APIRET	APIENTRY	VioCheckCharType (PUSHORT pType, USHORT usRow,
				  USHORT usColumn, HVIO hvio);

APIRET	APIENTRY	VioShowBuf (USHORT offLVB, USHORT cb, HVIO hvio);


#define ANSI_ON			   1
#define ANSI_OFF		   0

APIRET	APIENTRY	VioSetAnsi (USHORT fAnsi, HVIO hvio);
APIRET	APIENTRY	VioGetAnsi (PUSHORT pfAnsi, HVIO hvio);

APIRET	APIENTRY	VioPrtSc (HVIO hvio);
APIRET	APIENTRY	VioPrtScToggle (HVIO hvio);

#define VSRWI_SAVEANDREDRAW	   0
#define VSRWI_REDRAW		   1

#define VSRWN_SAVE		   0
#define VSRWN_REDRAW		   1

#define UNDOI_GETOWNER		   0
#define UNDOI_RELEASEOWNER	   1

#define UNDOK_ERRORCODE		   0
#define UNDOK_TERMINATE		   1

APIRET	APIENTRY	VioRedrawSize (PULONG pcbRedraw);
APIRET	APIENTRY	VioSavRedrawWait (USHORT usRedrawInd, PUSHORT pNotifyType,
				  USHORT usReserved);
APIRET	APIENTRY	VioSavRedrawUndo (USHORT usOwnerInd, USHORT usKillInd,
				  USHORT usReserved);

#define VMWR_POPUP		   0
#define VMWN_POPUP		   0

APIRET	APIENTRY	VioModeWait (USHORT usReqType, PUSHORT pNotifyType,
			     USHORT usReserved);
APIRET	APIENTRY	VioModeUndo (USHORT usOwnerInd, USHORT usKillInd,
			     USHORT usReserved);

#define LOCKIO_NOWAIT		   0
#define LOCKIO_WAIT		   1

#define LOCK_SUCCESS		   0
#define LOCK_FAIL		   1

APIRET	APIENTRY	VioScrLock (USHORT fWait, PUCHAR pfNotLocked, HVIO hvio);
APIRET	APIENTRY	VioScrUnLock (HVIO hvio);

#define VP_NOWAIT		   0x0000
#define VP_WAIT			   0x0001
#define VP_OPAQUE		   0x0000
#define VP_TRANSPARENT		   0x0002

APIRET	APIENTRY	VioPopUp (PUSHORT pfWait, HVIO hvio);
APIRET	APIENTRY	VioEndPopUp (HVIO hvio);

/* structure for VioGetConfig() */

typedef struct _VIOCONFIGINFO { /* vioin */
	USHORT	cb;
	USHORT	adapter;
	USHORT	display;
	ULONG	cbMemory;
	USHORT	Configuration;
	USHORT	VDHVersion;
	USHORT	Flags;
	ULONG	HWBufferSize;
	ULONG	FullSaveSize;
	ULONG	PartSaveSize;
	USHORT	EMAdaptersOFF;
	USHORT	EMDisplaysOFF;
	} VIOCONFIGINFO;
typedef VIOCONFIGINFO FAR *PVIOCONFIGINFO;


#define VIO_CONFIG_CURRENT	   0
#define VIO_CONFIG_PRIMARY	   1
#define VIO_CONFIG_SECONDARY	   2

APIRET	APIENTRY	VioGetConfig (USHORT usConfigId, PVIOCONFIGINFO pvioin,
			      HVIO hvio);

/* structure for VioGet/SetFont() */
typedef struct _VIOFONTINFO {	/* viofi */
	USHORT	cb;
	USHORT	type;
	USHORT	cxCell;
	USHORT	cyCell;
	PVOID	pbData;
	USHORT	cbData;
	} VIOFONTINFO;
typedef VIOFONTINFO FAR *PVIOFONTINFO;

#define VGFI_GETCURFONT		   0
#define VGFI_GETROMFONT		   1

APIRET	APIENTRY	VioGetFont (PVIOFONTINFO pviofi, HVIO hvio);
APIRET	APIENTRY	VioSetFont (PVIOFONTINFO pviofi, HVIO hvio);

APIRET	APIENTRY	VioGetCp (USHORT usReserved, PUSHORT pIdCodePage, HVIO hvio);
APIRET	APIENTRY	VioSetCp (USHORT usReserved, USHORT idCodePage, HVIO hvio);

typedef struct _VIOPALSTATE {	/* viopal */
	USHORT	cb;
	USHORT	type;
	USHORT	iFirst;
	USHORT	acolor[1];
	}VIOPALSTATE;
typedef VIOPALSTATE FAR *PVIOPALSTATE;

typedef struct _VIOOVERSCAN {	/* vioos */
	USHORT	cb;
	USHORT	type;
	USHORT	color;
	}VIOOVERSCAN;
typedef VIOOVERSCAN FAR *PVIOOVERSCAN;

typedef struct _VIOINTENSITY {	/* vioint */
	USHORT	cb;
	USHORT	type;
	USHORT	fs;
	}VIOINTENSITY;
typedef VIOINTENSITY FAR *PVIOINTENSITY;

typedef struct _VIOCOLORREG {  /* viocreg */
	USHORT	cb;
	USHORT	type;
	USHORT	firstcolorreg;
	USHORT	numcolorregs;
	PCH	colorregaddr;
	}VIOCOLORREG;
typedef VIOCOLORREG FAR *PVIOCOLORREG;

typedef struct _VIOSETULINELOC {  /* viouline */
	USHORT	cb;
	USHORT	type;
	USHORT	scanline;
	}VIOSETULINELOC;
typedef VIOSETULINELOC FAR *PVIOSETULINELOC;

typedef struct _VIOSETTARGET {	/* viosett */
	USHORT	cb;
	USHORT	type;
	USHORT	defaultalgorithm;
	}VIOSETTARGET;
typedef VIOSETTARGET FAR *PVIOSETTARGET;

APIRET	APIENTRY	VioGetState (PVOID pState, HVIO hvio);
APIRET	APIENTRY	VioSetState (PVOID pState, HVIO hvio);

#endif /* INCL_VIO */

#ifdef INCL_MOU

/* XLATOFF */
#ifdef INCL_16
#define	MouClose	Mou16Close
#define	MouDeRegister	Mou16DeRegister
#define	MouDrawPtr	Mou16DrawPtr
#define	MouFlushQue	Mou16FlushQue
#define	MouGetDevStatus	Mou16GetDevStatus
#define	MouGetEventMask	Mou16GetEventMask
#define	MouGetNumButtons	Mou16GetNumButtons
#define	MouGetNumMickeys	Mou16GetNumMickeys
#define	MouGetNumQueEl	Mou16GetNumQueEl
#define	MouGetPtrPos	Mou16GetPtrPos
#define	MouGetPtrShape	Mou16GetPtrShape
#define	MouGetScaleFact	Mou16GetScaleFact
#define	MouGetThreshold	Mou16GetThreshold
#define	MouInitReal	Mou16InitReal
#define	MouOpen		Mou16Open
#define	MouReadEventQue	Mou16ReadEventQue
#define	MouRegister	Mou16Register
#define	MouRemovePtr	Mou16RemovePtr
#define	MouSetDevStatus	Mou16SetDevStatus
#define	MouSetEventMask	Mou16SetEventMask
#define	MouSetPtrPos	Mou16SetPtrPos
#define	MouSetPtrShape	Mou16SetPtrShape
#define	MouSetScaleFact	Mou16SetScaleFact
#define	MouSetThreshold	Mou16SetThreshold
#define	MouSynch	Mou16Synch
#endif /* INCL_16 */
/* XLATON */

typedef unsigned short	HMOU;
typedef HMOU	FAR *	PHMOU;

APIRET	APIENTRY	MouRegister (PSZ pszModName, PSZ pszEntryName, ULONG flFuns);

#define MR_MOUGETNUMBUTTONS	   0x00000001L
#define MR_MOUGETNUMMICKEYS	   0x00000002L
#define MR_MOUGETDEVSTATUS	   0x00000004L
#define MR_MOUGETNUMQUEEL	   0x00000008L
#define MR_MOUREADEVENTQUE	   0x00000010L
#define MR_MOUGETSCALEFACT	   0x00000020L
#define MR_MOUGETEVENTMASK	   0x00000040L
#define MR_MOUSETSCALEFACT	   0x00000080L
#define MR_MOUSETEVENTMASK	   0x00000100L
#define MR_MOUOPEN		   0x00000800L
#define MR_MOUCLOSE		   0x00001000L
#define MR_MOUGETPTRSHAPE	   0x00002000L
#define MR_MOUSETPTRSHAPE	   0x00004000L
#define MR_MOUDRAWPTR		   0x00008000L
#define MR_MOUREMOVEPTR		   0x00010000L
#define MR_MOUGETPTRPOS		   0x00020000L
#define MR_MOUSETPTRPOS		   0x00040000L
#define MR_MOUINITREAL		   0x00080000L
#define MR_MOUSETDEVSTATUS	   0x00100000L

APIRET	APIENTRY	MouDeRegister (void);

APIRET	APIENTRY	MouFlushQue (HMOU hmou);

#define MHK_BUTTON1		   0x0001
#define MHK_BUTTON2		   0x0002
#define MHK_BUTTON3		   0x0004

/* structure for MouGet/SetPtrPos() */
typedef struct _PTRLOC {    /* moupl */
	USHORT row;
	USHORT col;
	} PTRLOC;
typedef PTRLOC FAR *PPTRLOC;

APIRET	APIENTRY	MouGetPtrPos (PPTRLOC pmouLoc, HMOU hmou);
APIRET	APIENTRY	MouSetPtrPos (PPTRLOC pmouLoc, HMOU hmou);

/* structure for MouGet/SetPtrShape() */
typedef struct _PTRSHAPE {  /* moups */
	USHORT cb;
	USHORT col;
	USHORT row;
	USHORT colHot;
	USHORT rowHot;
	} PTRSHAPE;
typedef PTRSHAPE FAR *PPTRSHAPE;

APIRET	APIENTRY	MouSetPtrShape (PBYTE pBuf, PPTRSHAPE pmoupsInfo, HMOU hmou);
APIRET	APIENTRY	MouGetPtrShape (PBYTE pBuf, PPTRSHAPE pmoupsInfo, HMOU hmou);

APIRET	APIENTRY	MouGetDevStatus (PUSHORT pfsDevStatus, HMOU hmou);

APIRET	APIENTRY	MouGetNumButtons (PUSHORT pcButtons, HMOU hmou);
APIRET	APIENTRY	MouGetNumMickeys (PUSHORT pcMickeys, HMOU hmou);

/* structure for MouReadEventQue() */
typedef struct _MOUEVENTINFO {	/* mouev */
	USHORT fs;
	ULONG  time;
	USHORT row;
	USHORT col;
	}MOUEVENTINFO;
typedef MOUEVENTINFO FAR *PMOUEVENTINFO;

APIRET	APIENTRY	MouReadEventQue (PMOUEVENTINFO pmouevEvent, PUSHORT pfWait,
				 HMOU hmou);

/* structure for MouGetNumQueEl() */
typedef struct _MOUQUEINFO {	/* mouqi */
	USHORT cEvents;
	USHORT cmaxEvents;
	} MOUQUEINFO;
typedef MOUQUEINFO FAR *PMOUQUEINFO;

APIRET	APIENTRY	MouGetNumQueEl (PMOUQUEINFO qmouqi, HMOU hmou);

APIRET	APIENTRY	MouGetEventMask (PUSHORT pfsEvents, HMOU hmou);
APIRET	APIENTRY	MouSetEventMask (PUSHORT pfsEvents, HMOU hmou);

/* structure for MouGet/SetScaleFact() */
typedef struct _SCALEFACT { /* mousc */
	USHORT rowScale;
	USHORT colScale;
	} SCALEFACT;
typedef SCALEFACT FAR *PSCALEFACT;

APIRET	APIENTRY	MouGetScaleFact (PSCALEFACT pmouscFactors, HMOU hmou);
APIRET	APIENTRY	MouSetScaleFact (PSCALEFACT pmouscFactors, HMOU hmou);

APIRET	APIENTRY	MouOpen (PSZ pszDvrName, PHMOU phmou);
APIRET	APIENTRY	MouClose (HMOU hmou);

/* structure for MouRemovePtr() */
typedef struct _NOPTRRECT { /* mourt */
	USHORT row;
	USHORT col;
	USHORT cRow;
	USHORT cCol;
	} NOPTRRECT;
typedef NOPTRRECT FAR *PNOPTRRECT;

APIRET	APIENTRY	MouRemovePtr (PNOPTRRECT pmourtRect, HMOU hmou);

APIRET	APIENTRY	MouDrawPtr (HMOU hmou);

#define MOU_NODRAW		   0x0001
#define MOU_DRAW		   0x0000
#define MOU_MICKEYS		   0x0002
#define MOU_PELS		   0x0000

APIRET	APIENTRY	MouSetDevStatus (PUSHORT pfsDevStatus, HMOU hmou);
APIRET	APIENTRY	MouInitReal (PSZ);

APIRET	APIENTRY	MouSynch(USHORT pszDvrName);

typedef struct _THRESHOLD {	/* threshold */
	USHORT Length;		/* Length Field            */
	USHORT Level1;		/* First movement level    */
	USHORT Lev1Mult;	/* First level multiplier  */
	USHORT Level2;		/* Second movement level   */
	USHORT lev2Mult;	/* Second level multiplier */
} THRESHOLD, *PTHRESHOLD;

APIRET	APIENTRY	MouGetThreshold(PTHRESHOLD pthreshold, HMOU hmou);
APIRET	APIENTRY	MouSetThreshold(PTHRESHOLD pthreshold, HMOU hmou);

#endif /* INCL_MOU */
