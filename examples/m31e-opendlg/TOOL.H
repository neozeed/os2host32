/*
    TOOL.H -- Contains common definitions
    Created by Microsoft Corporation, 1989
*/

#define INCL_WIN
#include <os2.h>

#include "opendlg.h"
#include "dialog.h"	/* dialog box id's and constants */

#define HABX		0L
#define CSTRINGS	7
#define CCHSTRINGSMAX	512 /* The total length of all loaded strings
                                must be less than or equal to this.
                                If this is exceeded, ToolInit() will fail.
                                Ifthis should occur, CCHSTRINGSMAX should
                                be increased as necessary and the initial 
                                heap size in WINTOOL.DEF should also
                                be increased accordingly.
                             */

/* Instead of including STDLIB.H */
#define min(a,b)  (((a) < (b)) ? (a) : (b))
#define max(a,b)  (((a) > (b)) ? (a) : (b))

#ifndef RC_INVOKED
#include "declare.h"
#endif
