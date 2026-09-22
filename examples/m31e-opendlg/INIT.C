/***************************************************************************\
*
* Sample file open, save, saveas dialog functions for PM
*
*  Created by Microsoft, IBM  Corporation 1990
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

#include "tool.h"
/****************************************************************************\
* This function initializes the file dialog library (by loading strings).
*
* Note: Initialization will fail if CCHSTRINGSMAX is smaller than the
*       space taken up by all strings in the .rc file.  Fix by increasing
*       CCHSTRINGSMAX in wintool.h and maybe also the initial heap size
*       in wintool.def.
*
* Returns:
*   TRUE if initialization successful
*   FALSE otherwise
\***************************************************************************/

ULONG EXPENTRY InitLibrary(hPDLL, hmod)
ULONG	hPDLL;
HMODULE	hmod;
{
    int i;
    int cch;
    PSTR pch;
    PSTR pmem;
    int cchRemaining;

    vhModule = hmod;

    /* allocate memory for strings */
    if (DosAllocMem((PPVOID)&pmem,cchRemaining=CCHSTRINGSMAX,fPERM|PAG_COMMIT))
        return FALSE;
    pch = pmem;

    /* load strings from resource file */
    for (i = 0; i < CSTRINGS; i++) {
        cch = 1 + WinLoadString(HABX, vhModule, i, cchRemaining, (PSZ)pch);
        if (cch < 2)
            /* loadstring failed */
            return FALSE;
        vrgsz[i] = pch;
        pch += cch;

        if ((cchRemaining -= cch) <= 0)
            /* ran out of space */
            return FALSE;
    }
    hPDLL;  /* to suppress warnings */
    return TRUE;
}
