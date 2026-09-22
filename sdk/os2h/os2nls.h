/*static char *SCCSID = "@(#)os2nls.h	13.3 90/03/12";*/
/**************************************************\
*
* Module Name: OS2NLS.H
*
* OS/2 Presentation Manager DBCS Window Manager include file
*
* Copyright (c) 1987  Microsoft Corporation
* Copyright (c) 1987  IBM Corporation
*
\**************************************************/

/*
 * definition of Virtual key values for DBCS
 */
#define VK_DBE_FIRST	0x080
#define VK_DBE_LAST	0x0ff

#define VK_DBE_ALPHANUMERIC	0x80 /* VK_DBE_FIRST + 0x00 */
#define VK_DBE_KATAKANA		0x81 /* VK_DBE_FIRST + 0x01 */
#define VK_DBE_HIRAGANA		0x82 /* VK_DBE_FIRST + 0x02 */
#define VK_DBE_SBCSCHAR		0x83 /* VK_DBE_FIRST + 0x03 */
#define VK_DBE_DBCSCHAR		0x84 /* VK_DBE_FIRST + 0x04 */
#define VK_DBE_KANJI		0x85 /* VK_DBE_FIRST + 0x05 */
#define VK_DBE_ROMAN		0x86 /* VK_DBE_FIRST + 0x06 */
#define VK_DBE_CONV		0x87 /* VK_DBE_FIRST + 0x07 */
#define VK_DBE_NOCONV		0x88 /* VK_DBE_FIRST + 0x08 */
#define VK_DBE_TANGO		0x89 /* VK_DBE_FIRST + 0x09 */

/*
 * definition of Keyboard Type for DBCS
 *
 */
/* IBM Japan A type keyboard */
#define KB_JATYPE		6

/* IBM Japan G type keyboard */
#define KB_JGTYPE		7

/*
 * definition of National Language Keyboard ID for DBCS
 *
 */
/* Japan Katakana translation table */
#define LG_JKATAKANA		0x50

/* Japan Hiragana translation table */
#define LG_JHIRAGANA		0x51

/* Japan Alphanumeric translation table */
#define LG_JALPHANUMERIC	0x52
#define LG_JCAPSALPHANUMERIC	0x52

/*
** WinDBCSStdWindowNotify has been removed, since US doesn't need it,
** and current work will remove the need to call it.
*/

/* Double byte character set messages */
#define WM_DBCSFIRST	    0x00b0
#define WM_DBCSLAST	    0x00cf

/* temporary define this message in DBCS range untill DCR is approved */
#define WM_QUERYCONVERTPOS	0x00b0	/* WM_DBCSFIRST */

#define WM_DBE_NOTIFYSETCP	0x00cf	/* WM_DBCEFIRST+0x1f */

/* resource ID offset for bi-lingual system resources (menu & string) */
#define RID_DBE_OFFSET		0x1000
#define STR_DBE_OFFSET		0x1000
