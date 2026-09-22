/***************************************************************************\
* wmchar.c -- Displays WM_CHAR messages
*
* Created by Microsoft, IBM Corporation 1990
*
*	DISCLAIMER OF WARRANTIES.  The following [enclosed] code is 
*	sample code created by Microsoft Corporation and/or IBM 
*	Corporation. This sample code is not part of any standard 
*	Microsoft or IBM product and is provided to you solely for 
*	the purpose of assisting you in the development of your 
*	applications.  The code is provided "AS IS", without 
*	warranty of any kind.  Neither Microsoft nor IBM shall be 
*	liable for any damages arising out of your use of the sample 
*	code, even if they have been advised of the possibility of 
*	such damages.
*
\***************************************************************************/


#define INCL_WIN
#define INCL_GPILCIDS
#include <os2.h>
#include <string.h>
#include <stdio.h>
#include "wmchar.h"


#define max2(a, b)  ((a) > (b) ? (a) : (b))
#define min2(a, b)  ((a) < (b) ? (a) : (b))


/* display dependent values */
SHORT gcyChar;
SHORT gcxAveChar;
SHORT gcxMaxChar;
SHORT gcxMargin;

/* program globals */
USHORT gcMessages = 0;      /* also the "number" of the most recent message */
CHAR gszNull[] = "  --";    /* denotes an empty field */
BOOL gfKeyUps = FALSE;


/* char messages are stored in a circular buffer */
typedef struct _CHMSG {     /* cm */
    USHORT usKC;
    UCHAR uchRep;
    UCHAR uchScan;
    USHORT ch;
    USHORT usVK;
} CHMSG;

#define CBUFFERELTS 20

CHMSG gacm[CBUFFERELTS];
SHORT gicmStart = 0;


/* display field attributes are stored in an array */
typedef struct _MSGFIELD {  /* mf */
    BOOL fDisplay;          /* TRUE if field is currently being displayed */
    SHORT cxWidth;          /* field width, initially chars, then pixels */
    CHAR *szHeading;        /* field heading */
} MSGFIELD;

#define INUMBER         0
#define IVIRTUALKEY     1
#define ICHAR           2
#define ISCANCODE       3
#define IREPEAT         4
#define IFLAGS          5

MSGFIELD gamf[CDISPLAYFIELDS] = {
    TRUE,   7, " #",
    TRUE,  15, "VK_ Value",
    TRUE,  14, "Char",
    FALSE, 10, "Scan",
    FALSE, 8,  "Rep",
    TRUE,  0,  "KC_ Flags"      /* variable width, so it's last */
};

SHORT gcxMargin = 3;


/* char flag strings */
CHAR *gachFlags[] = {
    "char",
    "virtualkey",
    "scancode",
    "shift",
    "ctrl",
    "alt",
    "keyup",
    "prevdown",
    "lonekey",
    "deadkey",
    "composite",
    "invalidcomp",
    "toggle",
    "invalidchar",
    "dbcsrsrvd1",
    "dbcsrsrvd2"
};

/* virtual key strings */
CHAR *gachVK[] = {
    gszNull,
    "button1",
    "button2",
    "button3",
    "break",
    "backspace",
    "tab",
    "backtab",
    "newline",
    "shift",
    "ctrl",
    "alt",
    "altgraf",
    "pause",
    "capslock",
    "esc",
    "space",
    "pageup",
    "pagedown",
    "end",
    "home",
    "left",
    "up",
    "right",
    "down",
    "printscrn",
    "insert",
    "delete",
    "scrllock",
    "numlock",
    "enter",
    "sysrq",
    "f1",
    "f2",
    "f3",
    "f4",
    "f5",
    "f6",
    "f7",
    "f8",
    "f9",
    "f10",
    "f11",
    "f12",
    "f13",
    "f14",
    "f15",
    "f16",
    "f17",
    "f18",
    "f19",
    "f20",
    "f21",
    "f22",
    "f23",
    "f24"
};



