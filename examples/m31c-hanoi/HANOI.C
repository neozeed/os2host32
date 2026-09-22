/*************************************************************

 This program implements a tower of hanoi program.  This
 sample app was written to demonstrate the use of a multi-
 threaded program.  The main thread handles the PM interface,
 the second thread is started to do the recursive execution
 of the hanoi algorithm.

 Created by Microsoft, IBM Corporation 1990

 DISCLAIMER OF WARRANTIES.  The following [enclosed] code is 
 sample code created by Microsoft Corporation and/or IBM 
 Corporation. This sample code is not part of any standard 
 Microsoft or IBM product and is provided to you solely for 
 the purpose of assisting you in the development of your 
 applications.  The code is provided "AS IS", without 
 warranty of any kind.  Neither Microsoft nor IBM shall be 
 liable for any damages arising out of your use of the sample 
 code, even if they have been advised of the possibility of 
 such damages.

 Procedures in this file:
   main()             Sets up the PM environment and heap and
                      calls the main window procedure ClientWndProc
   ClientWndProc()    Handles the main window messages
   CalcThread()       Sets up and terminates the secondary thread
   DrawDisk()         Draws or erases a disk on one of the poles
   MoveDisk()         Moves a disk from one pole to another
   Hanoi()            Recursive alogrithm for tower of hanoi prog
   EnableMenuItem()   Activates/deactivates a menu choice
   EntryFldDlgProc()  Handles the set number of disks dialog box
   SetupTowers()      Sets up the global tower data

**************************************************************/

#include "hanoi.h"

/********************* GLOBALS *******************************/

CHAR szClientClass[] = "Hanoi";

/* Note that this use of global data precludes multiple windows
   of hanoi running at the same time.  Thus, from an object-
   oriented perspective, this is less than desireable and the
   data should be passed into the window, rather than used
   explicitly. */

BYTE    abTowers[3][MAXDISKCNT];     /* Used to hold disk numbers on each post */
BYTE    abTowersNdx[3];              /* Current number of disks on each post   */
BYTE    cTowerSize = DEFAULTSIZE;   /* Holds the total number of disks        */
USHORT  ausPolePos[3]= { POSTOFFSET, /* Holds offset drawing information     */
                       POSTOFFSET + POSTSPACE,
                       POSTOFFSET + 2*POSTSPACE };
ULONG   ulIterations;
TID     tidCalcThread;
BOOL    fContinueCalc;
HWND    hwndFrame, hwndClient;


/*************************************************************/


/*
 * Function name: main()
 *
 * Parameters:  argc, argv.  If the user places a number (1 thru MAXDISKCNT)
 *              on the command line, that number will become the default
 *              number of disks on the stack. Otherwise there will be
 *              DEFUALTSIZE disks initially.
 *
 * Returns: Always returns 0
 *
 * Purpose: Parses the command line, sets up the PM environment, prepares
 *          the Tower arrays, calls the main window proc, handles the
 *          window's messages then cleans up and exits.
 *
 * Usage/Warnings:
 *
 * Calls: ClientWndProc() (thru PM)
 */

int main(int argc, char *argv[])
{

   HAB          hab;
   HMQ          hmq;
   QMSG         qmsg;
   
   ULONG flFramefContinueCalcFlags = FCF_TITLEBAR      | FCF_SYSMENU | FCF_MINBUTTON |
                        FCF_SHELLPOSITION | FCF_MENU    | FCF_TASKLIST  |
                        FCF_ICON          | FCF_BORDER  | FCF_ACCELTABLE;

   SHORT  sHold;

   if(argc > 1)  /* If command line arg, use as the initial number of disks */
   {
      sHold = atoi(argv[1]);
      if(sHold>0 && sHold<=MAXDISKCNT)
         cTowerSize = (BYTE) sHold;
   }
   SetupTowers();

   /* These PM calls should be error checked */
   hab = WinInitialize(0);
   hmq = WinCreateMsgQueue(hab, 0);

   if(!WinRegisterClass(hab, szClientClass,(PFNWP)ClientWndProc,0L,0))
   {
      WinAlarm(HWND_DESKTOP, WA_ERROR);        /* Register failed */
      DosExit(EXIT_PROCESS,1);
   }

   hwndFrame = WinCreateStdWindow(HWND_DESKTOP, WS_VISIBLE,
                                  &flFramefContinueCalcFlags, szClientClass, NULL,
                                  0L, NULL, ID_RESOURCE, &hwndClient);
   if(!hwndFrame)
   {
      WinAlarm(HWND_DESKTOP, WA_ERROR); /* Window create failed */
      DosExit(EXIT_PROCESS,1);
   }

   while(WinGetMsg(hab,&qmsg,NULL,0,0))        /* Message loop */
      WinDispatchMsg(hab,&qmsg);

   WinDestroyWindow(hwndFrame);                /* Clean up     */
   WinDestroyMsgQueue(hmq);
   WinTerminate(hab);
   return 0;
}


