/* This header file does all the necessary includes and defines all the
   structures and constants needed for the hanoi program                 */

#define INCL_WIN
#define INCL_WINHEAP
#define INCL_WINDIALOGS
#define INCL_GPIPRIMITIVES
#define INCL_DOSPROCESS


#include <os2.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* This structure is passed into the recursive Hanoi() procedure and contains
   the current window handle as well as a flag indicating the current
   processing status.  If the fContinueCalc flag becomes FALSE, that
   indicates that the user has selected stop from the menu bar and that
   the procedure should exit the recursive process and terminate. */


MRESULT EXPENTRY ClientWndProc  (HWND, USHORT, MPARAM, MPARAM);
MRESULT EXPENTRY EntryFldDlgProc(HWND, USHORT, MPARAM, MPARAM);
MRESULT EXPENTRY AboutDlgProc(HWND, USHORT, MPARAM, MPARAM);
VOID CalcThread(VOID);
VOID DrawDisk(HPS,BYTE,BYTE,BYTE);
VOID MoveDisk(HPS,BYTE,BYTE);
VOID Hanoi(BYTE, BYTE, BYTE, BYTE);
VOID EnableMenuItem(HWND hwnd, SHORT sMenuItem, BOOL fEnable);
VOID SetupTowers(VOID);


#define MAXDISKCNT         16    /* Maximum # of disks allowable */
#define BASEXOFFSET        10    /* X offset for platform base   */
#define BASEYOFFSET        10    /* Y offset for platform base   */
#define BASETHICK          10    /* Base thickness               */
#define BASELEN           300    /* Base width                   */
#define POSTOFFSET         62    /* First post offset from edge  */
#define POSTSPACE         100    /* Distance between posts       */
#define POSTWIDTH           5    /* Width of each post           */
#define POSTHALF            2    /* 1/2 width for centering      */
#define POSTEXTRAHT        25    /* Post height above top disk   */
#define DISKTHICK           3    /* Thickness of each disk       */
#define DISKSPACE         (DISKTHICK+1)     /* Thickness + space */
#define MINDISKWIDTH       11    /* Width of the smallest disk   */
#define MAXDISKWIDTH       91    /* Width of the thickest disk   */
#define BOTDISKYPOS        (BASEYOFFSET+BASETHICK+DISKSPACE-DISKTHICK)
#define STACK            8192    /* Stack size for thread        */
#define fDRAW               1    /* Indicate draw to DrawDisk()  */
#define fERASE              0    /* Indicate erase               */

#define DEFAULTSIZE         5    /* Default number of disks      */
#define MSGBUFSIZE         25    /* Space needed for sprintf msg */

/****** Resource IDs *****/


#define ID_RESOURCE    1
#define IDM_START      2
#define IDM_STOP       3
#define IDM_SET        4
#define IDM_ABOUT      5
#define ID_SETCOUNT    6
#define ID_ENTRYFLD    7
#define ID_MSGBOX      8
#define IDD_ABOUTBOX   9
#define UM_CALC_DONE   (WM_USER+0) /* Message posted when thread terminates */



/* Macro for drawing a rectangle.  Assumes hps and ptl structs are defined
   in the calling module. */

#define DrawRect(x1,y1,x2,y2,color)  ptl.x = (LONG) (x1);             \
                                     ptl.y = (LONG) (y1);             \
                                     GpiSetCurrentPosition(hps,&ptl); \
                                     ptl.x = (LONG) (x2);             \
                                     ptl.y = (LONG) (y2);             \
                                     GpiSetColor(hps,color);          \
                                     GpiBox(hps,DRO_FILL,&ptl,0L,0L);
