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
/*	Fixed point arithemtics, implementation. */
/* */
/*----------------------------------------------------------------------------- */


#ifndef __M_FIXED__
#define __M_FIXED__


#ifdef __GNUG__
#pragma interface
#endif


/* */
/* Fixed point, 32bit as 16.16. */
/* */
#define FRACBITS                16
#define FRACUNIT                (1<<FRACBITS)

typedef int fixed_t;

fixed_t FixedMul        (fixed_t a, fixed_t b);
fixed_t FixedDiv2       (fixed_t a, fixed_t b);

#ifdef USE_ASM
inline static const fixed_t FixedDiv        (fixed_t a, fixed_t b);

/* limits.h */
#define INT_MAX 2147483647

/*
updated code from prboom
https://svn.prboom.org/repos/tags/stable_wo_net/prboom2/src/m_fixed.h
*/

#  define D_abs(x) ({fixed_t _t = (x), _s = _t >> (8*sizeof _t-1); (_t^_s)-_s;})

inline static const fixed_t FixedDiv(fixed_t a, fixed_t b)
{
  if (D_abs(a) >> 14 < D_abs(b))
    {
/*
      fixed_t result;

      int dummy;
      asm(" idivl %4 ;"
	  : "=a" (result),
	    "=d" (dummy)     cphipps - fix compile problems with gcc 2.95.1
			     edx is clobbered, but also an input
	  : "0" (a<<16),
	    "1" (a>>16),
	    "r" (b)
	  : "%cc"
	  );
      return result;
*/
	return(FixedDivOLD(a,b));
    }
  return ((a^b)>>31) ^ INT_MAX;
}

#else
fixed_t FixedDiv ( fixed_t a, fixed_t b);
#endif	/*USE_ASM*/


#ifdef __WATCOMC__
#endif




#endif
/*----------------------------------------------------------------------------- */
/* */
/* $Log:$ */
/* */
/*----------------------------------------------------------------------------- */