/*
 * Function name: ClientWndProc()
 *
 * Parameters:  hwnd, msg, mp1, mp2.  Standard PM Window Proc params.
 *              No user data is expected in the WM_CREATE.
 *
 * Returns:
 *
 * Purpose: Handles all the messages associated with the main window
 *          and calls the appropriate handling procedures.
 *
 * Usage/Warnings: Called only by main().  Note that when WM_PAINT executes,
 *                 the secondary thread may change data during the update
 *                 which may cause a problem.  However, this is NOT a write
 *                 conflict, as only 1 thread does the writing.
 *
 * Calls:  DrawDisk(), CalcThread() (thru Thread), EntryFldDlgProc() (thru PM)
 */

MRESULT EXPENTRY ClientWndProc(HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2)
{
   HPS    hps;                         /* Handle for painting           */
   RECTL  rcl;                         /* Rectangle struct for painting */
   POINTL ptl;                         /* Point struct for painting     */
   BYTE   cPole,bCnt,bHeight,cnt;      /* Utility variables             */
   CHAR   szMsg[MSGBUFSIZE];           /* sprintf buffer                */

   switch(msg)
   {
      case WM_PAINT:
         hps = WinBeginPaint(hwnd,NULL,NULL);   /* Get paint handle     */
         WinQueryWindowRect(hwnd,&rcl);

         DrawRect(rcl.xLeft,rcl.yBottom,        /* White out the screen */
                  rcl.xRight,rcl.yTop,CLR_WHITE);

         /* Draw the base */
         DrawRect(BASEXOFFSET,           BASEYOFFSET,
                  BASEXOFFSET+BASELEN-1, BASEYOFFSET+BASETHICK-1,
                  CLR_DARKGREEN);

         /* Draw the 3 posts */
         bHeight = cTowerSize*DISKSPACE + POSTEXTRAHT;
         for(cnt=0;cnt<3;cnt++)
         {
            DrawRect(ausPolePos[cnt]-POSTHALF,          BASEYOFFSET,
                     ausPolePos[cnt]-POSTHALF+POSTWIDTH,bHeight,
                     CLR_DARKGREEN);
         }

         /* Place the appropriate disks on each pole */
         for(cPole=0;cPole<3;cPole++)
         {
            for(bCnt=0;bCnt<abTowersNdx[cPole];bCnt++)
            {
               DrawDisk(hps,cPole,bCnt,fDRAW);
            }
         }

         WinEndPaint(hps);
         return 0L;

      case WM_COMMAND:
         switch(SHORT1FROMMP(mp1))
         {
            case IDM_START:
                          /* Set continuation fContinueCalc  */
               fContinueCalc = TRUE;
               ulIterations = 0;         /* Zero iteration counter */

               if (DosCreateThread (&tidCalcThread, CalcThread,
                                         (ULONG)NULL, 0L, STACK))
               {
                  DosBeep (440, 175);
                  return 0L;
               }

               /* Disallow menu items that could change data while the second
                  thread is running */
               EnableMenuItem(hwnd,IDM_START,FALSE); /* Disable Start & set */
               EnableMenuItem(hwnd,IDM_SET,FALSE);
               EnableMenuItem(hwnd,IDM_STOP,TRUE);   /* Enable Stop item    */
               return 0L;

            case IDM_STOP:
               fContinueCalc = FALSE;  /* Notify thread to quit          */
               return 0L;

            case IDM_SET:
               if(WinDlgBox(HWND_DESKTOP, hwnd, /* Pop up the query/set box */
                         (PFNWP)EntryFldDlgProc,NULL,ID_SETCOUNT,NULL))
               {
                  SetupTowers();                          /* Reset towers   */
                  WinInvalidateRect(hwnd,NULL,FALSE);     /* Force a redraw */
               }
               return 0L;
               break;

            case IDM_ABOUT:
                WinDlgBox(HWND_DESKTOP,
                          hwnd,
                          (PFNWP)AboutDlgProc,
                          NULL,
                          IDD_ABOUTBOX,
                          NULL);
                break;


             default:
                return WinDefWindowProc(hwnd, msg, mp1, mp2);
         }

      case UM_CALC_DONE:
         EnableMenuItem(hwnd,IDM_START,TRUE);  /* Reenable Start & set      */
         EnableMenuItem(hwnd,IDM_SET,TRUE);
         EnableMenuItem(hwnd,IDM_STOP,FALSE);  /* Disable stop              */

         sprintf(szMsg,"%lu disks were moved.",ulIterations);  /* Print msg */
         WinMessageBox(HWND_DESKTOP, hwnd, szMsg, "Done!", 0, MB_OK);

         SetupTowers();                        /* Reset towers              */
         WinInvalidateRect(hwnd,NULL,FALSE);   /* Force a screen redraw     */
         return 0L;

       default:
          return WinDefWindowProc(hwnd, msg, mp1, mp2);
   }
}