/***************************************************************************\
* DrawText
*
* This function displays a text string.
*
* Parameters:
*   hps:        presentation space
*   cx, cy:     point to begin drawing
*   sz:         string to draw
*   iColor:     text color
*   fExtent:    whether to calc extent or not
*
* Returns:
*   x position of end of painted string if fExtent == TRUE
\***************************************************************************/

SHORT DrawText(
    HPS hps,
    SHORT cx,
    SHORT cy,
    CHAR *sz,
    LONG iColor,
    BOOL fExtent)
{
    RECTL rcl;
    SHORT cch;

    cch = strlen(sz);

    rcl.xLeft = cx;
    rcl.xRight = cx + cch * gcxMaxChar;     /* hack */
    rcl.yBottom = cy;
    rcl.yTop = cy + gcyChar;

    WinDrawText(hps, cch, (PCH)sz, (PRECTL)&rcl, iColor, SYSCLR_WINDOW, 0);

    if (fExtent) {
        WinDrawText(hps, cch, (PCH)sz, (PRECTL)&rcl, iColor,
                    SYSCLR_WINDOW, DT_QUERYEXTENT);
        return (SHORT)rcl.xRight;
    }

    return 0;
}



/***************************************************************************\
* FormatHexChar
*
* This function formats an unsigned hex number into a string.  It is needed
* to get around shortcomings of sprintf().
*
* Parameters:
*   uch:    number to format
*   sz:     string to get number
\***************************************************************************/

VOID FormatHexChar(
    USHORT uch,
    CHAR *sz)
{
    sprintf(sz, "0x%2x", uch);

    /* patch up hex byte display */
    if (strlen(sz) > 4) {
        /* remove sign extension of byte */
        sz[2] = sz[4];
        sz[3] = sz[5];
        sz[4] = 0;
    } else if (sz[2] == ' ') {
        /* fill in blank with 0 */
        sz[2] = '0';
    }
}



/***************************************************************************\
* DrawMessage
*
* This function draws a single message line.
*
* Parameters:
*   hps:        presentation space
*   iMessage:   message number to display
*   pcm:        pointer to message structure
*   cy:         y position of message
\***************************************************************************/

VOID DrawMessage(
    HPS hps,
    SHORT iMessage,
    CHMSG *pcm,
    SHORT cy)
{
    CHAR sz[16];
    USHORT fsFlags;
    SHORT i;
    SHORT cx;

    cx = gcxMargin;

    if (gamf[INUMBER].fDisplay) {
        /* draw message number */
        sprintf(sz, "%u.", iMessage);
        DrawText(hps, cx, cy, sz, SYSCLR_WINDOWTEXT, FALSE);
        cx += gamf[INUMBER].cxWidth;
    }

    if (gamf[IVIRTUALKEY].fDisplay) {
        /* draw virtual key code */
        DrawText(hps, cx, cy, gachVK[pcm->usVK], SYSCLR_WINDOWTEXT,
                 FALSE);
        cx += gamf[IVIRTUALKEY].cxWidth;
    }

    if (gamf[ICHAR].fDisplay) {
        /* draw character */
        if ((CHAR)pcm->ch) {
            FormatHexChar(pcm->ch, sz);
            DrawText(hps, cx, cy, sz, SYSCLR_WINDOWTEXT, FALSE);
            sprintf(sz, "'%c'", (CHAR)pcm->ch);
            DrawText(hps, cx + 7 * gcxAveChar, cy, sz, SYSCLR_WINDOWTEXT,
                     FALSE);
        } else {
            DrawText(hps, cx, cy, gszNull, SYSCLR_WINDOWTEXT, FALSE);
        }
        cx += gamf[ICHAR].cxWidth;
    }

    if (gamf[ISCANCODE].fDisplay) {
        if (pcm->uchScan) {
            FormatHexChar(pcm->uchScan, sz);
            DrawText(hps, cx, cy, sz, SYSCLR_WINDOWTEXT, FALSE);
        } else {
            DrawText(hps, cx, cy, gszNull, SYSCLR_WINDOWTEXT, FALSE);
        }
        cx += gamf[ISCANCODE].cxWidth;
    }

    if (gamf[IREPEAT].fDisplay) {
        sprintf(sz, "%u", pcm->uchRep);
        DrawText(hps, cx, cy, sz, SYSCLR_WINDOWTEXT, FALSE);
        cx += gamf[IREPEAT].cxWidth;
    }

    if (gamf[IFLAGS].fDisplay) {
        /* draw KC_ flags */
        fsFlags = pcm->usKC;
        for (i = 0; i < 16; i++) {
            if (fsFlags & 1) {
                switch (i) {
                case 0:
                    if (gamf[ICHAR].fDisplay)
                        goto DF_PRINTIT;
                    break;
                case 1:
                    if (gamf[IVIRTUALKEY].fDisplay)
                        goto DF_PRINTIT;
                    break;
                case 2:
                    if (gamf[ISCANCODE].fDisplay)
                        goto DF_PRINTIT;
                    break;
                default:
DF_PRINTIT:
                    cx = DrawText(hps, cx, cy, gachFlags[i], SYSCLR_WINDOWTEXT, TRUE);
                    cx += gcxAveChar;
                }
            }
            fsFlags >>= 1;
        }
        cx += gamf[IFLAGS].cxWidth;
    }
}



