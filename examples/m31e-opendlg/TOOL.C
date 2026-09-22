/*
    TOOL.C -- Contains commonly used routines and globals
   Created by Microsoft, IBM  Corporation 1990
*/

#include "tool.h"


/****************************************************************************\
* This function returns the character following the current one.
\****************************************************************************/

PSZ  FAR PASCAL NextChar (PSZ lpsz)
    { if (*lpsz) return ++lpsz; else return lpsz; }


/****************************************************************************\
* This function returns the character previous to the current one.
\****************************************************************************/

PSZ  FAR PASCAL PrevChar (PSZ lpszStart, PSZ lpszCurrent)
    { if (lpszCurrent > lpszStart) return --lpszCurrent; else return lpszStart; }


/****************************************************************************\
* This function transforms a string to upper case.
\****************************************************************************/

PSZ  FAR PASCAL Upper (PSZ lpsz) {
    PSZ  lpszPtr = lpsz;

    while (*lpszPtr) {
        if (*lpszPtr >= 'a' && *lpszPtr <= 'z') *lpszPtr &= ~0x20;
        lpszPtr++;
        }
    return lpsz;
    }