/*
 * Function name: CalcThread()
 *
 * Parameters: none 
 *
 * Returns: VOID
 *
 * Purpose: Calls the recursive Hanoi with initial setting of 0,2,1 meaning
 *          from pole 0, to pole 2, using pole 1 as a temporary.  Hanoi
 *          returns when finished, or the user says stop.  This proc then
 *          sets a critical section so the posted message won't be handled
 *          until the thread is terminated.
 *
 * Usage/Warnings: No DosExitCritSec() is called since _endthread() supposedly
 *                 clears the critical section when the thread is
 *                 terminated.
 *
 * Calls:  Hanoi()
 */

VOID CalcThread ()
{
   HAB hab;

   hab = WinInitialize(0);    /* Called to increase Ring 2 stack size */
   Hanoi(cTowerSize,0,2,1);      /* Execute the recursive routine */
   WinTerminate(hab);

   DosEnterCritSec(); /* Set Crit so the UM_CALC_DONE isn't processed */
                      /* until this thread has completely terminated  */
   WinPostMsg(hwndClient,UM_CALC_DONE,NULL,NULL);         /* Post done */

   DosExit (EXIT_THREAD, 0);                      /* Terminate thread */
}


/*
 * Function name: DrawDisk()
 *
 * Parameters:  hps is a handle to the main PS space.
 *              cPole is the pole (0-2) to draw the disk on.
 *              bLevel is the number of spaces from the bottom to draw disk.
 *              fDraw if =0, erase disk, if =1 draw disk.
 *
 * Returns: VOID
 *
 * Purpose: This routine takes a PS handle, the hanoi spindle and disk level
 *          and draws an appropriately sized disk.
 *
 * Usage/Warnings: Does not grab exclusive access to the screen before
 *                 drawing which may cause a problem.
 *
 * Calls:
 */

VOID DrawDisk(HPS hps,BYTE cPole, BYTE bLevel,BYTE fDraw)
{
   USHORT usXstart,usYstart,usWidth;
   POINTL ptl;

   usYstart = BOTDISKYPOS + DISKSPACE*bLevel;  /* Calculate Bottom of disk   */

   usWidth  = (MAXDISKWIDTH-MINDISKWIDTH)*abTowers[cPole][bLevel]/cTowerSize
              + MINDISKWIDTH;                  /* Calculate the disk's width */

   usXstart = ausPolePos[cPole] - usWidth/2;   /* Center disk on pole        */

   if(fDraw == fDRAW)  /* If we are to draw the disk */
   {
      DrawRect(usXstart,usYstart,usXstart+usWidth,
               usYstart+DISKTHICK-1,CLR_RED);
   }
   else         /* We are to erase the disk, then redraw the pole */
   {
      DrawRect(usXstart,usYstart,usXstart+usWidth,
               usYstart+DISKTHICK-1,CLR_WHITE);
      DrawRect(ausPolePos[cPole]-POSTHALF,usYstart,
               ausPolePos[cPole]-POSTHALF+POSTWIDTH,usYstart+DISKTHICK-1,
               CLR_DARKGREEN);
   }
}