/***************************************************************************\
* PaintClient
*
* This function paints a client window.
*
* Parameters:
*   hwnd:   window handle
\***************************************************************************/

VOID PaintClient(
    HWND hwnd)
{
    HPS hps;
    RECTL rcl;
    SHORT iMessage;
    SHORT yBottom;
    SHORT cx;
    SHORT i;

    hps = WinBeginPaint(hwnd, (HPS)NULL, (PRECTL)&rcl);

    /* clear invalidated region */
    WinFillRect(hps, (PRECTL)&rcl, SYSCLR_WINDOW);

    /* get y position to start drawing */
    WinQueryWindowRect(hwnd, (PRECTL)&rcl);
    yBottom = (SHORT)(rcl.yTop - gcyChar - gcyChar / 2);

    /* draw headings */
    cx = gcxMargin;
    for (i = 0; i < CDISPLAYFIELDS; i++) {
        if (gamf[i].fDisplay) {
            DrawText(hps, cx, yBottom, gamf[i].szHeading,
                    SYSCLR_WINDOWSTATICTEXT, FALSE);
            cx += gamf[i].cxWidth;
        }
    }

    /* draw each entry */
    i = gicmStart;
    iMessage = gcMessages;
    yBottom -= (gcyChar + gcyChar / 2);
    do {
        if (!iMessage || yBottom < -gcyChar)
            break;

        DrawMessage(hps, iMessage, &gacm[i], yBottom);

        i = (i + 1) % CBUFFERELTS;
        iMessage--;
        yBottom -= gcyChar;

    } while (i != gicmStart);

    WinEndPaint(hps);
}



/***************************************************************************\
* NewMessage
*
* This function adds a new WM_CHAR message to the list.
*
* Parameters:
*   hwnd:       window handle
*   mp1, mp2:
*   usKC:	key flags
*   usVK:	virtual key code
*   ch: 	char;
\***************************************************************************/

