/* Emacs style mode select   -*- C++ -*- */
/*----------------------------------------------------------------------------- */
/* */
/* $Id:$ */
/* */
/* Copyright (C) 1993-1996 by id Software, Inc. */
/* */
/* This program is free software; you can redistribute it and/or */
/* modify it under the terms of the GNU General Public License */
/* as published by the Free Software Foundation; either version 2 */
/* of the License, or (at your option) any later version. */
/* */
/* This program is distributed in the hope that it will be useful, */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the */
/* GNU General Public License for more details. */
/* */
/* DESCRIPTION: */
/*	Simple basic typedefs, isolated here to make it easier */
/*	 separating modules. */
/* */
/*----------------------------------------------------------------------------- */


#ifndef __DOOMTYPE__
#define __DOOMTYPE__


#ifndef __BYTEBOOL__
#define __BYTEBOOL__
/* Fixed to use builtin bool type with C++. */
#ifdef __cplusplus
typedef bool boolean;
#else
typedef enum {false, true} boolean;
#endif
typedef unsigned char byte;
#endif


/* Predefined with some OS. */
#ifdef VALUES_H
#include <values.h>
#endif
/*#else */
#define MAXCHAR         ((char)0x7f)
#ifndef MAXSHORT
#define MAXSHORT	((short)0x7fff)
#endif

/* Max pos 32-bit int. */
#ifndef MAXINT
#define MAXINT		((int)0x7fffffff)
#endif
/*#define MAXLONG		((long)0x7fffffff) */
#define MINCHAR         ((char)0x80)
#ifndef MINSHORT
#define MINSHORT	((short)0x8000)
#endif

/* Max negative 32-bit integer. */
#ifndef MININT
#define MININT		((int)0x80000000)
#endif
/*#define MINLONG		((long)0x80000000) */
/*#endif */




#endif
/*----------------------------------------------------------------------------- */
/* */
/* $Log:$ */
/* */
/*----------------------------------------------------------------------------- */