/*
 * Function name: MoveDisk()
 *
 * Parameters:  hps is a handle to the main PS space.
 *              bFrom is the spindle to take the top disk from.
 *              bTo is the spindle to place the disk on.
 *
 * Returns: VOID
 *
 * Purpose: This routine moves the top disk from the bFrom spindle to the top
 *          of the bTo spindle.
 *
 * Usage/Warnings: Does error checking for trying to move a disk from a
 *                 spindle that does not have any disks on it.
 *
 * Calls:  MoveDisk()
 */

VOID MoveDisk(HPS hps,BYTE bFrom,BYTE bTo)
{
   CHAR bTOSndx;  /* Top of stack index  */
   BYTE bDiskNum; /* Disk number to move */

   bTOSndx = abTowersNdx[bFrom]-1;
   if(bTOSndx < 0)
      return;

   DrawDisk(hps,bFrom,bTOSndx,fERASE);   /* Remove disk off from stack     */

   bDiskNum = abTowers[bFrom][bTOSndx];  /* Get move disk number           */
   abTowersNdx[bFrom]--;                 /* Decrease count on from spindle */

   bTOSndx = abTowersNdx[bTo]++;         /* Offset to place the disk at    */
   abTowers[bTo][bTOSndx] = bDiskNum;    /* Place on stack in memory       */

   DrawDisk(hps,bTo,bTOSndx,fDRAW);      /* Draw disk on the to stack      */
}


/*
 * Function name: Hanoi()
 *
 * Parameters:  pcp is a struct which contains the hwnd handle and the
 *                  continue fContinueCalcFlag which is initially set to TRUE.
 *              bHeight is the number of disks in the from stack to move.
 *              bFrom is the from spindle number, 0-2.
 *              bTo is the to spindle number.
 *              bTemp is the temporary spindle number.
 *
 * Returns: VOID
 *
 * Purpose: This routine implements a recursive hanoi program that works as
 *          follows:  By recursion, move all the disks, except for the
 *          bottom disk to the temporary stack.  Then move the bottom
 *          disk to the target spindle.  Now recursively move the stack
 *          on the temporary spindle to the target spindle.  The limiting
 *          case is when Hanoi() is called with a bHeight of 0 in which
 *          case the depth recursion is terminated.
 *
 * Usage/Warnings: This routine checks the ->fContClac fContinueCalcFlag, which is set
 *                 by the main thread when the user selects stop, to see if
 *                 the user wishes to abort the algorithm.  If so, it backs
 *                 out and exits.
 *
 * Calls:  MoveDisk()
 */

VOID Hanoi(BYTE bHeight, BYTE bFrom, BYTE bTo, BYTE bTemp)
{
   HPS hps;

   if(bHeight<=0 || !fContinueCalc)       /* Exit up if no more disks or */
      return;                             /* the user said Stop          */

   Hanoi(bHeight-1,bFrom,bTemp,bTo);  /* Move all but bottom disk    */

   if(!fContinueCalc)                /* If user said to stop        */
      return;

   /* Display bFrom -> bTo */
   hps = WinGetPS(hwndClient);
   MoveDisk(hps,bFrom,bTo);               /* Move the bottom disk        */
   WinReleasePS(hps);
   ulIterations++;

   Hanoi(bHeight-1,bTemp,bTo,bFrom);  /* Move disks over             */
}


/*
 * Function name: EnableMenuItem()
 *
 * Parameters:  hwnd is a handle of the current window.
 *              sMenuItem is the ID of the item to Enable/Disable.
 *              fEnable will enable item if TRUE, otherwise disables it.
 *
 * Returns: VOID
 *
 * Purpose: This routine handles enabling/disabling of menu items.  This
 *          is done by getting Parent and Menu hwnd handles then sending
 *          the appropriate message.
 *
 * Usage/Warnings:
 *
 * Calls:
 */