VOID NewMessage(
    HWND hwnd,
    MPARAM mp1,
    MPARAM mp2)
{
    RECTL rclScroll, rclUpdate;
    HPS hps;

    /* overwrite oldest entry */
    if (gicmStart)
        gicmStart--;
    else
        gicmStart = CBUFFERELTS - 1;

    gacm[gicmStart].usKC = SHORT1FROMMP(mp1);
    gacm[gicmStart].uchRep = CHAR3FROMMP(mp1);
    gacm[gicmStart].uchScan = CHAR4FROMMP(mp1);
    gacm[gicmStart].ch = SHORT1FROMMP(mp2);
    gacm[gicmStart].usVK = SHORT2FROMMP(mp2);

    gcMessages++;

    /* scroll and update intelligently */
    WinQueryWindowRect(hwnd, (PRECTL)&rclScroll);
    rclScroll.yTop -= 2 * gcyChar;
    rclScroll.yBottom = max2(0L, rclScroll.yTop - (LONG)(CBUFFERELTS * gcyChar));

    WinScrollWindow(hwnd, 0, -gcyChar, (PRECTL)&rclScroll, (PRECTL)&rclScroll,
                    NULL, &rclUpdate, 0);

    hps = WinGetPS(hwnd);
    WinFillRect(hps, (PRECTL)&rclUpdate, SYSCLR_WINDOW);
    DrawMessage( hps
	       , gcMessages
	       , &gacm[gicmStart]
	       , (SHORT)rclScroll.yTop - gcyChar );
    WinReleasePS(hps);
}



/***************************************************************************\
* CheckMenuItem
*
* This function sets the check state of a menu item.
*
* Parameters:
*   hwnd:   frame window
*   id:     menu id
*   fCheck: check state
\***************************************************************************/

VOID CheckMenuItem(
    HWND hwndFrame,
    SHORT id,
    BOOL fCheck)
{
    HWND hwndMenu;

    /* toggle menu checkmark */
    hwndMenu = WinWindowFromID(hwndFrame, FID_MENU);
    WinSendMsg(hwndMenu, MM_SETITEMATTR, MPFROM2SHORT(id, TRUE),
               MPFROM2SHORT(MIA_CHECKED, fCheck ? MIA_CHECKED : 0));
}



/***************************************************************************\
* MenuCommand
*
* This function processes WM_COMMAND messages.
*
* Parameters:
*   hwnd:   client window handle
*   id:     command identifier
\***************************************************************************/

VOID MenuCommand(
    HWND hwnd,
    SHORT id)
{
    BOOL *pfDisplay;
    HWND hwndFrame;

    if (id == IDM_CLEAR) {

        /* clear current messages */
        gcMessages = 0;
        WinInvalidateRect(hwnd, NULL, FALSE);

    } else if (id == IDM_KEYUPS) {

        /* toggle logging keyups */
        gfKeyUps = !gfKeyUps;
        hwndFrame = WinQueryWindow(hwnd, QW_PARENT, FALSE);
        CheckMenuItem(hwndFrame, IDM_KEYUPS, gfKeyUps);

    } else if (id >= IDM_DISPLAYFIRST && id <= IDM_DISPLAYLAST) {

        /* toggle display field */
        pfDisplay = &(gamf[id - IDM_DISPLAYFIRST].fDisplay);
        *pfDisplay = !*pfDisplay;
        hwndFrame = WinQueryWindow(hwnd, QW_PARENT, FALSE);
        CheckMenuItem(hwndFrame, id, *pfDisplay);

        /* repaint in new format */
        WinInvalidateRect(hwnd, NULL, FALSE);
    }
}



/***************************************************************************\
* MainWndProc
*
* This is the window procedure for the main client window.
\***************************************************************************/

MRESULT EXPENTRY MainWndProc(
    HWND hwnd,
    USHORT msg,
    MPARAM mp1,
    MPARAM mp2)
{
    switch (msg) {

    case WM_PAINT:
        PaintClient(hwnd);
        break;

    case WM_COMMAND:
        MenuCommand(hwnd, SHORT1FROMMP(mp1));
        break;

    case WM_CHAR:
        if (gfKeyUps || !(KC_KEYUP & SHORT1FROMMP(mp1))) {

            /* add new message to list */
            NewMessage(hwnd, mp1, mp2);
        }
        break;

    default:
        return WinDefWindowProc(hwnd, msg, mp1, mp2);
    }

    return (MRESULT)0;
}



