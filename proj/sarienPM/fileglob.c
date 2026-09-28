/*  Sarien - A Sierra AGI resource interpreter engine
 *  Copyright (C) 1999,2001 Stuart George and Claudio Matsuoka
 *  
 *  $Id: fileglob.c,v 1.2 2001/11/29 05:22:25 cmatsuoka Exp $
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; see docs/COPYING for further details.
 */

#define __32BIT__ 1
#define M_I386 1
#define _MSC_VER 6	

#define INC_VIO
#include <os2.h>

#define FILE_ALL         0x0007  /* Read-only, sys, hidden, & normal     */
#define FILE_INVALID          0  /* The filename was invalid */
#define FILE_PATH             1  /* The filename was a path  */
#define FILE_VALID            2  /* The filename was valid   */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sarien.h"
#include "agi.h"

int file_exists (char *fname)
{
   HDIR        hDir = 1;
   ULONG ulFiles;
   USHORT attr;
   FILEFINDBUF2 ffb;
   int rc;
  
   attr=FILE_ALL;
   DosFindFirst("*.*", (PHDIR)&hDir,attr,\
	 (PFILEFINDBUF2)&ffb, sizeof(FILEFINDBUF2),&ulFiles, FIL_STANDARD);
rc=1;
if(ulFiles>0)
	rc=0;
return(!rc);
//	return !_dos_findfirst (fname, _A_NORMAL | _A_ARCH | _A_RDONLY, &fdata);
}

int file_isthere (char *fname)
{
//	struct find_t fdata;
//	return !_dos_findfirst (fname, _A_NORMAL | _A_ARCH | _A_RDONLY, &fdata);
return(file_exists(fname));
}


char* file_name (char *fname)
{
	int rc;
#if 0
	struct find_t fdata;

	_D ("(\"%s\")", fname);
	fdata.name[0] = 0;
	rc = _dos_findfirst((char*)fname, _A_NORMAL, &fdata);
	while (rc == 0) {
		rc = _dos_findnext(&fdata);
		if(rc == 0) {
			strlwr (fdata.name);
			if (strstr (fdata.name, "dir.")!=NULL)
				rc = 1;
		}
	}

	return strdup (fdata.name);
#else
   HDIR        hDir = 1;
   ULONG ulFiles;
   USHORT attr;
   FILEFINDBUF2 ffb;
   char fnamebuf[15];
   char *p;

attr=FILE_ALL;
   DosFindFirst(fname, (PHDIR)&hDir,attr,\
         (PFILEFINDBUF2)&ffb, sizeof(FILEFINDBUF2),&ulFiles, FIL_STANDARD);
   if(ulFiles)
   {
p=ffb.achName;p+=2;
sprintf(fnamebuf,p);
   }
else
   memset(fnamebuf,0x0,sizeof(fnamebuf));

	return strdup(fnamebuf);
#endif
}



char *fixpath (int flag, char *fname)
{
	static char path[MAX_PATH];
	char *p;

    	strcpy (path, game.dir);

	if (*path && (path[strlen(path)-1]!='\\' && path[strlen(path)-1] != '/'))
	{
		if(path[strlen(path)-1]==':')
			strcat(path, "./");
		else
			strcat(path, "/");
	}

	if (flag==1)
		strcat (path, game.name);

	strcat (path, fname);

	p = path;

	while(*p) {
		if (*p=='\\')
		    *p='/';
		p++;
	}

	return path;
}