VOID EnableMenuItem(HWND hwnd, SHORT sMenuItem, BOOL fEnable)
{
   HWND hwndParent = WinQueryWindow(hwnd, QW_PARENT, FALSE);
   HWND hwndMenu   = WinWindowFromID(hwndParent, FID_MENU);

   WinSendMsg(hwndMenu, MM_SETITEMATTR,
              MPFROM2SHORT(sMenuItem, TRUE),
              MPFROM2SHORT(MIA_DISABLED, fEnable ? 0 : MIA_DISABLED));
}


/*
 * Function name: EntryFldDlgProc()
 *
 * Parameters:  hwnd, msg, mp1, mp2.  Standard PM Dialog Proc params.
 *              No user data is expected in the WM_CREATE.
 *
 * Returns: Terminates with a TRUE iff a new valid Tower Size has been entered.
 *
 * Purpose: Handles all the messages associated with the set entry field
 *          and calls the appropriate handling procedures.  The purpose
 *          of this dialog box is to get a new number of disks for the
 *          hanoi routine.
 *
 *
 * Usage/Warnings: If the value entered is valid, global cTowerSize is
 *                 changed to the new value, and TRUE is returned.
 *
 * Calls:
 */

MRESULT EXPENTRY EntryFldDlgProc(HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2)
{
   SHORT sNewSize;            /* Holds new number of disks        */

   switch(msg)
   {
      case WM_INITDLG:
         WinSendDlgItemMsg(hwnd, ID_ENTRYFLD,EM_SETTEXTLIMIT,  /* Limit len */
                                 MPFROM2SHORT(2,0),NULL);
         WinSetDlgItemShort(hwnd, ID_ENTRYFLD,(SHORT) cTowerSize,TRUE);
         return 0L;                           /* Allow normal focus setting */

      case WM_COMMAND:
         switch(SHORT1FROMMP(mp1))
         {
            case DID_OK:
               WinQueryDlgItemShort(hwnd, ID_ENTRYFLD,
                                    &sNewSize, TRUE); /* Get the short      */
               if(sNewSize>0 && sNewSize<=MAXDISKCNT) /* Set new Tower size */
               {
                  cTowerSize = (BYTE) sNewSize;
                  WinDismissDlg(hwnd,TRUE);
               }
               else                                   /* Invalid value      */
               {
                   WinMessageBox (HWND_DESKTOP, hwndFrame, "You may only select up to 16 disks",
                                  "OOPS! Too many disks!", ID_MSGBOX, MB_OK |
                                   MB_ICONEXCLAMATION);
               }

               return 0L;

            case DID_CANCEL:
               WinDismissDlg(hwnd,FALSE);
               return 0L;

            default:
               return WinDefDlgProc(hwnd, msg, mp1, mp2);
         }

      default:
         return WinDefDlgProc(hwnd, msg, mp1, mp2);
   }
}

/*
 * Function name: AboutDlgProc()
 *
 * Parameters:  hwnd, msg, mp1, mp2.  Standard PM Dialog Proc params.
 *              No user data is expected in the WM_CREATE.
 *
 * Returns:
 *
 * Purpose: Handles all the messages associated with the About Box
 *
 *
 * Usage/Warnings:
 *
 * Calls:
 */

MRESULT EXPENTRY AboutDlgProc(HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2)
{

   switch(msg)
   {
      case WM_COMMAND:
         WinDismissDlg(hwnd, TRUE);
         break;

      default:
         return WinDefDlgProc(hwnd, msg, mp1, mp2);
   }
}

/*
 * Function name: SetupTowers()
 *
 * Parameters:  None
 *
 * Returns: VOID
 *
 * Purpose: This routine initializes the global arrays that represent the
 *          hanoi stacks.  This involves placing all the disks on the
 *          first peg, emptying the other 2 pegs and setting the associated
 *          counts.
 *
 * Usage/Warnings: Calling uses the global variable cTowerSize to determine
 *                 how many disks there are.
 *
 * Calls:
 */

VOID SetupTowers()
{
   USHORT cnt;

   for(cnt=0;cnt<cTowerSize;cnt++)       /* Setup the intial post with disks */
      abTowers[0][cnt] = cTowerSize-cnt-1;

   abTowersNdx[0] = cTowerSize;          /* Set disk count for initial post  */
   abTowersNdx[1] = abTowersNdx[2] = 0;  /* Zero other post counts           */
}