/***************************************************************************\
* CheckMenus
*
* This function sets menu check marks according to their current internal
* state.
*
* Parameters:
*   hwndFrame:  main app window handle
\***************************************************************************/

VOID CheckMenus(
    HWND hwndFrame)
{
    SHORT id;

    CheckMenuItem(hwndFrame, IDM_KEYUPS, gfKeyUps);

    for (id = IDM_DISPLAYFIRST; id <= IDM_DISPLAYLAST; id++) {
        CheckMenuItem(hwndFrame, id, gamf[id - IDM_DISPLAYFIRST].fDisplay);
    }
}



/***************************************************************************\
* RegisterSwitchEntry
*
* This function registers the app with the switch list.
*
* Parameters:
*   hwndFrame:  main app window handle
\***************************************************************************/

VOID RegisterSwitchEntry(
    HWND hwndFrame)
{
    PID pid;
    TID tid;
    SWCNTRL swc;

    WinQueryWindowProcess(hwndFrame, (PPID)&pid, (PTID)&tid);
    WinQueryWindowText(hwndFrame, MAXNAMEL, (PSZ)swc.szSwtitle);
    swc.hwnd = hwndFrame;
    swc.hwndIcon = (ULONG)NULL;
    swc.hprog = (HPROGRAM)NULL;
    swc.idProcess = pid;
    swc.idSession = NULL;
    swc.uchVisibility = SWL_VISIBLE;
    swc.fbJump = SWL_JUMPABLE;
    WinAddSwitchEntry((PSWCNTRL)&swc);
}



/***************************************************************************\
* GetDisplayValues
*
* This function sets display dependent global variables.
\***************************************************************************/

VOID GetDisplayValues(VOID)
{
    HPS hps;
    FONTMETRICS fm;
    SHORT i;

    hps = WinGetPS(HWND_DESKTOP);
    GpiQueryFontMetrics(hps, (ULONG)sizeof(FONTMETRICS), (PFONTMETRICS)&fm);
    WinReleasePS(hps);

    gcyChar = (SHORT)(fm.lMaxBaselineExt + fm.lExternalLeading);
    gcxAveChar = (SHORT)fm.lAveCharWidth;
    gcxMaxChar = (SHORT)fm.lMaxCharInc;

    /* adjust display widths for this font */
    gcxMargin *= gcxAveChar;
    for (i = 0; i < CDISPLAYFIELDS; i++) {
        gamf[i].cxWidth *= gcxAveChar;
    }
}



/***************************************************************************\
* main
*
* This is the main procedure for the app.
\***************************************************************************/

VOID cdecl main(VOID)
{
    HAB hab;
    HMQ hmq;
    QMSG qmsg;
    ULONG ulCreate;
    HWND hwndFrame;
    HWND hwndClient;
    static CHAR szApp[] = "Char Messages";

    /* init app */
    hab = WinInitialize(NULL);
    hmq = WinCreateMsgQueue(hab, 0);
    GetDisplayValues();

    /* create main window */
    WinRegisterClass(hab, szApp, MainWndProc, 0L, 0);
    ulCreate = FCF_TITLEBAR | FCF_SYSMENU | FCF_MINMAX | FCF_SIZEBORDER |
               FCF_MENU | FCF_SHELLPOSITION | FCF_ICON;
    hwndFrame = WinCreateStdWindow(HWND_DESKTOP, WS_VISIBLE, &ulCreate,
                                    szApp, szApp, 0L, NULL, ID_RESOURCES,
                                    &hwndClient);

    /* register with switch list */
    RegisterSwitchEntry(hwndFrame);

    /* initialize menu */
    CheckMenus(hwndFrame);

    /* message loop */
    while (WinGetMsg(hab, &qmsg, NULL, 0, 0))
        WinDispatchMsg(hab, &qmsg);

    /* clean up */
    WinDestroyWindow(hwndFrame);
    WinDestroyMsgQueue(hmq);
    WinTerminate(hab);
}
