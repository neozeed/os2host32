/*** Public Module *****************************************************\
*
*   Copyright (c) IBM Corporation 1987, 1989
*   Copyright (c) MicroSoft Corporation 1987,1989
*
* Function Key Areas
* For OS/2 Presentation Manager 1.2
* Copyright IBM Hursley 1988
*
* This header contains public definitions of function key area related
* data.
*
\***********************************************************************/

#if !(defined(INCL_32) || defined(INCL_16))
#ifdef M_I386
    #define INCL_32
#else /* not M_I386 */
    #define INCL_16
#endif /* M_I386 */
#endif /* INCL_32 || INCL_16 */

#ifdef INCL_FKA

/* FKA Frame control IDs                       */
#define FID_FKA             0x8009

/* Frame create flags                          */
/* Use with frame create flags                 */
#define FCF_FKA          0x00800000L

/* Window class for FKA                        */
#define WC_FKA          ((PSZ)0xffff000bL)

/* Use with fka style bits                     */

#define FKAS_SHORT       0x00010000L
#define FKAS_OFF         0x00020000L
#define FKAS_BORDER      0x00040000L

/* error returns from FKA control messages     */

#define FIT_NONE     -1
#define FIT_ERROR    -2
#define FIT_MEMERROR -3
#define FIT_END      -4

/* FKA messages                                                                      */
/* these messages range between FM_FKAFIRST and FM_FKALAST                           */

#define FM_FKAFIRST               0x0171
#define FM_INSERTITEM             0x0171
#define FM_DELETEITEM             0x0172
#define FM_QUERYITEM              0x0173
#define FM_SETITEM                0x0174
#define FM_QUERYITEMCOUNT         0x0175
#define FM_QUERYITEMTEXT          0x0176
#define FM_QUERYITEMTEXTLENGTH    0x0177
#define FM_SETITEMTEXT            0x0178
#define FM_ITEMPOSITIONFROMID     0x0179
#define FM_ITEMIDFROMPOSITION     0x017a
#define FM_NEXTFORM               0x017b
#define FM_FKALAST                0x017b

/* FKA button styles & attributes                                                    */

#define FIS_CURRENT         0x0000 /* specifies the currently active form of the FKA */
#define FIS_SHORT           0x0100 /* specifies the short form of the FKA area       */
#define FIS_LONG            0x0200 /* specifies the long form of the FKA area        */
#define FIS_NEXT            0x0400 /* specifies the next FKA form in the cycle       */
#define FIS_NONE            0x0800 /* specifies that the FKA should be turned off    */

#ifdef OLD_H2INC
#define FIS_USERDRAW        MIS_OWNERDRAW
#define FIS_SYSCOMMAND      MIS_SYSCOMMAND
#define FIS_HELP            MIS_HELP
#else
/* new h2inc does not accept #define FOO BAR
 * the following constants are the MIS_ equivalents
 */
#define FIS_USERDRAW        0x0008
#define FIS_SYSCOMMAND      0x0040
#define FIS_HELP            0x0080
#endif

/* Flags for use with WinSetFKAForm and WinQueryFKAForm                              */

#define SFF_OFF             0x0001
#define SFF_ON              0x0002
#define SFF_SHORT           0x0004
#define SFF_LONG            0x0008
#define SFF_BORDER          0x0010
#define SFF_NOBORDER        0x0020
#define SFF_NEXT            0x0040

typedef struct _FKAITEM {  /* fki                                */
    SHORT iPosition;       /* zero oriented column of button     */
    SHORT iRow;            /* zero oriented row of button        */
    USHORT afStyle;        /* style bits associated with button  */
    USHORT id;             /* id of the button control           */
    HWND  hwndSubMenu;     /* handle of the button               */
} FKAITEM;
typedef FKAITEM FAR * PFKAITEM;


typedef struct _MTI {      /* mti */
        USHORT afStyle;    /* Style Flags      */
        USHORT pad;        /* pad for template */
        USHORT idItem;     /* Item ID          */
        CHAR   c[2];       /*                  */
} MTI;

typedef struct _MT {       /* mt */
    ULONG  len;            /* Length of template in bytes */
    USHORT codepage;       /* Codepage                    */
    USHORT reserved;       /* Reserved.                   */
    USHORT cMti;           /* Count of template items.    */
    MTI    rgMti[1];       /* Array of template items.    */
} MT;
typedef MT FAR * LPMT;


/* declarations from fka.c */
HWND    APIENTRY WinLoadFKA( HWND hFrame, HMODULE hModule, USHORT idMenu);
HWND    APIENTRY WinCreateFKA( HWND hFrame, LPMT lpmt );
HWND    APIENTRY WinGoToFKA( HWND hwnd );
BOOL    APIENTRY WinGoFromFKA( HWND hFKA );
BOOL    APIENTRY WinSetFKAForm( HWND hOwner, USHORT iStyle, BOOL bRedraw );
USHORT  APIENTRY WinQueryFKAForm( HWND hOwner );

/* this will disappear as soon as the resource compiler is fixed. */
HWND    APIENTRY TfLoadMenu( HWND hFrame, HMODULE hModule, USHORT idMenu );

/* I don't really like this public */
typedef struct _FKASIZE {  /* fks */
       USHORT Form;        /* complete SFF form of the FKA */
       SHORT  NumRows;     /* number of rows displayed     */
       SHORT  Height;      /* height in pels               */
} FKASIZE;
typedef FKASIZE FAR *PFKASIZE;

#define FKA_LONG_ROW_HEIGHT 20
#define FKA_SHORT_ROW_HEIGHT 25

#endif /* INCL_FKA */
